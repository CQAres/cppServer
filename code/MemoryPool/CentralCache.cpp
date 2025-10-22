#include "CentralCache.h"
#include "PageCache.h"

// CentralCache饿汉对象
CentralCache CentralCache::_Inst;

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
    Span* it = list.Begin();
    // 遍历查找链表中非空的span
    while (it != list.End())
    {
        if(it->_freeList != nullptr)return it;
        else it = it->_next;
    }
    // 解掉桶锁，让其他线程向该桶申请资源时能拿到锁
    list._mtx.unlock();

    // 走到这表示没有非空的span，因此需要往PC中申请
    // 将申请的size转换成匹配的页数
    size_t k = SizeClass::NumMovePage(size);

    // 整个PC只有一把锁，在调用PC的时候加锁
    PageCache::getInstance()->_pageMtx.lock();
    // 向PC申请一个全新的k页的span
    Span* span = PageCache::getInstance()->NewSpan(k);
    span->_isUse = true;

    PageCache::getInstance()->_pageMtx.unlock();

    // 这里从PC申请到span后，CC会自己对span对切分，分成固定大小的块

    /* 因为pageID是unsigned long long类型，不能直接赋予给指针，因此需要强转
       由页号*页大小可以得到这个span所管理空间的首地址，
       start + 页数*页大小可以得到span所管理空间的尾地址 */
    char* start = (char*)(span->_pageID << PAGE_SHIFT);
    char* end = (char*)(start + (span->n << PAGE_SHIFT));

    span->_objSize = size;

    // 开始切分span
    span->_freeList = start;

    void* tail = start;
    start += size;

    // 将span分成n个块，并将n个块以链表形式连接起来
    while (start < end)
    {
        ObjNext(tail) = start;
        start += size;
        tail = ObjNext(tail);
    }
    ObjNext(tail) = nullptr;

    // 切好span后，需要把span挂在桶下，因此先把桶锁起来
    list._mtx.lock();
    // 切好span后，将span挂载到对应桶下
    list.PushFront(span);
    
    return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
    // 获得桶的索引
    size_t index = SizeClass::Index(size);

    // 给桶上锁
    _spanLists[index]._mtx.lock();

    // 从桶中找到一个管理空间非空的span
    Span* span = GetOneSpan(_spanLists[index], size);
    assert(span);
    assert(span->_freeList);
    
    // 让头尾指针都指向span的空闲链表，让end不断向后走
    start = end = span->_freeList;
    size_t actualNum = 1;

    // 在end没走到最后一块的前提下，让end走batchNum - 1步，即总长度为batchNum块
    size_t i = 0;
    while (i < batchNum - 1 && ObjNext(end) != nullptr)
    {
        end = ObjNext(end);
        ++i;
        ++actualNum;
    }

    // 将end后的块接回到span的空闲链表中，并将end后续设为空
    span->_freeList = ObjNext(end);
    span->use_count += actualNum;
    ObjNext(end) = nullptr;
    
    // 解锁
    _spanLists[index]._mtx.unlock();

    return actualNum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
    // 通过块大小找到对应的桶
    size_t index = SizeClass::Index(size);

    // 上桶锁
    _spanLists[index]._mtx.lock();

    // 便利回收链表中的每个块，将对应的块都放回对应页的span所管理的自由链表下
    while (start)
    {
        void* next = ObjNext(start);

        Span* span = PageCache::getInstance()->MapObjectToSpan(start);

        ObjNext(start) = span->_freeList;
        span->_freeList = start;

        span->use_count--;

        // 如果这个span管理的所有块都还回来了，那将span还给PC管理
        if(span->use_count == 0)
        {
            // 先从桶中移除span
            _spanLists[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            _spanLists[index]._mtx.unlock();

            // 回收span
            PageCache::getInstance()->_pageMtx.lock();
            PageCache::getInstance()->ReleaseSpanToPageCache(span);
            PageCache::getInstance()->_pageMtx.unlock();

            _spanLists[index]._mtx.lock();
        }

        start = next;
    }
    
    // 解锁
    _spanLists[index]._mtx.unlock();
}