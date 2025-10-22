#pragma once
#include "Common.h"
#include "FixlengthMemoryPool.h"

struct TreeNode
{
    /* data */
    int _val;
    TreeNode* _left;
    TreeNode* _right;

    TreeNode() : _val(0), _left(nullptr), _right(nullptr) {}
    TreeNode(int val) : _val(val), _left(nullptr), _right(nullptr) {}
    TreeNode(int val, TreeNode* left, TreeNode* _right) : _val(val), _left(left), _right(_right) {}
};

void TestMemoryPool()
{
    // 申请轮次
    const size_t Round = 10;

    // 每轮申请释放多少次
    const size_t N = 10000;

    std::vector<TreeNode*> vec;

    // 预分配内存
    vec.reserve(N);

    // 测试malloc
    size_t begin1 = clock();
    for(size_t i = 0; i < Round; ++i)
    {
        for(size_t j = 0; j < N; ++j)
        {
            // new底层也是调用malloc
            vec.push_back(new TreeNode(j));
        }
        for(size_t j = 0; j < N; ++j)
        {
            delete vec[j];
        }
        vec.clear();
    }
    size_t end1 = clock();

    // 申请定长内存池，其中定长大小设为一个TreeNode类型大小
    FixlengthMemoryPool<TreeNode> memoryPool;
    size_t begin2 = clock();
    for(size_t i = 0; i < Round; ++i)
    {
        for(size_t j = 0; j < N; ++j)
        {
            vec.push_back(memoryPool.New());
        }
        for(size_t j = 0; j < N; ++j)
        {
            memoryPool.Delete(vec[j]);
        }
        vec.clear();
    }
    size_t end2 = clock();

    cout << "malloc time(ms):" << end1 - begin1 << endl;
    cout << "MemoryPool time(ms):" << end2 - begin2 << endl;

}