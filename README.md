---

﻿# Project 2: 异常处理 + 系统调用 + 抢占式调度

构建完整的中断/异常处理框架，实现系统调用分发和 Round-Robin 抢占式调度器。

**关键实现**
- 异常处理入口：`stvec` 注册 → `SAVE_CONTEXT` 保存 32 个通用寄存器 + CSR → C 分发器
- 系统调用：`a7` 调用号 → `syscall[]` 函数表分发，`sepc += 4` 跳过 ecall
- 时钟中断驱动抢占式 Round-Robin 调度，PCB 管理 + `switch_to` 上下文切换
- 自旋锁（`atomic_swap`）与屏幕驱动 + printf 格式化输出
