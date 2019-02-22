#include <unordered_map>
#include <windows.h>
#define KDEXT_64BIT
#include <wdbgexts.h>
#include "common.h"

void Log(const char* format, ...);

class symbol_manager {
  const std::string module_;
  std::unordered_map<std::string, uint32_t> offset_map_;
  std::unordered_map<std::string, FIELD_INFO> field_info_map_;

public:
  symbol_manager(const char *module_name)
    : module_(module_name)
  {}

  uint32_t get_field_offset(const char *type, const char *field) {
    const auto full_symbol_name = module_ + '!' + type + '.' + field;
    auto found = offset_map_.find(full_symbol_name);
    if (found != offset_map_.end()) {
      return found->second;
    }

    ULONG offset = 0xffffffff;
    uint32_t status = GetFieldOffset(type, field, &offset);
    // Regardless of the result, we update the map to block subsequent attempts
    offset_map_[full_symbol_name] = offset;
    if (status != 0) {
      Log("GetFieldOffset(%s) failed with %08x\n",
          full_symbol_name.c_str(),
          status);
    }
    return offset;
  }

  FIELD_INFO get_field_info(const char *type, const char *field) {
    const auto full_type_name = module_ + '!' + type;
    const auto full_symbol_name = full_type_name + '.' + field;
    auto found = field_info_map_.find(full_symbol_name);
    if (found != field_info_map_.end()) {
      return found->second;
    }

    FIELD_INFO flds = {
      (PUCHAR)field,
      (PUCHAR)"",
      0,
      DBG_DUMP_FIELD_FULL_NAME | DBG_DUMP_FIELD_RETURN_ADDRESS,
      0,
      nullptr
    };
    SYM_DUMP_PARAM sym = {
      sizeof (SYM_DUMP_PARAM),
      (PUCHAR)full_type_name.c_str(),
      DBG_DUMP_NO_PRINT,
      0,
      nullptr,
      nullptr,
      nullptr,
      1,
      &flds
    };
    sym.nFields = 1;
    auto status = Ioctl(IG_DUMP_SYMBOL_INFO, &sym, sym.size);
    // Regardless of the result, we update the map to block subsequent attempts
    field_info_map_[full_symbol_name] = flds;
    if (status != 0) {
      Log("GetFieldInfo(%s) failed with %08x\n",
          full_symbol_name.c_str(),
          status);
    }
    return flds;
  }

  void dump_all() const {
    Log("offset_map_\n");
    for (const auto &pair : offset_map_) {
      Log("%s: %08x\n", pair.first.c_str(), pair.second);
    }
    Log("field_info_map_\n");
    for (const auto &pair : field_info_map_) {
      Log("%s: +%08x %d\n",
          pair.first.c_str(),
          pair.second.FieldOffset,
          pair.second.size);
    }
  }
};

namespace {
  symbol_manager smanager("chakracore");
}

uint32_t get_field_offset(const char *type, const char *field) {
  return smanager.get_field_offset(type, field);
}

FIELD_INFO get_field_info(const char *type, const char *field) {
  return smanager.get_field_info(type, field);
}

void dump_symbol_manager() {
  smanager.dump_all();
}