---

﻿# Project 1: Bootloader 与基本启动

实现 RISC-V 内核的 Bootloader，完成从 BIOS 到内核再到用户程序的三级启动流程。

**关键实现**
- 跳转表（jmptab）注册 BIOS 服务：串口读写、SD 卡读取
- BSS 段清零验证，确保未初始化全局变量行为正确
- 从 SD 卡加载 ELF 用户程序镜像，跳转执行 2048 游戏等测试程序
