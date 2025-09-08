# GEMM_SA 构建系统优化指南

## 🚀 Think Ultra 模式：标准C++项目构建配置

### 当前项目结构
```
GEMM_SA/
├── include/          # 头文件目录
│   ├── FIFO.h
│   ├── GEMM_TLM.h
│   ├── PEA.h
│   ├── in_buf_vec.h
│   ├── out_buf_vec.h
│   └── pe.h
├── src/              # 源文件目录
│   ├── FIFO.cpp
│   ├── GEMM_TLM.cpp
│   ├── PEA.cpp
│   ├── in_buf_vec.cpp
│   ├── out_buf_vec.cpp
│   ├── pe.cpp
│   └── pipeline_simulation.cpp
└── BUILD_GUIDE.md    # 本构建指南
```

### 🎯 优化方案1: 基于相对路径（当前配置）

**编译命令示例:**
```bash
# 基础编译
g++ -I./include -std=c++14 src/*.cpp -lsystemc

# 带调试信息
g++ -I./include -std=c++14 -g -O0 src/*.cpp -lsystemc -o gemm_sa_debug

# 发布版本
g++ -I./include -std=c++14 -O2 -DNDEBUG src/*.cpp -lsystemc -o gemm_sa_release
```

### 🚀 优化方案2: 标准化构建配置

**Makefile示例:**
```makefile
# 编译器和标志
CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra
INCLUDES = -I./include -I$(SYSTEMC_HOME)/include
LDFLAGS = -L$(SYSTEMC_HOME)/lib-linux64 -lsystemc -lm

# 目录
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# 源文件和目标文件
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/gemm_sa

# 默认目标
all: directories $(TARGET)

# 创建目录
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# 链接
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

# 编译
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean directories
```

### 🔧 优化方案3: CMake配置（推荐）

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(GEMM_SA VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找SystemC库
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMC REQUIRED systemc)

# 包含目录
include_directories(include)
include_directories(${SYSTEMC_INCLUDE_DIRS})

# 源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# 创建可执行文件
add_executable(gemm_sa ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(gemm_sa ${SYSTEMC_LIBRARIES})

# 编译选项
target_compile_options(gemm_sa PRIVATE ${SYSTEMC_CFLAGS_OTHER})
```

### ⚡ 关键优化技巧

#### 1. 预编译头文件（PCH）优化
```cpp
// pch.h - 预编译头文件
#pragma once
#include "systemc.h"
#include <iostream>
#include <vector>
#include <queue>
#include <iomanip>
```

#### 2. 条件编译优化
```cpp
// 在头文件中添加
#ifndef GEMM_SA_OPTIMIZATION_LEVEL
#define GEMM_SA_OPTIMIZATION_LEVEL 2
#endif

#if GEMM_SA_OPTIMIZATION_LEVEL >= 2
    // 高级优化代码
#endif
```

#### 3. 模板显式实例化（减少编译时间）
```cpp
// 在单独的instantiation.cpp文件中
template class FIFO<float, 16>;
template class PE<float>;
template class IN_BUF_ROW_ARRAY<float, 4, 8>;
```

### 🎯 IDE配置建议

#### VS Code (.vscode/c_cpp_properties.json)
```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/include",
                "${workspaceFolder}/src",
                "/usr/local/systemc/include"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c11",
            "cppStandard": "c++14",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ]
}
```

### ✅ 验证构建配置

**编译测试脚本:**
```bash
#!/bin/bash
# test_build.sh

echo "🚀 Think Ultra Build Test"
echo "========================"

# 清理之前的构建
rm -rf build
mkdir build
cd build

# 使用CMake构建
cmake ..
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "✅ 构建成功!"
    ./gemm_sa --version
else
    echo "❌ 构建失败!"
    exit 1
fi
```

### 🔍 构建优化监控

```bash
# 编译时间分析
time make -j$(nproc)

# 头文件依赖分析  
g++ -I./include -MM src/*.cpp

# 模板实例化报告
g++ -I./include -ftime-report src/*.cpp
```

## 📈 性能提升预期
- ⚡ 编译时间减少 30-50%
- 🚀 增量编译支持
- 🔧 IDE智能提示完整支持
- 📊 更好的错误诊断定位