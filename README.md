# STUA-fft-mode-sim

# STUA-fft-mode-sim: SystemC FFT 加速器仿真平台

## 概述

`STUA-fft-mode-sim` 是一个基于 SystemC 和 TLM-2.0 标准构建的 FFT (快速傅里叶变换) 加速器仿真平台。该项目旨在模拟一个片上系统 (SoC)，其中包含一个专门用于 FFT 计算的 VCore (矢量核心)。它支持多帧数据处理，并提供了灵活的配置选项，用于测试不同大小和模式下的 FFT 运算。

此仿真平台的核心是通过 TLM (Transaction-Level Modeling) 建模技术，对硬件组件及其交互进行高层次的抽象，从而实现对复杂 SoC 行为的快速仿真和验证。

## 主要特性

  - **TLM-2.0 标准**: 采用业界标准的 TLM-2.0 接口，实现了组件之间的高效通信和互操作性。
  - **多帧处理**: 支持连续处理多个数据帧，以模拟真实的流数据计算场景。
  - **事件驱动控制**: 测试流程由 SystemC 事件驱动，实现了精确的时序控制和流程同步。
  - **自动化测试流程**: 实现了从数据生成、FFT 计算到结果验证的全自动化测试。
  - **模块化架构**: 系统被划分为 `Soc`、`VCore`、`DDR`、`GSM` 等多个模块，结构清晰，易于扩展。
  - **灵活的FFT配置**: 支持配置 FFT 模式、大小、蝶形级旁路等参数，以适应不同的硬件实现。

## 项目结构

```
STUA_fft_sim/
├── src/                      # 核心模块源代码
│   ├── vcore/                # VCore 及其子模块 (DMA, SPU, VPU, AM, SM)
│   │   ├── FFT_SA/           # FFT 脉动阵列 (Systolic Array) 相关模块
│   │   └── GEMM_SA/          # GEMM 脉动阵列模块
│   ├── CAC.h                 # Cache Coherent Agent
│   ├── DDR.h                 # DDR 内存模型
│   ├── GSM.h                 # Global Shared Memory 模型
│   ├── Soc.h                 # SoC 顶层模块
│   └── VCore.h               # VCore 顶层模块
├── util/                     # 通用工具和常量定义
│   ├── base_initiator_modle.h # TLM initiator 基类
│   ├── const.h               # 全局常量定义
│   ├── instruction.h         # TLM 事务指令封装
│   └── tools.h               # 通用工具函数
├── testbench.cpp             # 测试平台顶层文件
├── FFT_initiator.h           # FFT 测试激励器头文件
├── FFT_initiator.cpp         # FFT 测试激励器实现
├── Makefile                  # 项目构建文件
└── README.md                 # 项目说明文档
```

## 核心组件

  - **`Top` (testbench.cpp)**: SystemC 仿真的顶层模块，负责实例化 `Soc` 和 `FFT_Initiator` 并连接它们。
  - **`FFT_Initiator`**: 测试激励生成器，负责发起测试流程。它继承自 `BaseInitiatorModel`，实现了数据生成、FFT 计算请求和结果验证的完整逻辑。
  - **`Soc`**: 模拟一个片上系统，内部集成了 `VCore`、`DDR`、`GSM` 和 `CAC` 等关键组件，并负责它们之间的通信路由。
  - **`VCore`**: 仿真的核心计算单元，内部包含 `SPU` (标量处理单元)、`DMA`、`AM` (阵列内存)、`SM` (标量内存) 以及 `FFT_TLM` (FFT 加速器)。
  - **`DMA`**: 直接内存访问模块，负责在不同内存区域 (如 DDR、AM、SM) 之间高效地传输数据。
  - **`FFT_TLM`**: FFT 加速器的 TLM 封装模块，接收高层指令并控制底层的 `PEA_FFT` (脉动阵列 FFT) 硬件执行运算。
  - **`PEA_FFT`**: 脉动阵列 FFT 的核心实现，由多个 `PE_DUAL` (双功能处理单元) 构成，是实际执行蝶形运算的硬件模型。

## 构建与运行

### 依赖

  - **SystemC**: 需要预先安装 SystemC 库。`Makefile` 中的路径 (`/opt/systemc/`) 可能需要根据您的实际安装位置进行修改。
  - **C++ 编译器**: 需要支持 C++17 标准的编译器 (如 g++)。

### 构建步骤

1.  **配置 Makefile**: 打开 `Makefile` 文件，确认 `INCLUDE_DIR` 和 `LIB_DIR` 变量指向正确的 SystemC 库路径。

2.  **执行构建**: 在项目根目录下运行 `make` 命令来编译和链接所有源文件，生成名为 `main` 的可执行文件。

    ```bash
    make
    ```

### 运行仿真

构建成功后，执行以下命令来运行仿真：

```bash
make run
```

或者直接运行生成的可执行文件：

```bash
./main
```

仿真将启动并运行 `2000 ns` 的模拟时间，您将在控制台看到详细的日志输出，包括每个测试帧的数据生成、计算过程和验证结果。
