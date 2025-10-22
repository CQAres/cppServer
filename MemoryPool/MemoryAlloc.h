#pragma once
#include "ThreadCache.h"
#include "PageCache.h"

static void* MemoryAlloc(size_t size)
{
    // 单次申请超过256KB，则按页对齐给分配内存
    if (size > THREADCACHE_MAX_BYTES)
    {
        size_t alignSize = SizeClass::RoundUp(size);
        size_t k = alignSize >> PAGE_SHIFT;

        // 直接向PC要
        PageCache::getInstance()->_pageMtx.lock();
        Span* span = PageCache::getInstance()->NewSpan(k);
        span->_objSize = size;
        PageCache::getInstance()->_pageMtx.unlock();

        // 通过获得的span来提供空间
        void* ptr = (void*)(span->_pageID >> PAGE_SHIFT);
        return ptr;
    }
    else
    {
        // 因为pThreadCache每个线程只有一个且相互独立，因此不存在竞争，直接判断就可以创建
        // 这样相当于每个线程都拥有自己的TC
        if (pThreadCache == nullptr)
        {
            
            pThreadCache = new ThreadCache;
        }

        cout << std::this_thread::get_id() << " " << pThreadCache << endl;
    }

    return pThreadCache->Allocate(size);
}

static void MemoryFree(void* ptr)
{
    // 确保回收的地址不为空
    assert(ptr);

    // 通过地址找到对应的span
    Span* span = PageCache::getInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize; // 通过映射来的span获取ptr所指空间大小

	// 通过size判断是不是大于256KB的，是了就走pc
	if (size > THREADCACHE_MAX_BYTES)
	{
		PageCache::getInstance()->_pageMtx.lock(); // 记得加锁解锁
		PageCache::getInstance()->ReleaseSpanToPageCache(span); // 直接通过span释放空间
		PageCache::getInstance()->_pageMtx.unlock(); // 记得加锁解锁
	}
	else // 不是大于256KB的就走tc
	{
		pThreadCache->Deallocate(ptr, size);
	}
}