# FFT TLM2.0 测试套件

## 概述

本测试套件为FFT Systolic Array (FFT_SA)模块提供完整的TLM2.0级别验证，支持多种FFT点数配置和全面的功能验证。

## 功能特性

### 🎯 测试覆盖
- **寄存器访问测试**: TLM2.0内存映射寄存器读写验证
- **基本FFT功能测试**: 端到端FFT数据路径测试
- **多点数FFT测试**: 支持4/8/16/32/64点FFT配置
- **Bypass模式测试**: 通过stage_bypass_en实现小点数FFT (16→8/4/2点)
- **参考DFT验证**: 使用标准DFT算法对比验证结果
- **Twiddle因子管理**: 自动生成和加载FFT系数

### 🔧 支持的配置
- **FFT点数**: 4, 8, 16, 32, 64点
- **Bypass模式**: 16点硬件实现8/4/2点FFT
- **数据类型**: complex<float>浮点复数
- **测试模式**: FFT/GEMM双模式支持
- **验证精度**: 可配置容差比较(默认1e-2)

### 🔄 Bypass模式说明
使用stage_bypass_en机制，可以在固定硬件上实现不同点数的FFT：
- **16点硬件 + bypass stage[0]** → 8点FFT
- **16点硬件 + bypass stage[0,1]** → 4点FFT  
- **16点硬件 + bypass stage[0,1,2]** → 2点FFT

这种设计避免了为每种点数都设计专门硬件的成本。

## 文件结构

```
testbench/
├── test_fft_tlm.cpp      # 主测试程序
├── bypass_config.h       # Bypass模式配置参数
├── Makefile              # 构建配置
├── run_fft_tests.sh      # 测试运行脚本
├── README_FFT_Tests.md   # 本文档
└── results/              # 测试结果目录 (自动创建)
    ├── fft_4pt_results.log
    ├── fft_8pt_results.log
    ├── fft_bypass_results.log
    └── ...
```

## 快速开始

### 1. 编译测试程序
```bash
cd testbench/
make
```

### 2. 运行默认测试 (4点FFT)
```bash
./run_fft_tests.sh
```

### 3. 运行特定点数测试
```bash
./run_fft_tests.sh 8      # 8点FFT
./run_fft_tests.sh 16     # 16点FFT
./run_fft_tests.sh 32     # 32点FFT
```

### 4. 运行所有测试
```bash
./run_fft_tests.sh all
```

### 5. 运行Bypass模式测试
```bash
./run_fft_tests.sh bypass
# 或者
make run_bypass
```

## 详细使用说明

### Makefile目标

| 目标 | 功能 |
|------|------|
| `make all` | 构建所有测试程序 |
| `make test_fft_tlm` | 构建FFT TLM测试 |
| `make run_fft_4` | 运行4点FFT测试 |
| `make run_fft_8` | 运行8点FFT测试 |
| `make run_fft_16` | 运行16点FFT测试 |
| `make run_fft_32` | 运行32点FFT测试 |
| `make run_fft_64` | 运行64点FFT测试 |
| `make run_fft_all` | 运行所有点数测试 |
| `make clean` | 清理构建文件 |
| `make help` | 显示帮助信息 |

### 命令行参数
测试程序支持命令行指定FFT点数：
```bash
./test_fft_tlm [点数]
```

### 测试脚本选项
```bash
./run_fft_tests.sh [选项]
```

| 选项 | 功能 |
|------|------|
| (无参数) | 运行默认4点测试 |
| `4,8,16,32,64` | 运行指定点数测试 |
| `all` | 运行所有点数测试 |

## 测试详情

### 测试流程
1. **系统初始化**: SystemC环境设置和模块实例化
2. **寄存器测试**: TLM2.0接口基础功能验证
3. **FFT配置**: 模式设置、Twiddle因子加载
4. **数据处理**: 输入数据写入、FFT计算触发
5. **结果验证**: 输出数据读取、与参考DFT比较

### 验证方法
- **参考算法**: 标准DFT (Discrete Fourier Transform)
- **比较策略**: 逐点浮点数值比较，支持容差设置
- **映射处理**: PE输出自动重构为自然顺序FFT结果

### 输出解读

#### 成功测试示例
```
========================================
FFT TLM2.0 模块测试开始
========================================

----------------------------------------
开始测试: 寄存器读写访问测试
----------------------------------------
测试结果: 寄存器读写访问测试 - 通过

----------------------------------------
开始测试: 4点FFT详细测试
----------------------------------------
加载4点FFT Twiddle因子...
Twiddle因子加载完成
4点FFT输入序列: (1,1) (2,2) (3,3) (4,4) 
4点FFT计算完成！
PE输出Y0: (10,10) (0,0) 
PE输出Y1: (-2,-2) (0,0) 
开始FFT结果验证...
FFT输出（自然顺序）: (10,10) (-2,-2) (-2,-2) (-2,-2) 
参考DFT结果: (10,10) (-2,-2) (-2,-2) (-2,-2) 
FFT验证结果: 通过
测试结果: 4点FFT详细测试 - 通过
```

#### 错误指示
- `❌ 编译失败`: 检查SystemC环境和依赖
- `❌ XX点FFT测试失败`: 检查TLM通信或算法实现
- `FFT验证结果: 失败`: 数值精度问题或算法错误

## 扩展和定制

### 添加新的测试案例
在`FftTlmTestbench`类中添加新的测试方法：
```cpp
void test_custom_scenario() {
    print_test_header("自定义测试场景");
    // 测试逻辑
    print_test_result("自定义测试场景", test_passed);
}
```

### 修改验证精度
调整`verify_fft_result`函数的tolerance参数：
```cpp
test_passed = verify_fft_result(output_y0, output_y1, input_data, N, 1e-3); // 更高精度
```

### 支持新的FFT点数
1. 在`sc_main`中添加新的case分支
2. 创建对应的`TestTopX`模块 
3. 更新Makefile和测试脚本

## 依赖和环境

### 系统要求
- SystemC 2.3.x
- GCC/G++ 支持C++17
- Linux环境 (推荐)

### 依赖库
- SystemC核心库 (`libsystemc`)
- TLM 2.0库 (SystemC自带)
- FFT测试工具库 (`fft_test_utils.h`)

### 编译配置
```makefile
SYSTEMC_DIR = /root/systemC/systemc-2.3.4
CXXFLAGS = -I$(SYSTEMC_DIR)/include -O0 -std=c++17
LDFLAGS = -L$(SYSTEMC_DIR)/lib-linux64
LIBS = -lsystemc -lm -lpthread
```

## 故障排除

### 常见问题

**Q: 编译时出现SystemC头文件找不到**
A: 检查`SYSTEMC_DIR`路径设置，确保SystemC正确安装

**Q: 测试运行时段错误**
A: 检查SystemC环境变量，确保库路径正确配置

**Q: FFT结果验证失败但数值看起来正确**
A: 可能是精度问题，尝试调整tolerance值或检查PE输出映射

**Q: 某些点数测试失败**
A: 大点数FFT可能需要更多仿真时间，调整超时参数

### 调试技巧
1. 使用详细模式查看中间结果
2. 检查TLM事务响应状态
3. 对比不同点数测试的输出模式
4. 使用SystemC波形跟踪分析时序

## 贡献和支持

### 改进建议
- 添加更多测试场景 (边界条件、错误注入)
- 支持更多数据类型 (定点数、双精度)
- 增加性能基准测试
- 添加自动化回归测试

### 联系方式
如需技术支持或反馈建议，请联系STUA-FFT开发团队。

---
*最后更新: 2025-09-08*
*版本: v2.0*