#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <pthread.h>
#include <unistd.h>
#include <mutex>
#include <algorithm>
#include <sys/mman.h>
#include <unordered_map>
#include <thread>
using std::vector;
using std::cout;
using std::endl;

// 页号表示的范围=内存地址/页空间大小
typedef unsigned long long PageID;

// 哈希表中桶的个数
static const size_t FREE_LIST_NUM = 133;

// TC单次申请的最大内存256KB
static const size_t THREADCACHE_MAX_BYTES = 256 * 1024;

// PC中span管理的最大页数
static const size_t PAGE_NUM = 129;

// 页大小2^8,即8KB
static const size_t PAGE_SHIFT = 13;

// 大块区间映射表
const struct {
    size_t aligned_size;
    int index;
} LARGE_ENTRIES[5] = {
    {16384, 128},
    {32768, 129},
    {65536, 130},
    {131072, 131},
    {262144, 132}
};

// 获取对象下一个指针
static void*& ObjNext(void* obj)
{
    return *(void**)obj;
}

class SizeClass;

class FreeList
{
private:
    // 自由链表
    void* _freeList = nullptr;

    // 自由链表未到上限时，能申请的最大空间块数量
    // 初始值为1，表示第一次申请只能申请一块
    // 当自由链表到达上限，_maxSize作废
    size_t _maxSize = 1;

    // 当前自由链表中有多少块空间
    size_t _size = 0;
public:

    // 获取当前桶中有多少块空间
	size_t Size()
	{
		return _size;
	}

    // FreeList未到上限时，能够申请的最大空间是多少块
    size_t& MaxSize()
    {
        return _maxSize;
    }

    // 将整条链n个块插入到自由链表中
    void PushRange(void* start, void* end, size_t n)
    {
        ObjNext(end) = _freeList;
        _freeList = start;

        _size += n;
    }

    // 删除链表中的n个块
    void PopRange(void* start, void* end, size_t n)
    {
        // 删除块数不能超过链表长度
        assert(n <= _size);

        start = end = _freeList;

        for(size_t i = 0; i < n - 1; ++i)
        {
            end = ObjNext(end);
        }

        _freeList = ObjNext(end);
        ObjNext(end) = nullptr;
        _size -= n;
    }

    // 空闲内存插入链表
    void Push(void* ptr)
    {
        // 确保ptr不为空
        assert(ptr);

        ObjNext(ptr) = _freeList;
        _freeList = ptr;

        ++_size;
    }

    // 空闲内存弹出链表
    void* Pop()
    {
        // 确保链表不为空
        assert(_freeList);

        void* ptr = _freeList;
        _freeList = ObjNext(_freeList);

        --_size;

        return ptr;
    }

    bool Empty()
    {
        return _freeList == nullptr;
    }
};

// 以页为基本单位的结构体
struct Span
{
public:
    // 页号
    PageID  _pageID = 0;
    // 当前span管理的页数
    size_t n = 0;
    // span管理页被切分成的块有多大
    size_t _objSize = 0;

    // span下面挂的小块空间的头节点
    void* _freeList = nullptr;
    // 当前span分配出去的块空间个数
    size_t use_count = 0;

    // 前一个span结点
    Span* _prev = nullptr;
    // 后一个span结点
    Span* _next = nullptr;

    // 判断当前span在PC还是在CC，0表示在PC，1表示在CC
    bool _isUse = false;

};

class SpanList
{
private:
    // 哨兵位头节点
    Span* _head;

public:
    // 每个桶都要有个桶锁
    std::mutex _mtx;

    SpanList()
    {
        _head = new Span;

        // 双链表
        _head->_next = _head;
        _head->_prev = _head;
    }

    bool Empty()
    {
        return _head == _head->_next;
    }

    // 在pos位置前插入一个span节点
    void Insert(Span* pos, Span* ptr)
    {
        assert(pos);
        assert(ptr);

        Span* prev = pos->_prev;

        prev->_next = ptr;
        ptr->_prev = prev;

        ptr->_next = pos;
        pos->_prev = ptr;

    }

    void Erase(Span* ptr)
    {
        // 确保ptr不为空，且不是哨兵位头节点
        assert(ptr);
        assert(ptr != _head);

        Span* prev = ptr->_prev;
        Span* next = ptr->_next;

        prev->_next = next;
        next->_prev = prev;

        // Span可以回收，因此不需要delete
    }

    // 头结点
    Span* Begin()
    {
        return _head->_next;
    }

    // 尾结点
    Span* End()
    {
        return _head;
    }

    void PushFront(Span* ptr)
    {
        Insert(Begin(), ptr);
    }

    // 获取非空SpanList中第一个span
    Span* PopFront()
    {
        Span* front = _head->_next;
        // 在链表中移除这个span
        Erase(front);

        return front;
    }
};

class SizeClass
{
    // 线程申请size的对齐规则：整体控制在最多10%左右的内碎片浪费
	// size范围				    对齐数				对应哈希桶下标范围
	// [1,128]					8B 对齐      		freelist[0,16)
	// [128+1,1024]			    16B 对齐  			freelist[16,72)
	// [1024+1,8*1024]			128B 对齐  			freelist[72,128)
	// [8*1024+1,64*1024]		1024B 对齐    		freelist[128,184)
	// [64*1024+1,256*1024]	   8*1024B 对齐  		freelist[184,208)
private:
public:
    SizeClass();
    ~SizeClass();

	// 计算每个分区对应的对齐后的字节数(大佬写法)
	static size_t _RoundUp(size_t size, size_t alignNum)
	{	
        // alignNum是size对应分区的对齐数
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}

    // 计算对齐后的字节数，size为线程申请的空间大小
	static size_t RoundUp(size_t size)
	{
		if (size <= 128)
		{ 
            // [1,128] 8B
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{ 
            // [128+1,1024] 16B
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{ 
            // [1024+1,8*1024] 128B
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{ 
            // [8*1024+1,64*1024] 1024B
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{ 
            // [64*1024+1,256*1024] 8 * 1024B
			return _RoundUp(size, 8 * 1024);
		}
		else
		{ 
            // 单次申请空间大于256KB，直接按照页来对齐
			return _RoundUp(size, 1 << PAGE_SHIFT);
			// 这里直接给PAGE_SHIFT虽然说和前一个的8 * 1024KB一样，但
			// 是我们如果后续想要修改页大小的时候就直接修改PAGE_SHIFT
			// 这样就和前面的不一样了
		}
	}

    // 求size对应在哈希表中的下标（大佬写法）
    static inline size_t _Index(size_t size, size_t align_shift)
    {
        /*这里align_shift是指对齐数的二进制位数。比如size为2的时候对齐数
        为8，8就是2^3，所以此时align_shift就是3*/
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
        //这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
    }

    // 计算映射的哪一个自由链表桶（tc和cc用，二者映射规则一样）
    static inline size_t Index(size_t size)
    {
        assert(size <= THREADCACHE_MAX_BYTES);

        // 每个区间有多少个链
        static int group_array[4] = { 16, 56, 56, 56 };
        if (size <= 128)
        { // [1,128] 8B -->8B就是2^3B，对应二进制位为3位
            return _Index(size, 3); // 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
        }
        else if (size <= 1024)
        { // [128+1,1024] 16B -->4位
            return _Index(size - 128, 4) + group_array[0];
        }
        else if (size <= 8 * 1024)
        { // [1024+1,8*1024] 128B -->7位
            return _Index(size - 1024, 7) + group_array[1] + group_array[0];
        }
        else if (size <= 64 * 1024)
        { // [8*1024+1,64*1024] 1024B -->10位
            return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
                + group_array[0];
        }
        else if (size <= 256 * 1024)
        { // [64*1024+1,256*1024] 8 * 1024B  -->13位
            return _Index(size - 64 * 1024, PAGE_SHIFT) + group_array[3] +
                group_array[2] + group_array[1] + group_array[0];
        }
        else
        {
            assert(false);
        }
        return -1;
    }

    // 粗略计算单次申请的上限块数
    // params: 对齐后的内存大小
    static size_t NumMoveSize(size_t size)
    {
        // 不能申请0字节空间
        assert(size > 0);

        // 简单控制一下申请块的数量
        int num = THREADCACHE_MAX_BYTES / size;

        /*假设本次申请8B，256K/8B得到的是一个三万多的数，
        相当于单次申请上限三万多块，这样会造成极大资源浪费，
        且并不现实，因此设定小一点*/
        if(num > 512)
        {
            num = 512;
        }

        /*如果除了后特别小，例如申请256KB，那除之后就是1，
        那这样就太过小了，因为线程可能需要多于256KB的空间，
        因此设定为2比较好，且不会过于浪费资源*/
        if(num < 2)
        {
            num = 2;
        }

        // [2, 512]

        return num;
    }

    // 块页匹配算法, size表示一块的大小
    static size_t NumMovePage(size_t size)
    {
        // 得到tc向cc申请size大小时单次最大申请的内存块数
        size_t num = NumMoveSize(size);

        // num * size是单次申请的最大空间大小
        size_t npage = num * size;

        // 右移表示除以页大小，计算分配的页数
        npage >>= PAGE_SHIFT;

        // 不足1页就给1页
        if(npage == 0)return 1;

        return npage;
    }
};