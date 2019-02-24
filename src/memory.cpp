#include <vector>
#include <iomanip>
#include <sstream>
#include <windows.h>
#define KDEXT_64BIT
#include <wdbgexts.h>
#include "common.h"
#include "heapconstants.h"

namespace Memory {
  class HeapBlock : public debug_object {
    enum HeapBlockType : uint8_t
    {
        FreeBlockType = 0,                  // Only used in HeapBlockMap.  Actual HeapBlock structures should never have this.
        SmallNormalBlockType,
        SmallLeafBlockType,
        SmallFinalizableBlockType,
#ifdef RECYCLER_WRITE_BARRIER
        SmallNormalBlockWithBarrierType,
        SmallFinalizableBlockWithBarrierType,
#endif
#ifdef RECYCLER_VISITED_HOST
        SmallRecyclerVisitedHostBlockType,
#endif
        MediumNormalBlockType,
        MediumLeafBlockType,
        MediumFinalizableBlockType,
#ifdef RECYCLER_WRITE_BARRIER
        MediumNormalBlockWithBarrierType,
        MediumFinalizableBlockWithBarrierType,
#endif
#ifdef RECYCLER_VISITED_HOST
        MediumRecyclerVisitedHostBlockType,
#endif
        LargeBlockType,

#ifdef RECYCLER_VISITED_HOST
        SmallAllocBlockTypeCount = 7, // Actual number of types for blocks containing small allocations
#else
        SmallAllocBlockTypeCount = 6,
#endif

#ifdef RECYCLER_VISITED_HOST
        MediumAllocBlockTypeCount = 6, // Actual number of types for blocks containing medium allocations
#else
        MediumAllocBlockTypeCount = 5,
#endif

        SmallBlockTypeCount = SmallAllocBlockTypeCount + MediumAllocBlockTypeCount,      // Distinct block types independent of allocation size using SmallHeapBlockT
        LargeBlockTypeCount = 1, // There is only one LargeBlockType

        BlockTypeCount = SmallBlockTypeCount + LargeBlockTypeCount,
    };

  protected:
    address_t address{},
              segment{};
    HeapBlockType heapBlockType;

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      address = load_pointer(
        addr + get_field_offset("Memory::HeapBlock", "address"));
      segment = load_pointer(
        addr + get_field_offset("Memory::HeapBlock", "segment"));
      heapBlockType = load_data<HeapBlockType>(
        addr + get_field_offset("Memory::HeapBlock", "heapBlockType"));
    }
  };

  template <class TBlockAttributes>
  class SmallHeapBlockT : public HeapBlock {
    const std::string full_type = std::string("Memory::SmallHeapBlockT<")
      + TBlockAttributes::type()
      + '>';

    address_t next{},
              freeObjectList{},
              lastFreeObjectHead{},
              heapBucket{};
    uint32_t bucketIndex{};
    uint16_t objectSize{},
             objectCount{},
             freeCount{},
             lastFreeCount{},
             markCount;

  public:
    virtual void load(address_t addr) {
      HeapBlock::load(addr);
      next = load_pointer(
        addr + get_field_offset(full_type.c_str(), "next"));
      lastFreeObjectHead = load_pointer(
        addr + get_field_offset(full_type.c_str(), "lastFreeObjectHead"));
      heapBucket = load_pointer(
        addr + get_field_offset(full_type.c_str(), "heapBucket"));
      bucketIndex = load_data<uint32_t>(
        addr + get_field_offset(full_type.c_str(), "bucketIndex"));
      objectSize = load_data<uint16_t>(
        addr + get_field_offset(full_type.c_str(), "objectSize"));
      objectCount = load_data<uint16_t>(
        addr + get_field_offset(full_type.c_str(), "objectCount"));
      freeCount = load_data<uint16_t>(
        addr + get_field_offset(full_type.c_str(), "freeCount"));
      lastFreeCount = load_data<uint16_t>(
        addr + get_field_offset(full_type.c_str(), "lastFreeCount"));
      markCount = load_data<uint16_t>(
        addr + get_field_offset(full_type.c_str(), "markCount"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_),
                     s2(heapBucket),
                     s3(address);
      s << s1
        << std::setfill(' ') << std::setw(3) << bucketIndex
        << " (" << std::setfill(' ') << std::setw(3) << objectSize << ") " << s2
        << ' ' << s3
        << std::endl;
    }

    void dump_for_allocator(std::ostream &s) const {
      address_string s1(base_),
                     s2(address);
      s << s1
        << " : Bucket#" << bucketIndex
        << ' ' << s2;
    }

    address_t GetNextBlock() const { return next; }
  };

  template <class TBlockAttributes>
  class SmallNormalHeapBlockT : public SmallHeapBlockT<TBlockAttributes> {
  public:
    static std::string type() {
      return std::string("Memory::SmallNormalHeapBlockT<") + TBlockAttributes::type() + '>';
    }
  };

  typedef SmallNormalHeapBlockT<SmallAllocationBlockAttributes> SmallNormalHeapBlock;
  typedef SmallNormalHeapBlockT<MediumAllocationBlockAttributes> MediumNormalHeapBlock;

  template <typename TBlockType>
  class SmallHeapBlockAllocator : public debug_object {
    const std::string full_type = std::string("Memory::SmallHeapBlockAllocator<")
      + TBlockType::type()
      + " >";

    address_t endAddress{},
              freeObjectList{};
    TBlockType heapBlock;
    address_t prev{},
              next{};

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      endAddress = load_pointer(
        addr + get_field_offset(full_type.c_str(), "endAddress"));
      freeObjectList = load_pointer(
        addr + get_field_offset(full_type.c_str(), "freeObjectList"));
      heapBlock.load(load_pointer(
        addr + get_field_offset(full_type.c_str(), "heapBlock")));
      prev = load_pointer(
        addr + get_field_offset(full_type.c_str(), "prev"));
      next = load_pointer(
        addr + get_field_offset(full_type.c_str(), "next"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(endAddress),
                     s2(freeObjectList);
      s << s2 << '-' << s1 << " Block{ ";
      heapBlock.dump_for_allocator(s);
      s << " }";
    }

    address_t GetNext() const { return next; }
  };

  class HeapBucket : public debug_object {
    address_t heapInfo{};
    uint32_t sizeCat{};

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      heapInfo = load_pointer(
        addr + get_field_offset("Memory::HeapBucket", "heapInfo"));
      sizeCat = load_data<uint32_t>(
        addr + get_field_offset("Memory::HeapBucket", "sizeCat"));
    }

    virtual void HeapBucket::dump(std::ostream &s) const;
  };

  template <typename TBlockType>
  class HeapBucketT : public HeapBucket {
    typedef SmallHeapBlockAllocator<TBlockType> TBlockAllocatorType;

    const std::string full_type = std::string("Memory::HeapBucketT<")
      + TBlockType::type()
      + " >";

    address_t allocatorHead,
              nextAllocableBlockHead{},
              emptyBlockList{},
              fullBlockList{},
              heapBlockList{},
              allocableHeapBlockListHead{},
              lastKnownNextAllocableBlockHead{},
              sweepableHeapBlockList{},
              explicitFreeList{},
              lastExplicitFreeListAllocator{};

  public:
    virtual void load(address_t addr) {
      HeapBucket::load(addr);
      allocatorHead = addr + get_field_offset(full_type.c_str(), "allocatorHead");
    }

    virtual void dump(std::ostream &s) const {
      HeapBucket::dump(s);
      s << "allocatorHead:" << std::endl;
      for (address_t p = allocatorHead; ; ) {
        TBlockAllocatorType allocator;
        allocator.load(p);
        s << ' ';
        allocator.dump(s);
        p = allocator.GetNext();
        if (p == allocatorHead) break;
      }
    }
  };

  template <typename TBlockType>
  class SmallNormalHeapBucketBase : public HeapBucketT<TBlockType>
  {};

  template <typename TBlockAttributes>
  class SmallNormalHeapBucketT
    : public SmallNormalHeapBucketBase<SmallNormalHeapBlockT<TBlockAttributes>>
  {};

  template <class TBlockAttributes>
  class HeapBucketGroup : public debug_object {
    const std::string full_type = std::string("Memory::HeapBucketGroup<")
      + TBlockAttributes::type()
      + '>';

    address_t heapBucket{},
              leafHeapBucket{},
              finalizableHeapBucket{};

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      heapBucket = addr + get_field_offset(
        full_type.c_str(), "heapBucket");
      leafHeapBucket = addr + get_field_offset(
        full_type.c_str(), "leafHeapBucket");
      finalizableHeapBucket = addr + get_field_offset(
        full_type.c_str(), "finalizableHeapBucket");
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(heapBucket),
                     s2(leafHeapBucket),
                     s3(finalizableHeapBucket);
      s << "normal " << s1 << " leaf " << s2 << " finalized " << s3 << std::endl;
    }
  };

  class HeapInfo : public debug_object {
    address_t recycler{},
              newNormalHeapBlockList{};
    std::vector<HeapBucketGroup<SmallAllocationBlockAttributes>> heapBuckets;
    //std::vector<HeapBucketGroup<MediumAllocationBlockAttributes>> mediumHeapBuckets;
    //LargeHeapBucket largeObjectBucket;

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      newNormalHeapBlockList = load_pointer(
        addr + get_field_offset("Memory::HeapInfo", "newNormalHeapBlockList"));
      recycler = load_pointer(
        addr + get_field_offset("Memory::HeapInfo", "recycler"));

      auto field_array = get_field_info("Memory::HeapInfo", "heapBuckets"),
           field_item = get_field_info("Memory::HeapInfo", "heapBuckets[0]");
      auto num_items = field_array.size / field_item.size;
      heapBuckets.resize(num_items);
      addr = base_ + field_array.FieldOffset;
      for (size_t i = 0; i < heapBuckets.size(); ++i) {
        heapBuckets[i].load(addr);
        addr += field_item.size;
      }
#if 0
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
#endif
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_);
      s << s1 << std::endl;

      s << "newNormalHeapBlockList:" << std::endl;
      SmallNormalHeapBlock block;
      int cnt = 0;
      for (address_t p = newNormalHeapBlockList; p; p = block.GetNextBlock()) {
        s << std::setfill(' ') << std::setw(4) << cnt++ << ' ';
        block.load(p);
        block.dump(s);
      }

      s << "heapBuckets:" << std::endl;
      for (size_t i = 0; i < heapBuckets.size(); ++i) {
        s << std::setfill(' ') << std::setw(4) << i << ' ';
        heapBuckets[i].dump(s);
      }
#if 0
      s << "mediumHeapBuckets:" << std::endl;
      for (size_t i = 0; i < mediumHeapBuckets.size(); ++i) {
        s << std::setfill(' ') << std::setw(4) << i << ' ';
        mediumHeapBuckets[i].dump(s);
      }
#endif
    }

    address_t GetRecycler() const {
      return recycler;
    }
  };

  void HeapBucket::dump(std::ostream &s) const {
    HeapInfo heapInfo_loaded;
    heapInfo_loaded.load(heapInfo);
    address_string s1(base_),
                   s2(heapInfo),
                   s3(heapInfo_loaded.GetRecycler());
    s << "Bucket " << s1
      << " (" << sizeCat
      << ") HeapInfo " << s2
      << " Recycler " << s3
      << std::endl;
  }

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
      s << "Memory::HeapInfoManager "
        << s1 << std::endl
        << "defaultHeap Memory::HeapInfo ";
      defaultHeap.dump(s);
    }

    const HeapInfo &GetDefaultHeap() const {
      return defaultHeap;
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
        << "autoHeap ";
      autoHeap.dump(s);
    }
  };
}

DECLARE_API(heap) {
  const auto vargs = get_args(args);
  if (vargs.size() < 2) return;
  auto addr = GetExpression(vargs[1].c_str());
  if (vargs[0] == "-m") {
    Memory::HeapInfoManager manager;
    manager.load(addr);
    addr = manager.GetDefaultHeap().GetRecycler();
    goto Recycler;
  }
  else if (vargs[0] == "-r") {
  Recycler:
    dump_object<Memory::Recycler>(addr);
  }
}

DECLARE_API(bucket) {
  const auto vargs = get_args(args);
  if (vargs.size() < 2) return;
  const auto addr = GetExpression(vargs[1].c_str());
  if (vargs[0] == "-sn") {
    dump_object<Memory::SmallNormalHeapBucketT<SmallAllocationBlockAttributes>>(addr);
  }
}