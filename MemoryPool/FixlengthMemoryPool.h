#pragma once
#include <sys/mman.h>
#include <iostream>
using std::cout;
using std::endl;

// 模板类的实现通常放在头文件中
// 定长内存池
template <class T>
class FixlengthMemoryPool
{
private:
    /* data */
    // 指向内存的指针
    char* _memory = nullptr;
    // 剩余的字节数
    size_t _remanentBytes = 0;
    // 内存链表，连接归还的空闲内存
    void* _freelist = nullptr;

public:
    FixlengthMemoryPool();
    ~FixlengthMemoryPool();

    // 申请一个T类型大小的内存空间
    T* New()
    {
        T* obj = nullptr;

        // 如果_freelist链表中有空闲内存，表示内存池中有回收的T大小内存可重复利用，直接在内存池中申请
        if(_freelist)
        {
            // 采用头删法，从_freelist链表中取出第一个空闲内存
            // 将下一个空闲内存的指针赋值给_freelist
            // 返回第一个空闲内存
            void* tmp = *(void**)_freelist;
            obj = (T*)_freelist;
            _freelist = tmp;
        }
        else
        {
            // 当内存池剩余内存少于一个T类型大小时，申请新的内存
            if((_remanentBytes < sizeof(T)))
            {
                // 申请128KB内存
                // _remanentBytes = 128 * 1024;
                // _memory = (char*)malloc(_remanentBytes);
                // if(_memory == nullptr)
                // {
                //     throw std::bad_alloc();
                // }
                // 改用Linux下mmap系统调用的形式申请内存，适合一次性分配大块内存
                _remanentBytes = 128 * 1024;
                _memory = (char*)mmap(nullptr, _remanentBytes,
                     PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                // 注意：检查MAP_FAILED而非nullptr
                if (_memory == MAP_FAILED) 
                {
                    // 内存分配失败处理
                    throw std::bad_alloc();
                }
            }

            // 在内存池中给定一个T类型大小
            obj = (T*)_memory;
            // 判断T大小，防止T大小小于指针大小
            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
            // 将内存池指针指向下一个T类型大小的内存
            _memory += objSize;
            // 剩余内存减少一个T类型大小
            _remanentBytes -= objSize;
        }

        // new(address)T(params)表示在指定的内存地址上调用T构造函数初始化
        new(obj)T;

        // 返回T空间大小的内存
        return obj;
    }

    // 回收归还的小内存
    void Delete(T* obj)
    {
        // 调用析构函数清理内存
        obj->~T();
        // 在归还的内存中划分一个指针大小的空间用来指向下一个空闲内存
        // 采用头插法，将归还的内存插入到_freelist链表中
        // _freelist指向第一块归还内存
        *(void**)obj = _freelist;
        _freelist = obj;
    }
};

template <typename T>
FixlengthMemoryPool<T>::FixlengthMemoryPool() {

}

template <typename T>
FixlengthMemoryPool<T>::~FixlengthMemoryPool() {

}
