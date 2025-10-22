#include "MemoryAlloc.h"

void Test1()
{
    void* ptr1 = MemoryAlloc(15);
    void* ptr2 = MemoryAlloc(8);
    void* ptr3 = MemoryAlloc(4);
    void* ptr4 = MemoryAlloc(3);

    cout << ptr1 << endl;
    cout << ptr2 << endl;
    cout << ptr3 << endl;
    cout << ptr4 << endl;
}

void Test2()
{
    for(int i = 0; i < 1024; ++i)
    {
        void* ptr = MemoryAlloc(5);
        cout << ptr << endl;
    }
}

/* int main()
{
    TestMemoryPool();
    Test2();
    return 0;
} */