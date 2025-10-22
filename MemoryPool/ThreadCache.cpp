#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size)
{
    assert(size <= THREADCACHE_MAX_BYTES);

    // 对齐
    size_t alignSize = SizeClass::RoundUp(size);
    size_t index = SizeClass::Index(alignSize);

    if(!_freeLists[index].Empty())
    {
        // 自由链表不为空，直接从自由链表中申请空间
        return _freeLists[index].Pop();
    }
    else
    {
        // 自由链表为空，要向CC申请空间
        return FromCentoalCache(index, alignSize);
    }

}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
    // 确保回收空间不为空且小于等于256KB
    assert(ptr);
    assert(size <= THREADCACHE_MAX_BYTES);

    size_t index = SizeClass::Index(size);
    _freeLists[index].Push(ptr);
}

// params1: 桶号
// params2: 申请内存对齐后的大小
void* ThreadCache::FromCentoalCache(size_t index, size_t size)
{
    /*MaxSize表示index位置的自由链表单次申请未到上限时，能够申请的最大空间块
    NumMoveSize表示TC向CC申请size大小的空间块最多申请多少块*/
    size_t batchNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));

    // 如果没有打到上限，下次分配的时候多给一块，开始反馈调节
    if(batchNum == _freeLists[index].MaxSize())
    {
        _freeLists[index].MaxSize()++;
    }

    void* start = nullptr;
    void* end = nullptr;

    // 实际获取的块数
    size_t realNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);

    assert(realNum >= 1);

    if(realNum == 1)
    {
        // 只有一块，直接返回
        assert(start == end);
        return start;
    }
    else
    {
        // 有多块，将多余的块放入自由链表
        _freeLists[index].PushRange(ObjNext(start), end, realNum - 1);

        // 给线程返回start所指的空间
        return start;
    }

}

void ThreadCache::ListToLong(FreeList& list, size_t size)
{
    void* start = nullptr;
    void* end = nullptr;

    // 归还内存块数 = 当前块数 - 单次能够申请的最多块数
    list.PopRange(start, end, list.Size() - list.MaxSize());

    // 归还空间给span
    CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}