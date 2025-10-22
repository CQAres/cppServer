#include "PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k)
{
    assert(k > 0);

    // 如果单次申请的页数超过128页时才需要向os申请，如果没有超过128页的还可以向pc申请
	if (k > PAGE_NUM - 1) 
	{
		void* ptr = malloc(k * 8192); // 直接向os申请
		Span* span = new Span;
		
		span->_pageID = ((PageID)ptr >> PAGE_SHIFT); // 申请空间的对应页号
		span->n = k; // 申请了多少页
		
		// 把这个span管理的首页映射到哈希中，后面在删除这个span的时候能找到就行
		_idSpanMap[span->_pageID] = span;

		return span;
	}

    // 三种情况
    // 1、k号桶中有span
    if(!_spanLists[k].Empty())
    {
        Span* span =  _spanLists[k].PopFront();

        // 在分配之前，在哈希表中保存页号与对应的span映射关系
        for(PageID i = 0; i < span->n; ++i)
        {
            _idSpanMap[span->_pageID + i] = span;
        }

        return span;
    }
    // 2、k号桶没有span，但后面的桶有span
    for(int i = k + 1; i < PAGE_NUM; ++i)
    {
        // i号桶中有span，对span进行切分
        if(!_spanLists[i].Empty())
        {
            Span* nSpan = _spanLists[i].PopFront();

            // 新建一个span指针，然后对原span切分
            Span* kSpan = new Span;
            kSpan->_pageID = nSpan->_pageID;
            kSpan->n = k;

            nSpan->_pageID += k;
            nSpan->n -= k;
            // 将切分后的n-k页span放进对应页数的桶中
            _spanLists[nSpan->n].PushFront(nSpan);

            // 保存留在PC中的n-k页的span的首页和尾页的映射关系，方便后续合并
            _idSpanMap[nSpan->_pageID] = nSpan;
            _idSpanMap[nSpan->_pageID + nSpan->n - 1] = nSpan;

            // 保存分配出去的span每一页的映射关系
            for(PageID i = 0; i < kSpan->n; ++i)
            {
                _idSpanMap[kSpan->_pageID + i] = kSpan;
            }

            return kSpan;
        }
    }

    // 3、k号桶和后面的桶都没有span，向系统申请最大页数(128)的span

    // size_t _remanentBytes = 8192 * (PAGE_NUM - 1);
    // void* _memory = (char*)mmap(nullptr, _remanentBytes,
    //      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // // 注意：检查MAP_FAILED而非nullptr
    // if (_memory == MAP_FAILED) 
    // {
    //     // 内存分配失败处理
    //     throw std::bad_alloc();
    // }

    // 申请128KB内存
    size_t _remanentBytes = 8192 * (PAGE_NUM - 1);
    void* _memory = (char*)malloc(_remanentBytes);
    if(_memory == nullptr)
    {
        throw std::bad_alloc();
    }

    Span* bigSpan = new Span;

    bigSpan->_pageID = ((PageID)_memory) >> PAGE_SHIFT;
    bigSpan->n = PAGE_NUM - 1;

    // 申请完后放回Span链表
    _spanLists[PAGE_NUM - 1].PushFront(bigSpan);
    // 递归再次申请k页span，这次会走2逻辑
    return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* addr)
{
    // 计算对应的页号
    PageID id = ((PageID)addr) >> PAGE_SHIFT;

    // 通过哈希找到对应的span
    auto ret = _idSpanMap.find(id);

    if(ret != _idSpanMap.end())
    {
        return ret->second;
    }else{
        assert(false);
        return nullptr;
    }

}


void PageCache::ReleaseSpanToPageCache(Span* span)
{
    // 如果回收的空间是大于128页的话，则直接还给系统
    if(span->n > PAGE_NUM - 1)
    {
        void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
        free(ptr);
        delete(span);

        return;
    }

    // 向左合并
    while(1)
    {
        // 找到左边相邻页对应的span
        PageID leftId = span->_pageID - 1;
        auto ret = _idSpanMap.find(leftId);

        if(ret == _idSpanMap.end())
        {
            break;
        }

        Span* leftSpan = ret->second;
        if(leftSpan->_isUse)
        {
            break;
        }

        if(leftSpan->n + span->n > PAGE_NUM  - 1)
        {
            break;
        }

        // 合并左相邻
        span->_pageID = leftSpan->_pageID;
        span->n += leftSpan->n;

        // 在之前页面的桶中去除span
        _spanLists[leftSpan->n].Erase(leftSpan);
        delete leftSpan;
    }
    // 向右合并
    while (1)
    {
        // 右边相邻页
        PageID rightId = span->_pageID + span->n;

        auto ret = _idSpanMap.find(rightId);

        if(ret == _idSpanMap.end())
        {
            break;
        }

        Span* rightSpan = ret->second;

        if(rightSpan->_isUse)
        {
            break;
        }

        if(rightSpan->n + span->n > PAGE_NUM - 1)
        {
            break;
        }

        // 合并右相邻
        span->n += rightSpan->n;

        _spanLists[rightSpan->n].Erase(rightSpan);
        delete rightSpan;
    }

    // 合并完后，将当前span挂载到对应页数桶中
    _spanLists[span->n].PushFront(span);
    // 放回PC后，将使用标志位置0
    span->_isUse = false;

    // 补充映射关系，后续还可以合并
    _idSpanMap[span->_pageID] = span;
    _idSpanMap[span->_pageID + span->n - 1] = span;
}