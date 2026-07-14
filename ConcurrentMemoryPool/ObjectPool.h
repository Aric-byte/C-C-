
#include<iostream>
#include <vector>
using namespace std;
using std::cout;
using std::endl;

#ifdef _WIN32
#include<windows.h>
#else
// 
#endif

////定长内存池
//template<size_t N>//表示申请内存块大小都是N
//class ObjectPool
//{
//
//};

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

template<class T>//表示每次申请固定大小的T对象 
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;//指向对象池内部大块内存中某个 T 大小的位置。
		//优先把内存块还回来的对象再次重复利用
		if (_freeList)
		{
			//头删
			void* next = *((void**)_freeList); // 读出当前头节点里存的"下一个地址"
			obj = (T*)_freeList;//obj指向当前释放出来空间的第一块
			_freeList = next; // 头指针后移
			return obj;
		}
		else
		{
			//当剩余内存不足一个对象大小的时候，重新开大块空间
			if (_remainBytes < sizeof(T))
			{
				_remainBytes = 128 * 1024;
				//_memory = (char*)malloc(_remainBytes);//给memory分配大块（此处采用适中的128K）内存
				_memory = (char*)SystemAlloc(_remainBytes >> 13);//脱离malloc,直接找堆要
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory;//obj指向内存的开始
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;//移动指针,每次“拿走”一个T大小的对象
			_remainBytes -= objSize;
		}

		//定位new，显示调用T的构造函数初始化
			new(obj)T;
			return obj;
				
	}
	void Delete(T* obj)
	{
		//显示调用析构函数清理对象
		obj->~T();
			//头插
			*(void**)/*32位和64位都可用*/obj = _freeList;// 新节点的 next 指向旧的头结点
			_freeList = obj;// 头指针指向新节点
	}
private:
		char* _memory = nullptr;//指向向系统要的大块内存的指针，char* 1字节，方便切内存块
		size_t _remainBytes = 0;//大块内存在切分过程中剩余字节数
		void* _freeList = nullptr;//归还内存过程中链接在一起的自由链表的头指针
};

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{
	}
};

void TestObjectPool()
{
	// 申请释放的轮次
	const size_t Rounds = 5;

	// 每轮申请释放多少次
	const size_t N = 100000;

	std::vector<TreeNode*> v1;
	v1.reserve(N);

	size_t begin1 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}

	size_t end1 = clock();

	std::vector<TreeNode*> v2;
	v2.reserve(N);

	ObjectPool<TreeNode> TNPool;
	size_t begin2 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < N; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t end2 = clock();

	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pool cost time:" << end2 - begin2 << endl;
}
