Data Plane Development Kit (DPDK) : A Software Optimization Guide to the User Space-Based Network Applications

Heqing Zhu

[TOC]

# wording

**临界区域**critical section是指多使用者可能同时共同操作的那部分代码，比如自加自减操作，多个线程处理时就需要对自加自减进行保护，这段代码就是临界区域。

 thread switching between cores can easily lead to performance losses due to cache misses and cache write-back

For example, Huge Page memory can reduce the TLB misses, multichannel-interleaved memory access can improve the total bandwidth efficiency, and the asymmetric memory access can reduce the access latency. The key idea is to get the data into cache as quickly as possible, so that CPU doesn’t stall.

Basically, all industry-leading Ethernet companies are offering the PMD at DPDK.

In theory, I/O interface, PCIe bus, memory bandwidth, and cache utilization can set quantitative limits; it is easy to say but not easy to do, as it requires in-depth system-level understanding. 

- software perspective

- host/tenant(VM, container)

- transaction efficiency

# 2 cache & memory

## 2.4 Cache

The cache is introduced as the data latency reduction mechanism between processor and memory, and it enhances the data throughput significantly, as the program tends to access the local data more often. For a software programmer to make the best of cache mechanism, it is important to understand the concepts such as the *cache line*, *data prefetch*, *cache coherency*, *and its challenge* for software optimization. 

### 2.4.1 CACHE LINE

```
#define RTE_CACHE_LINE_SIZE 64
#define __rte_cache_aligned __attribute__((__aligned__(RTE_CACHE_LINE_SIZE)))
```

### 2.4.2 CACHE PREFETCHING

Cache prefetch is a predictive action. It speculates to load the data that will be used soon. “C” programming language has not defined an explicit function to do the data prefetch. You may have seen that the prefetch option is quite common in the system BIOS, which can turn on|off the CPU prefetcher. Most CPU will speculate what’s the next data/instruction to be used, the speculative mechanism will load the instructions/data from memory to cache, and it can be done without software code to make a request. But it can also be done with software’s explicit ask. As such, **data locality** is one design principle; if the instructions or data may be temporarily stored in the cache, there is a higher chance to be used by CPU again. A software loop is such an example: CPU will repeatedly execute the instructions until loop exits. **Spatial locality** is another example; it refers to the instruction or data to be used that may be adjacent to the instruction or data being used now. As said, a cache line is designed as 64 bytes long. By loading more data from the adjacent memory, it can reduce the access latency for future processing; hence, it helps the performance.

However, there is a risk due to the nature of speculative execution. If the prefetch data is not the actual data that the CPU will use, t**he prefetch can pollute the limited cache space**. Another load will be issued later, and in the worst case, this may lead to performance degradation. In some cases, even if the hardware prefetcher is activated, the program may not see the benefits. It is workload specific, and the result depends on the data and code execution. Likely you may (or not) see some system optimization guides to suggest: Turn off prefetch as part of the system tuning practice.

### 2.4.3 SOFTWARE PREFETCHING

x86 CPU allows the software to give hints for the data prefetcher.

### 2.4.4 FALSE SHARING

The LLC (L3) has a larger capacity, and it is shared by all cores. In certain cases, the same memories are accessed by multiple cores, causing conflict to occur when multiple cores write or read the data *in the same cache line*. x86 is designed with a sophisticated mechanism to ensure cache coherency, and software programmers can enjoy such CPU features without worrying about **data contention and corruption** in a multicore running environment. There is a cost for the data contention in a cache line, and if multicore is trying to access the different data in the same cache, CPU will invalidate the cache line and force an update, hurting the performance. T**his data sharing is not necessary** because the multiple cores are not trying to access the same data,

which is known as false sharing. The compiler can find the false sharing, and it will try to eliminate the false sharing at the optimization phase. If compiler optimization is disabled, then there is no compiler attempt to work on this problem.

### 2.4.5 CACHE COHERENCY

The cache coherence is handled by CPU since this reduces software complexity. There is a performance hit if multiple cores need work on the same cache line. DPDK is expected to be deployed on a multicore environment. One basic rule is to avoid multiple cores to access the same memory address or data structure as much as possible

When multiple cores want to access the same NIC, DPDK usually will prepare one RXQ and one TXQ for each core to avoid the race condition. 

### 2.4.6 NOISY TENANT AND RDT

When multiple cores are competing for the shared cache resource, some cores may repeatedly ask for more cache resources, and the remaining cores are forced with less cache to use; the **aggressive core** is somewhat acting as the role of the noisy neighbor. This is not a real problem if the whole system is utilized by single owner, who will make the decision to assign resources correctly. In public cloud computing, where resource is shared to accommodate multiple guests’ workloads together, avoiding the noisy neighbor is important because all tenants are paid for the resource. It is highly desirable to provide a more granularity control on using the shared cache resource. Intel® proposed RDT (Resource Director Technology) framework to tackle the “noisy neighbor” problem, and RDT includes multiple technology ingredients such as cache monitoring, **cache allocation**, memory bandwidth monitoring, and cache and data optimization technologies. RDT is not specific to DPDK, so it is not introduced in this book.

## 2.5 TLB and HugePage

In the computing system, software uses the virtual memory address, not the physical memory address. Memory management and paging technology has been widely used for address translation. System memory is organized in pages, and the traditional memory page size is 4 KB. HugePage is later introduced and Linux supports the hugepage size as 2 MB or 1 GB. The memory translation is indeed a multilevel page table lookup. TLB (translation lookaside buffer) is part of the CPU. It can speed up the memory address translation, and it is a cache for virtual memory to physical memory. On a given virtual address, if the entry resides in the TLB, the physical address will be found immediately on a given virtual address. Such a match is known as a TLB hit. However, if the address is not in the TLB (TLB miss), the CPU may do the page walk to finish the address translation, which takes long cycles as it may do the multiple memory references; if the page table is not available in cache, the miss will lead to the memory access, which depends on the access latency, potentially goes up to hundreds of cycles.

# 4 Synchronization

## 4.1 Atomic and Memory Barrier

An atom refers to “the smallest particle that cannot be further divided”. From the computing perspective, atomic operation refers to “one operation cannot be interrupted”. In a multiple threads’ software program, one thread completes the operation, whereas the other threads will see that the operation is completed instantaneously. Atomic operation is the foundation for the synchronization, and atomic memory access is important, as all cores share the memory.

### 4.1.1 MEMORY BARRIER

Memory barrier, also known as memory fence, enforces the memory access (load/store) in order. Memory load or store can be expensive as the access latency can go up to hundreds of cycles in the context of DRAM access. Intel® Xeon-based CPU has designed with out-of-order execution; software compiler can help with the program optimization attempt. CPU and compiler may find ways to execute program with less memory access, which may lead to program reorder, including the reorder of memory access. The impact is the program execution order is different than the software code order. If the shared data is already loaded from memory to cache, the memory read/modify/write (RMW) operation can happen in the cache, even used by other cores; this means less latency due to the caching benefits. In some cases, it is okay to allow the program reorder, but not always.

Manage the shared data’s memory use to follow the exact program order; x86 supports LFENCE, SFENCE, and MFENCE instructions to realize the memory barriers.

- *LFENCE* is a memory load fence; this ensures a serialization operation on all the **previous** memory loads that are globally visible.
- *SFENCE* is a memory store fence; it only enforces that all **previous** memory stores are globally visible.
- *MFENCE* is the combination to enforce that all the **previous** memory load and store operations are globally visible.

### 4.1.2 ATOMIC OPERATION

In the old days of a single-processor (=core) system, a single instruction can be regarded as “atomic operation”, because interrupt can only occur among instructions, if there are no other tasks running in parallel. The memory operation like “INC” is indeed memory RMW; it is “atomic” in a single-task environment. In a multicore system, if the instruction is not performed on the shared data, such an instruction is atomic, because there is no other access to the data. When multiple cores are trying to access the shared data, even for a simple “INC”, it is still translated into actions like the memory RMW, but for all involved cores. When multiple cores are accessing the same data in memory, the data might be cached by a core; the cached data is also used by other cores; the remaining cores will be updated automatically if there is any data change; x86 is designed with cache coherence to guarantee this, but it comes at a cost of using more cycles to achieve the data coherency.

The core can perform reads and writes to the aligned bytes, words, double words, and four bytes atomically; for unaligned bytes, words, double words, and four bytes, their reads and writes might be atomic if the memory load/store happened in the same cache line, because CPU always loads/stores data in the size of a cache line. But if the data resides in the multiple cache lines, this becomes complicated as a result of multiple load/store operations. A simple solution is to ensure that the shared data stays in the same cache line, which avoid the unnecessary data spreading over multiple cache lines. A program directive like “`__rte_cache_aligned`” can ask the compiler to enforce the cache line alignment on the specified data structure. This works for the small data structure, whose size is less than 64 bytes (x86).

x86 supports “`LOCK`” instruction prefix; it was a bus lock; other cores will not access the memory if “`LOCK`” signal is asserted. This helps the relevant instructions to achieve the atomic memory access. This is **not free** as other cores are now waiting for this to complete first. “Compare and swap (`CAS`)” is a common way for multi-threading to achieve synchronization. x86 implements this as an instruction—“compare and exchange (CMPXCHG)”. LOCK prefix is used together on a multicore system. This is the foundation to implement the lock or is used for lockless data structure design. Together with lock, “CMPXCHG” instruction is used for lockless data structures; the lockless queue is performant way for the multicore to exchange the data.

### 4.1.3 LINUX ATOMIC OPERATION

Software atomic operation relies on hardware atomic operation. Linux kernel supports the atomic operations for simple flags, atomic counters, exclusive ownership, and shared ownership in the multi-threaded environment. It provides with two sets of software interfaces: One is the integer level, and the other is the bit level.

| Atomic Integer Operation                    | Functions                                                                                 |
| ------------------------------------------- | ----------------------------------------------------------------------------------------- |
| ATOMIC_INIT(int i)                          | When one atomic_t variable is claimed, it shall be initialized as i                       |
| int atomic_read(atomic_t *v)                | Atomically read the integer variable v                                                    |
| void atomic_set(atomic_t *v, int i)         | Atomically set the value of v as i                                                        |
| void atomic_add(int i, atomic_t *v)         | Atomically add i tov                                                                      |
| void atomic_sub(int i, atomic_t *v)         | Atomically subtract i from v                                                              |
| void atomic_inc(atomic_t *v)                | Atomically add i to v                                                                     |
| void atomic_dec(atomic_t *v)                | Atomically subtract 1 from v                                                              |
| int atomic_sub_and_test(int i, atomic_t *v) | Atomically subtract 1 from v. If the result is 0, returns true; otherwise, returns false. |
| int atomic_add_negative(int i, atomic_t *v) | Atomically add i to v. If the result is negative, returns true; otherwise, returns false. |
| int atomic_dec_and_test(atomic_t *v)        | Atomically subtract 1 from v. If the result is 0, returns true; otherwise, returns false. |
| int atomic_inc_and_test(atomic_t *v)        | Atomically add 1 to v. If the result is 0, returns true; otherwise, returns false.        |

| Atomic Bit Operation                        | Functions                                                                             |
| ------------------------------------------- | ------------------------------------------------------------------------------------- |
| void set_bit(int nr, void *addr)            | Atomically set No. nr of the object where addr points                                 |
| void clear_bit(int nr, void *addr)          | Atomically clear No. nr of the object where addr points                               |
| void change_bit(int nr, void *addr)         | Atomically flip No. nr of the object where addr points                                |
| int test_and_set_bit(int nr, void *addr)    | Atomically set No. nr of the object where addr points and return the original value   |
| int test_and_clear_bit(int nr, void *addr)  | Atomically clear No. nr of the object where addr points and return the original value |
| int test_and_change_bit(int nr, void *addr) | Atomically flip No. nr of the object where addr points and return the original value  |
| int test_bit(int nr, void *addr)            | Atomically return No. nr of the object where addr points                              |

### 4.1.4 DPDK ATOMIC OPERATION

Atomic operation is about data access, and data sits in the memory. The atomic interface is mostly about the atomic memory access. DPDK implements its own memory barriers and atomic interfaces

#### 4.1.4.1 Memory Barrier API

It keeps the memory load/store instructions issued before the memory barriers are global visible (for all cores).

- *rte_mb()*: Memory barrier read-write API, using MFENCE.
- *rte_wmb()*: Memory barrier write API, using SFENCE.
- *rte_rmb()*: Memory barrier read API, using LFENCE.

## 4.2 RW Lock

The RW (Reader Writer) lock—`rwlock`—is a lock for resource protection. If it is read-only access, the shared memory will not be modified, and the multiple core (reader) access is safe for parallel access. But write access is different as data will be modified; the access needs to be exclusive. Compared to the spinlock, the rwlock improves the read concurrency in the multicore system. The write operation is exclusive. This means that *only a write operation is allowed*, while others need to delay the access until the write is completed.

The RW spinlock is characterized as follows:

- Resources between read locks are shared. When a thread has a read lock, other threads can share the access by reading only.
- Write locks are mutually exclusive. When a thread has a write lock, other threads can’t share it either by reading or by writing.
- Read and write locks are mutually exclusive. When a thread has a read lock, other threads can’t access it by writing.

本质是 CAS + spin lock

## 4.3 Spinlock

Spinlock is a locking mechanism to access a shared resource. Before acquiring the lock, the caller can stay in the busy-waiting mode. Once the lock is acquired, the access to shared resource will get started; when the access is completed, the spinlock can be released. In the previous section, write lock is a spinlock, which is used for the mutual exclusion scenario, but read lock is different, it is counter-based for concurrent access.

Spinlock is designed as a solution for the mutually exclusive use of shared resources; it is similar to the mutex lock, but there is a difference. For a mutex lock or a spinlock, only one owner can acquire the lock. However, if the lock is acquired by others,

- For the mutex lock, the caller will enter the sleep mode.
- For the spinlock, the caller will keep spinning—**busy waiting**.

Because the spinlock is busy waiting, it has no extra cost of context switching, so it is faster. It is worth noting that core is not doing anything useful in the busy wait period. The spinlock needs to be used with caution; it is easy to cause the deadlock if there is a repeated lock request from the same caller thread. Note that DPDK has not implemented its own mutex.

## 4.4 Lock-Free

When lock is in use, the waiting cores are not utilized in an optimized way. Lock helps the data synchronization, but it forces the other cores to wait until the shared resource is available. The server platform has lots of core for software use. If software focuses on high performance, core-based scaling is important to achieve the high concurrency, the resource is ideally localized, and the data sharing needs to be as less as possible. But data sharing is often required, in this case, a lock-free mechanism can improve software performance. This enables more cores to work meaningfully in parallel. Performance scaling is a key design goal for DPDK, a multi-threading software that focuses on moving packets from cores to cores safely and fast.

To realize this, DPDK introduced its ring library; this is a specific optimization for packet processing workload, known as rte_ring; it is based on FreeBSD’s bufring mechanism. In fact, rte_ring supports the flexible enqueue/dequeue models for different cases.

# 8 NIC-Based Parallelism

The Linux utility “`ethtool`” can query and configure the NIC device for the multi-queue, and channel is a similar concept to queue. The utility is also used for virtual NIC, such as virtio multi-queue, which will be discussed later.

### 8.2.2 RECEIVE-SIDE SCALING (RSS)

The RSS concept is discussed earlier. As a NIC hardware feature designed for load balance use case, once enabled, RSS can distribute packets to different queues, and it is based on a hash calculation on certain packet header fields. 

### 8.2.3 FLOW DIRECTOR (FDIR)

Flow Director originated from Intel®, and the NIC can distribute packets to different queues according to a match on certain packet header fields (such as n-tuples). For RSS, most assigned cores (serving the RX queues) are doing similar processing. For FDIR use case, the assigned cores may work on different processing, because the RX queue may get different packet flow; for example, IPsec flows can be sent into some queue, and UDP traffic can be directed into other queues. This approach can enable the application target processing, and the target application may depend on the destination port or IP address. [Figure 8.4](http://127.0.0.1:5001/xhtml/C13_chapter8.xhtml#fig8_4) shows how it works.

### 8.2.4 NIC SWITCHING

If the NIC is embedded with an internal switch, sometimes it is called **SR-IOV-based switching**, and the packet forward decision can be made on a NIC; it compares the Ethernet header such as MAC/VLAN fields to do an internal switching, for example, from VM1 to VM2, because VM1 and VM2 can be placed in the same network. This is a Layer 2-based networking solution to interconnect VMs. 

### 8.2.5 CUSTOMIZED FLOW CLASSIFICATION

 Some advanced NICs (such as Intel® XL710) are able to support the additional custom firmware; essentially, it configures the NIC with the new capability, and this additional feature is known as **DDP** (Dynamic Device Personalization), which helps the workload-specific optimization 

Without DDP, Intel® XL710 is not able to parse certain packet types; for example, PPPoE and GTP-U are not supported, which implies that NIC is limited to directing the PPPoE/GTP-U traffic into multiple queues. PPPoE is heavily used for the broadband access gateway, and GTP-U is heavily used for the wireless core gateway. DDP leverages the built-in packet pipeline parser in NIC, but it goes a little further on the packet header parsing. With DDP support, the additional firmware can be loaded into the NIC, and it adds the new capability to NIC at runtime. FPGA is a popular choice to make a Smart NIC, and using the FPGA programmability to add the advanced flow classification is also a common choice.

### 8.3.2 VIRTUALIZATION

Intel® XL710 NIC can be configured as follows:

- PF is managed by the Linux kernel driver (i40e)
- VFs, passed through to VMs, can be managed by DPDK (i40evf) driver.

The Linux “ethtool” utility can configure the Flow Director on the NIC, and the specific packets can be directed and sent to a given VF’s queues. configure 

```
ethtool –N ethx flow-type ether dst 00:00:00:00:01:00 m
ff:ff:ff:ff:ff:ff src 00:00:00:00:20:00 m 00:00:00:00:00:00
vlan 10 user-def 0x800000000 action 2 loc 1
```

### 8.3.3 RTE_FLOW

Rte_flow is the DPDK API to configure the packet flow, it has been introduced since DPDK v17.02, and the intent is to provide a generic means for flow-based configuration, so that the hardware can handle the ingress or egress traffic at the flow level. NIC is very different, and a generalized software interface is desired to work with all PMDs. As a result, rte_flow API is at a somewhat higher level, and it is used to configure NIC or even FPGA.

The below example (test-pmd) demonstrates how to create flow rules for a NIC (Intel® XL710) with two approaches: Configure via command lines, or configure via rte_flow source call.

Let’s start first with the command lines.

1. Bind NIC to igb_uio
   
   ```
   ./usertools/dpdk-devbind.py -b igb_uio 0000:06:00.0
   ```

2. Create a VF on DPDK PF; then, bind it to vfio-pci
   
   ```
   echo 1 > /sys/bus/pci/devices/0000:06:00.0/max_vfs
   ./usertools/dpdk-devbind.py -b vfio-pci 0000:06:02.0
   ```

3. Launch two test-pmd instances with PF and VF separately
   
   ```
   ./x86_64-native-linuxapp-gcc/app/test-pmd -c 1ffff   -n
   4--socket-mem 1024,1024--file-prefix=pf -w 06:00.0-- -i
   ```
