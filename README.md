---

﻿# Project 4: Sv39 虚拟内存

完整实现 RISC-V Sv39 三级页表虚拟内存，支持独立地址空间与写时复制（COW）。

**关键实现**
- `setup_vm()` 建立启动页表：内核 KVA 256MB + 恒等映射 16MB + 用户 VA 1GB，2MB 大页与 4KB 小页混合使用
- 运行时三级页表 walk + 按需分配（`alloc_page_helper`），`enable_vm()` 写 satp 开启 MMU + TLB 预热
- 每个 task 独立 VA 基址，`switch_to` 中切换页表（`csrw satp` + `sfence.vma` + `fence.i`），内核页表共享（VPN[2] >= 256）
- 缺页异常处理支持 COW 语义
