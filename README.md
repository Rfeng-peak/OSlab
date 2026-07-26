---

﻿# Project 3: SMP 多核 + 同步原语 + 进程管理

实现多核启动（SMP），从零构建六大同步原语，完成进程创建与 Shell 交互。

**关键实现**
- `do_exec()` 构造进程：分配独立页表 → 内核栈 + 用户栈 → 加载 ELF → 布置 `regs_context_t` / `switchto_context_t` 双上下文 → 插入就绪队列
- 六大同步原语：自旋锁、互斥锁（锁 + 阻塞队列）、屏障 Barrier、条件变量、信号量、邮箱 Mailbox（环形缓冲 + 双向阻塞队列）
- Shell 实现 `exec` / `waitpid`，精心设计用户栈 argv 布局（16 字节对齐、NULL 哨兵）
