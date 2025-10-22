#pragma once

#include "Common.h"

class ThreadCache
{
private:
    FreeList _freeLists[FREE_LIST_NUM];
public:

    // 线程申请size大小空间
    void* Allocate(size_t size);
    
    // 回收size大小空间(size为对齐后的空间)
    void Deallocate(void* ptr, size_t size);

    // TC中空间不够，往CC中申请空间
    void* FromCentoalCache(size_t index, size_t size);

    // TC向CC归还空间，从哪个桶申请的就还到对应桶的自由链表下
    void ListToLong(FreeList& list, size_t size);
};

// TLS(线程局部存储)的全局变量
// 这样每个线程都有一个自己独立的全局变量
static thread_local ThreadCache* pThreadCache = nullptr;