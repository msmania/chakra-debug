#include <unordered_map>
#include <windows.h>
#define KDEXT_64BIT
#include <wdbgexts.h>
#include "common.h"

void Log(const char* format, ...);

class symbol_manager {
  const std::string module_;
  std::unordered_map<std::string, uint32_t> map_;

public:
  symbol_manager(const char *module_name)
    : module_(module_name)
  {}

  uint32_t get_field_offset(const std::string &type, const std::string &field) {
    const auto full_symbol_name = module_ + '!' + type + '.' + field;
    auto found = map_.find(full_symbol_name);
    if (found != map_.end()) {
      return found->second;
    }
    else {
      ULONG offset = 0xffffffff;
      uint32_t status = GetFieldOffset(type.c_str(), field.c_str(), &offset);
      if (status == 0)
        map_[full_symbol_name] = offset;
      else
        Log("GetFieldOffset(%s) failed with %08x\n",
            full_symbol_name.c_str(),
            status);
      return offset;
    }
  }

  void dump_all() const {
    for (const auto &pair : map_) {
      Log("%s: %08x\n", pair.first.c_str(), pair.second);
    }
  }
};

namespace {
  symbol_manager smanager("chakracore");
}

uint32_t get_field_offset(const std::string &type, const std::string &field) {
  return smanager.get_field_offset(type, field);
}

void dump_symbol_manager() {
  smanager.dump_all();
}