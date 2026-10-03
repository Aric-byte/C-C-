# C/C++ Systems Practice

面向 C/C++ 底层能力、内存管理和数据结构实践的个人项目仓库。

当前仓库的重点项目是一个面向定长对象的对象池（Object Pool）。它绕过通用 `malloc/new` 的频繁分配路径，按大块向系统申请内存，再以固定大小切分并复用对象空间，用于观察内存分配策略、空闲链表和对象生命周期管理之间的关系。

> 当前实现是单线程实验版本，名称中的 `Concurrent` 代表后续演进方向，并不表示当前代码已经提供线程安全保证。Linux 分支的系统内存申请也仍待补充。

## 项目亮点

- 使用模板支持不同的定长对象类型；
- 使用空闲链表复用已经释放的对象空间；
- 使用 placement new 显式构造对象；
- 在回收时显式调用析构函数；
- Windows 下通过 `VirtualAlloc` 按页向系统申请大块内存；
- 对比原生 `new/delete` 与对象池路径的耗时；
- 兼顾 32 位和 64 位环境下空闲链表节点指针的存储方式。

## 目录结构

```text
C-C++/
├── ConcurrentMemoryPool/
│   ├── ObjectPool.h   # 系统内存申请、ObjectPool<T> 与基准测试
│   ├── Test.cpp       # 测试入口
│   └── README.md      # 对象池子项目说明
└── README.md
```

## 核心设计

### 大块申请与固定切分

当剩余空间不足以容纳一个 `T` 对象时，代码一次性申请 128 KiB 内存块，然后按对象大小切分：

```text
系统内存 → 128 KiB 大块 → [T][T][T][T]... → New() 移动当前指针
```

相比每次调用通用堆分配器，这种方式减少了系统分配调用次数，适合对象大小固定、创建和销毁频繁的场景。

### 空闲链表复用

释放对象时，内存不会立刻归还系统，而是把对象占用的区域暂时当作链表节点，插入 `_freeList` 头部。下一次 `New()` 优先从链表头部取出，因此回收和复用都是 O(1)。

### 对象生命周期与原始内存分离

```cpp
new (obj) T;    // 在指定地址构造 T
obj->~T();      // 显式析构 T
```

对象池管理原始存储，对象自身的构造和析构由 placement new 与显式析构控制。

## 运行流程

```text
ObjectPool<T>::New()
    ├── 有空闲节点：从 free list 头部取出
    ├── 大块仍有空间：切出一个 T 大小的区域
    ├── 空间不足：申请新的 128 KiB 区域
    └── placement new 构造 T 并返回

ObjectPool<T>::Delete(T*)
    ├── 显式调用析构函数
    ├── 将当前地址改造成 free list 节点
    └── 头插回收空间
```

## 运行测试

测试程序使用 `TreeNode` 作为对象类型，多轮对比原生 `new/delete` 与 `ObjectPool<TreeNode>::New/Delete` 的 `clock()` 耗时。

### Visual Studio

1. 创建 C++ 控制台项目；
2. 添加 `ConcurrentMemoryPool/ObjectPool.h` 和 `ConcurrentMemoryPool/Test.cpp`；
3. 使用 C++17 或更新标准编译；
4. 运行程序并观察两条路径的耗时。

### MinGW / GCC

当前 `SystemAlloc()` 只实现了 Windows 分支，因此 Linux 下不能直接获得等价实现。Windows MinGW 可尝试：

```bash
g++ -std=c++17 -O2 ConcurrentMemoryPool/Test.cpp -o object_pool.exe
object_pool.exe
```

输出格式：

```text
new cost time:...
object pool cost time:...
```

具体数值会受到编译器、优化级别、CPU、操作系统和系统负载影响，不应把单次运行结果当作稳定结论。

## 复杂度与资源模型

| 操作 | 平均复杂度 | 说明 |
|---|---:|---|
| `New()` 复用空闲节点 | O(1) | 从空闲链表头部取出 |
| `New()` 切分大块空间 | O(1) | 移动 `_memory` 指针 |
| `Delete()` | O(1) | 析构后头插回收链表 |
| 新内存块申请 | O(1) 次系统调用 | 以 128 KiB 为基本申请单位 |

对象池以空间换取分配效率。当前版本会保留已申请的大块内存，没有实现按块归还系统、异常安全保护或跨线程同步。

## 当前边界

当前版本适合学习和验证内存分配路径、空闲链表、placement new 和固定对象切分策略，但暂不应直接用于生产环境：

- `_freeList` 没有加锁，不支持多个线程同时调用 `New()` / `Delete()`；
- Windows 使用 `VirtualAlloc`，Linux 分支尚未实现；
- 没有校验指针是否属于当前对象池；
- 对象池析构时没有释放已申请的内存块；
- `Delete(nullptr)`、重复释放和跨池释放没有防护；
- 基准测试只使用 `clock()`，没有统计分配次数、峰值内存和多线程吞吐；
- 当前对象池只适合固定大小的 `T`。

## 后续演进计划

### 正确性与资源管理

- 记录每个大块内存区域，并在析构时统一释放；
- 增加空指针、重复释放、跨池释放检查；
- 将 `SystemAlloc` 抽象为跨平台内存后端；
- 增加单元测试和 AddressSanitizer 检查。

### 并发版本

- 设计线程本地缓存，减少全局锁竞争；
- 对中央空闲链表使用互斥锁或无锁结构；
- 比较单线程、互斥锁版本和线程本地缓存版本的吞吐；
- 记录延迟分布，而不只比较总耗时。

### 工程化基准

- 使用 Google Benchmark 或等价工具；
- 增加不同对象大小、线程数和申请释放比例的测试矩阵；
- 使用 Valgrind、AddressSanitizer、ThreadSanitizer 检查内存与并发问题；
- 增加 CMake 构建和 CI 编译验证。

## 技术关键词

`C++` `Memory Pool` `Object Pool` `Free List` `Placement New` `Explicit Destructor` `VirtualAlloc` `Memory Reuse` `Benchmark` `Concurrency Roadmap`

## License

本仓库当前未声明开源许可证。如需公开复用，建议补充 MIT License 或其他明确许可证文件。
