---

﻿# Project 5: 网络子系统（E1000 网卡 + PLIC 中断）

实现 MMIO 内存映射、Intel E1000 千兆网卡驱动和 PLIC 外部中断控制器，完成 Shell 网络收发。

**关键实现**
- `ioremap`：Sv39 页表直接操作 PTE，将 MMIO 物理地址映射到内核 IO 虚拟地址
- E1000 驱动：64 个发送描述符环形队列 + 64 个接收描述符环形队列（各 2KB 缓冲区），DD 位轮询 + 超时保护，MAC 地址配置 + 单播混杂模式
- PLIC 中断：初始化（优先级/使能/阈值）→ 外部中断入口（claim → 分发 → complete）→ E1000 中断唤醒阻塞队列
- Shell 添加 `send` / `recv` 命令，配套 pktRxTx 网络测试工具
