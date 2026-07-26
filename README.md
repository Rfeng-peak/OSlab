---

# 从零构建 RISC-V 操作系统内核

中国科学院大学 操作系统研讨课（科研实践课程）项目，从 RISC-V 汇编起步，逐步实现进程管理、同步原语、虚拟内存和网络协议栈。

**学生**：王俊峰（2023K8009937008）  
**验收方式**：面对面逐阶段验收

---

## 六个阶段概览

| 阶段 | 目录 | 内容 | 核心技术点 |
|------|------|------|-----------|
| **P0** | `p0-prime-1-200` / `p0-sum-1-50` | RISC-V 汇编入门 | 素数计算、累加求和、BIOS 跳转表调用 |
| **P1** | `Project1` | Bootloader 与启动 | 跳转表注册 BIOS 服务、BSS 清零、ELF 加载执行 |
| **P2** | `Project2` | 异常 + 系统调用 + 调度 | `stvec` 异常入口、系统调用分发、Round-Robin 抢占式调度、自旋锁 |
| **P3** | `Project3` | SMP 多核 + 同步原语 + 进程 | `do_exec` 进程构造、六大同步原语（锁/屏障/条件变量/信号量/邮箱）、Shell 交互 |
| **P4** | `Project4` | Sv39 虚拟内存 | 三级页表、独立地址空间、缺页异常 COW、TLB 预热 |
| **P5** | `Project5` | 网络子系统 | MMIO ioremap、E1000 网卡驱动、PLIC 中断、Shell 网络收发 |

---

## 技术栈

- **语言**：C（主要）、RISC-V 汇编（entry.S / bootblock.S / crt0.S）
- **目标平台**：RISC-V 64 位（RV64IMAFD），QEMU virt 模拟器
- **工具链**：`riscv64-unknown-elf-gcc`、GNU Make、U-Boot 引导
- **核心子系统**：异常/中断框架、系统调用、抢占式调度器、Sv39 虚拟内存、PLIC 中断控制器、E1000 网卡驱动
- **同步原语**：自旋锁、互斥锁、屏障、条件变量、信号量、邮箱 — 全部从零实现
- **并发模型**：SMP 多核、内核级线程（PCB）、DMA 描述符环、阻塞队列

---

## 构建与运行

每个子项目独立构建，在各自目录下执行：

```bash
make all        # 编译生成内核镜像
make run        # QEMU 启动内核
make clean      # 清理构建产物
```

> 需要预装 `riscv64-unknown-elf-` 交叉编译工具链和 QEMU（`qemu-system-riscv64`）。

---

## 项目结构

```
OSlab/
├── wangjunfeng23-main/          # 基础框架（Makefile / 链接脚本 / 测试入口）
├── wangjunfeng23-p0-prime-1-200/ # P0: 汇编素数计算
├── wangjunfeng23-p0-sum-1-50/    # P0: 汇编累加求和
├── wangjunfeng23-Project1/       # P1: Bootloader
├── wangjunfeng23-Project2/       # P2: 中断 / 调度
├── wangjunfeng23-Project3/       # P3: SMP / 同步 / 进程
├── wangjunfeng23-Project4/       # P4: Sv39 虚拟内存
└── wangjunfeng23-Project5/       # P5: E1000 网络
```

每个 Project 目录内统一组织：
- `arch/riscv/` — 架构相关代码（启动 / 异常入口 / CSR 定义 / 页表）
- `kernel/` — 内核子系统（调度 / 中断 / 同步 / 内存 / 网络 / 系统调用）
- `drivers/` — 设备驱动（屏幕 / E1000 / PLIC）
- `tiny_libc/` — 用户态 C 库（printf / syscall / string）
- `test/` — 各阶段测试程序
- `docs/` — 实验指导书与个人笔记
- `include/` — 头文件
