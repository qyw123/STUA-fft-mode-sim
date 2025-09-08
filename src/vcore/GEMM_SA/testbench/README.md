# GEMM_SA 测试平台使用指南

## 🚀 快速开始

### 编译和运行
```bash
# 进入testbench目录
cd /path/to/STUA_gemm_sim/src/vcore/GEMM_SA/testbench

# 编译程序
make

# 运行完整测试
make run

# 快速测试（输出前50行）
make test
```

## 📋 可用命令

| 命令 | 功能 |
|------|------|
| `make` | 编译程序 |
| `make debug` | 编译调试版本 |
| `make run` | 编译并运行完整测试 |
| `make test` | 快速测试运行（限制输出） |
| `make verbose` | 详细测试运行（保存日志） |
| `make clean` | 清理编译文件 |
| `make check-systemc` | 检查SystemC安装 |
| `make info` | 显示构建信息 |
| `make help` | 显示帮助信息 |

## 🏗️ 系统要求

### SystemC依赖
- SystemC 2.3.4 或更新版本
- 默认安装路径：`/opt/systemc`
- 如果安装在其他位置，请修改Makefile中的`SYSTEMC_HOME`变量

### 编译器要求
- g++ 支持C++17标准
- 支持SystemC编译

## 📊 测试内容

### GEMM脉动阵列测试
- **矩阵运算**：C = A×B + D
- **默认测试尺寸**：100×100矩阵
- **支持变长矩阵**：1×1 到 16×16
- **大矩阵分块**：自动分块处理超过16×16的矩阵

### 测试模式
1. **单帧模式**：矩阵尺寸 ≤ 16×16
2. **分块模式**：矩阵尺寸 > 16×16，自动分块处理
3. **流水线分析**：性能统计和优化分析

## 🔧 自定义配置

### 修改测试矩阵尺寸
在`gemm_pingpong_test.cpp`中修改：
```cpp
const int DEFAULT_M = 100;  // 矩阵A行数
const int DEFAULT_K = 100;  // 内部维度
const int DEFAULT_N = 100;  // 矩阵B列数
```

### 修改PE阵列大小
在代码中修改：
```cpp
const int PEA_SIZE = 16;    // PE阵列大小
```

## 📈 性能分析

测试程序提供详细的性能统计：
- **基础执行时间**：加载、计算、读取耗时
- **流水线分析**：重叠效率、吞吐率提升
- **PE利用率**：硬件资源利用情况
- **分块统计**：大矩阵分块处理信息

## 🐛 故障排除

### 编译错误
1. 检查SystemC安装：`make check-systemc`
2. 确认编译器版本支持C++17
3. 检查路径配置是否正确

### 运行错误
1. 确认可执行文件权限
2. 检查SystemC库路径
3. 查看详细错误日志：`make verbose`

### 常见问题
- **链接错误**：通常是SystemC库路径问题
- **头文件找不到**：检查include路径配置
- **运行时错误**：查看SystemC版本兼容性

## 📝 输出说明

### 正常输出示例
```
🚀 Think Ultra 简化GEMM测试开始
  测试矩阵: A[100×100] × B[100×100]
📊 检测到大矩阵，启动分块模式
📋 3D分块策略: 7×7×7 = 343 个3D块
✅ 大矩阵成功分解为 343 个计算块
🎯 Think Ultra GEMM测试完成!
  基础执行时间: 327240 ns
  流水线分析时间: 97895 ns
  流水线分析帧数: 343
```

### 性能统计输出
- **重叠效率**：流水线重叠优化效果
- **吞吐率提升**：相对于顺序执行的性能提升
- **PE利用率**：处理单元的使用效率

## 🎯 开发指南

### 添加新测试
1. 在`gemm_pingpong_test.cpp`中添加测试函数
2. 修改`run_gemm_test()`调用新测试
3. 重新编译运行

### 修改矩阵尺寸限制
1. 修改`PEA_SIZE`常量
2. 确认硬件资源足够
3. 重新编译测试

### 调试模式
使用调试编译：
```bash
make debug
./gemm_pingpong_test
```

## 📞 联系支持

如遇问题，请检查：
1. 构建信息：`make info`
2. SystemC状态：`make check-systemc` 
3. 详细日志：`make verbose`