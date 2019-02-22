#include <vector>
#include <iomanip>
#include <sstream>
#include <windows.h>
#define KDEXT_64BIT
#include <wdbgexts.h>
#include "common.h"

namespace Memory {
  class SmallNormalHeapBucketT : public debug_object {
  public:
    virtual void load(address_t addr) {
      base_ = addr;
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_);
      s << s1 << std::endl;
    }
  };

  const char *HeapBucketGroupTypeNames[] = {
    "Memory::HeapBucketGroup<SmallAllocationBlockAttributes>",
    "Memory::HeapBucketGroup<MediumAllocationBlockAttributes>",
  };

  template <int TypeNameIndex>
  class HeapBucketGroup : public debug_object {
    address_t heapBucket{},
              leafHeapBucket{},
              finalizableHeapBucket{};

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      heapBucket = addr + get_field_offset(
        HeapBucketGroupTypeNames[TypeNameIndex], "heapBucket");
      leafHeapBucket = addr + get_field_offset(
        HeapBucketGroupTypeNames[TypeNameIndex], "leafHeapBucket");
      finalizableHeapBucket = addr + get_field_offset(
        HeapBucketGroupTypeNames[TypeNameIndex], "finalizableHeapBucket");
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(heapBucket),
                     s2(leafHeapBucket),
                     s3(finalizableHeapBucket);
      s << "normal " << s1 << " leaf " << s2 << " finalized " << s3 << std::endl;
    }
  };

  class LargeHeapBucket : public debug_object {

  public:
    virtual void load(address_t addr) {
      base_ = addr;
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_);
      s << s1 << std::endl;
    }
  };

  class HeapInfo : public debug_object {
    address_t recycler{};
    std::vector<HeapBucketGroup<0>> heapBuckets;
    std::vector<HeapBucketGroup<1>> mediumHeapBuckets;
    LargeHeapBucket largeObjectBucket;

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      recycler = load_pointer(addr + get_field_offset("Memory::HeapInfo", "recycler"));

      auto field_array = get_field_info("Memory::HeapInfo", "heapBuckets"),
           field_item = get_field_info("Memory::HeapInfo", "heapBuckets[0]");
      auto num_items = field_array.size / field_item.size;
      heapBuckets.resize(num_items);
      addr = base_ + field_array.FieldOffset;
      for (size_t i = 0; i < heapBuckets.size(); ++i) {
        heapBuckets[i].load(addr);
        addr += field_item.size;
      }

      field_array = get_field_info("Memory::HeapInfo", "mediumHeapBuckets");
      field_item = get_field_info("Memory::HeapInfo", "mediumHeapBuckets[0]");
      num_items = field_array.size / field_item.size;
      mediumHeapBuckets.resize(num_items);
      addr = base_ + field_array.FieldOffset;
      for (size_t i = 0; i < mediumHeapBuckets.size(); ++i) {
        mediumHeapBuckets[i].load(addr);
        addr += field_item.size;
      }

      largeObjectBucket.load(
        addr + get_field_offset("Memory::HeapInfo", "largeObjectBucket"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_);
      s << s1 << std::endl
        << "heapBuckets:" << std::endl;
      for (size_t i = 0; i < heapBuckets.size(); ++i) {
        s << std::setfill(' ') << std::setw(4) << i << ' ';
        heapBuckets[i].dump(s);
      }
      s << "mediumHeapBuckets:" << std::endl;
      for (size_t i = 0; i < mediumHeapBuckets.size(); ++i) {
        s << std::setfill(' ') << std::setw(4) << i << ' ';
        mediumHeapBuckets[i].dump(s);
      }
    }

    address_t GetRecycler() const {
      return recycler;
    }
  };

  class HeapInfoManager : public debug_object {
    HeapInfo defaultHeap;

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      defaultHeap.load(
        addr + get_field_offset("Memory::HeapInfoManager", "defaultHeap"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_),
                     s2(defaultHeap.GetRecycler());
      s << "Memory::Recycler "
        << s2 << std::endl
        << "Memory::HeapInfoManager "
        << s1 << std::endl
        << "defaultHeap Memory::HeapInfo ";
      defaultHeap.dump(s);
    }

    void dump_oneline(std::ostream &s) const {
      address_string s1(base_);
      s << s1 << std::endl;
    }
  };

  class Recycler : public debug_object {
    HeapInfoManager autoHeap;

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      autoHeap.load(addr + get_field_offset("Memory::Recycler", "autoHeap"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_);
      s << "Memory::Recycler "
        << s1 << std::endl
        << "autoHeap Memory::HeapInfoManager ";
      autoHeap.dump_oneline(s);
    }
  };
}

DECLARE_API(heap) {
  const auto vargs = get_args(args);
  if (vargs.size() < 2) return;

  const auto addr = GetExpression(vargs[1].c_str());
  if (vargs[0] == "-r") {
    dump_object<Memory::Recycler>(addr);
  }
  else if (vargs[0] == "-m") {
    dump_object<Memory::HeapInfoManager>(addr);
  }
}
