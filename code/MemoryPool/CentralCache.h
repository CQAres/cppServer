#pragma once
#include "Common.h"

class CentralCache
{
private:
    SpanList _spanLists[FREE_LIST_NUM];

    // 饿汉模式创建一个CentralCache对象
    static CentralCache _Inst;

    CentralCache(){}
    CentralCache(const CentralCache& copy) = delete;
    CentralCache& operator = (const CentralCache& copy) = delete;
public:

    static CentralCache* GetInstance()
    {
        return &_Inst;
    }

    /*从CC缓存获取一定数量的内存空间块给ThreadCache
    start和end表示CC提供空间的开始和结尾
    batchNum表示TC需要多少块size大小的内存块
    返回CC实际提供的空间大小*/
    size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

    // 从桶中找到一个管理空间非空的span
    // params1: 桶空间对应的span的链表
    // params2: 对齐后的块内存大小
    Span* GetOneSpan(SpanList& list, size_t size);

    // 将TC归还的块放入Span中
    void ReleaseListToSpans(void* start, size_t size);

};