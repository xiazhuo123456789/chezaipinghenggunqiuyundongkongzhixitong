# 车载平衡滚球运动控制系统 H 题

#### Description
This project is the vehicle main controller code for the 2026 Electronic Design Competition Problem H "Vehicle-Mounted Ball Balancing Control System" . It runs on the TI MSPM0G3507 (Tianmengxing 3507 development board) and works with the K230 vision module to achieve composite control tasks including line tracking, steel ball balancing, and dynamic ball stabilization.

The system core adopts a three-interrupt layered architecture : real-time tasks (tracking, key scanning, task coordination) are placed in the 10ms timer interrupt; visual PID calculation is placed in the 50Hz timer interrupt; UART reception is placed in the UART interrupt; non-real-time tasks (menu, display) are placed in the main loop. Modules are decoupled through Tick functions, keeping the main file at only 60 lines.

Core Features:

- 5-channel infrared line tracking (PD control + three-stage precise parking)
- K230 vision reception (UART 115200, dx=±XXX protocol)
- Steel ball position closed-loop PID control (50Hz servo adjustment)
- 6-task unified scheduling (table-driven configuration, one-click start)
- OLED menu system (5-page table-driven switching)
- Global volatile + critical section protection (concurrency safety)

#### Software Architecture
Overall Architecture: Three-Interrupt Layering + Dual Table-Driven Design


#### Installation

- Setup Development Environment

- Install Code Composer Studio (CCS) 12.6+ or Theia 1.x
- Install MSPM0G3507 SDK v2.01+
- Compiler: TI Clang for ARM
- Debugger: XDS110 (on-board) or J-Link
- Import Project

#### Instructions

1.  xxxx
2.  xxxx
3.  xxxx

#### Contribution

1.  Fork the repository
2.  Create Feat_xxx branch
3.  Commit your code
4.  Create Pull Request


#### Gitee Feature

1.  You can use Readme\_XXX.md to support different languages, such as Readme\_en.md, Readme\_zh.md
2.  Gitee blog [blog.gitee.com](https://blog.gitee.com)
3.  Explore open source project [https://gitee.com/explore](https://gitee.com/explore)
4.  The most valuable open source project [GVP](https://gitee.com/gvp)
5.  The manual of Gitee [https://gitee.com/help](https://gitee.com/help)
6.  The most popular members  [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
