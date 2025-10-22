#pragma once
#include "Common.h"

class PageCache
{
private:
    // PC中的哈希桶
    SpanList _spanLists[PAGE_NUM];

    PageCache(){}
    PageCache(const PageCache& pc) = delete;
    PageCache& operator = (const PageCache& pc) = delete;

    // PC单例类对象
    static PageCache _sInst;

    // 使用哈希表根据页号快速找到对应的Span
    std::unordered_map<PageID, Span*> _idSpanMap;

public:
    static PageCache* getInstance()
    {
        return &_sInst;
    }

    // PC锁
    std::mutex _pageMtx;

    // PC从_SpanList中获取一个k页的Span
    Span* NewSpan(size_t k);

    // 通过块的地址找到对应的Span
    Span* MapObjectToSpan(void* addr);

    /* 向左向右重复合并相邻的span页，3种情况停止
    1、没有相邻页
    2、相邻页在CC中使用
    3、相邻页与当前span合并后超过128页（最大）
    */
    void ReleaseSpanToPageCache(Span* span);
};