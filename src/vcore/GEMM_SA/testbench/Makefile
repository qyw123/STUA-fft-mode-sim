# Makefile for GEMM_SA testbench

# Compiler
CXX = g++

# Directories - SystemC installation paths
SYSTEMC_HOME = /opt/systemc
INCLUDE_DIR = $(SYSTEMC_HOME)/include
LIB_DIR = $(SYSTEMC_HOME)/lib

# 项目路径配置
PROJECT_ROOT = ../../../../..
GEMM_SA_ROOT = ..
GEMM_INCLUDE_DIR = $(GEMM_SA_ROOT)/include
GEMM_SRC_DIR = $(GEMM_SA_ROOT)/src

# Compiler flags
DEBUG_FLAGS = -O0 -Wall -Wno-unused-variable -fsanitize=address,undefined -g
RELEASE_FLAGS = -O2 -DNDEBUG
CXXFLAGS = -std=c++17 -DSC_ALLOW_DEPRECATED_IEEE_API \
           -I. \
           -I$(INCLUDE_DIR) \
           -I$(GEMM_INCLUDE_DIR) \
           -I$(GEMM_SRC_DIR) \
           $(RELEASE_FLAGS)

# Linker flags
LDFLAGS = -L$(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LIBS = -lsystemc -lm

# Target executable
TARGET = gemm_pingpong_test

# Source files
MAIN_SRC = gemm_pingpong_test.cpp

# Implementation source files that need to be compiled
# Note: pipeline_simulation.cpp is included in GEMM_TLM.h, don't compile separately
IMPL_SOURCES = $(GEMM_SRC_DIR)/GEMM_TLM.cpp

# All source files
ALL_SOURCES = $(MAIN_SRC) $(IMPL_SOURCES)

# Header dependencies (for tracking changes)
HEADERS = matrix_test_utils.h \
          large_matrix_block_control.h \
          $(GEMM_INCLUDE_DIR)/GEMM_TLM.h \
          $(GEMM_INCLUDE_DIR)/PEA.h \
          $(GEMM_INCLUDE_DIR)/pe.h \
          $(GEMM_INCLUDE_DIR)/FIFO.h \
          $(GEMM_INCLUDE_DIR)/in_buf_vec.h \
          $(GEMM_INCLUDE_DIR)/out_buf_vec.h

# Template implementation files (included in headers, no separate compilation needed)
TEMPLATE_IMPL = $(GEMM_SRC_DIR)/PEA.cpp \
                $(GEMM_SRC_DIR)/pe.cpp \
                $(GEMM_SRC_DIR)/FIFO.cpp \
                $(GEMM_SRC_DIR)/in_buf_vec.cpp \
                $(GEMM_SRC_DIR)/out_buf_vec.cpp

# Build targets
all: $(TARGET)

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: CXXFLAGS := $(filter-out $(RELEASE_FLAGS),$(CXXFLAGS))
debug: $(TARGET)

# Main target
$(TARGET): $(ALL_SOURCES) $(HEADERS)
	@echo "🚀 编译GEMM脉动阵列测试程序..."
	@echo "编译器: $(CXX)"
	@echo "SystemC: $(SYSTEMC_HOME)"
	@echo "源文件: $(ALL_SOURCES)"
	@echo "包含路径: $(CXXFLAGS)"
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(ALL_SOURCES) $(LIBS)
	@echo "✅ 编译完成: $(TARGET)"

# Clean up
clean:
	@echo "🧹 清理编译文件..."
	rm -f $(TARGET)
	rm -f core
	rm -f *.o
	@echo "✅ 清理完成"

# Run the test
run: $(TARGET)
	@echo "🎯 运行GEMM测试程序..."
	@echo "========================================="
	./$(TARGET)
	@echo "========================================="
	@echo "✅ 测试运行完成"

# Quick test run with limited output
test: $(TARGET)
	@echo "⚡ 快速测试运行..."
	./$(TARGET)

# Verbose run with all debug output
verbose: $(TARGET)
	@echo "📊 详细测试运行..."
	./$(TARGET) 2>&1 | tee test_output.log
	@echo "📝 详细输出已保存到 test_output.log"

# Check SystemC installation
check-systemc:
	@echo "🔍 检查SystemC安装..."
	@if [ -d "$(SYSTEMC_HOME)" ]; then \
		echo "✅ SystemC目录存在: $(SYSTEMC_HOME)"; \
	else \
		echo "❌ SystemC目录不存在: $(SYSTEMC_HOME)"; \
		exit 1; \
	fi
	@if [ -f "$(INCLUDE_DIR)/systemc.h" ]; then \
		echo "✅ SystemC头文件存在"; \
	else \
		echo "❌ SystemC头文件不存在: $(INCLUDE_DIR)/systemc.h"; \
		exit 1; \
	fi
	@if [ -f "$(LIB_DIR)/libsystemc.so" ] || [ -f "$(LIB_DIR)/libsystemc.a" ]; then \
		echo "✅ SystemC库文件存在"; \
	else \
		echo "❌ SystemC库文件不存在于: $(LIB_DIR)"; \
		exit 1; \
	fi

# Show build info
info:
	@echo "📋 GEMM_SA测试平台构建信息"
	@echo "================================"
	@echo "编译器: $(CXX)"
	@echo "SystemC: $(SYSTEMC_HOME)"
	@echo "目标文件: $(TARGET)"
	@echo "源文件: $(MAIN_SRC)"
	@echo "编译标志: $(CXXFLAGS)"
	@echo "链接标志: $(LDFLAGS) $(LIBS)"
	@echo "================================"

# Install systemc (if needed)
install-systemc:
	@echo "⚠️  请参考SystemC官方安装指南"
	@echo "   https://www.accellera.org/downloads/standards/systemc"

# Help
help:
	@echo "🚀 GEMM_SA测试平台 Makefile"
	@echo "使用方法:"
	@echo "  make            - 编译程序"
	@echo "  make debug      - 编译调试版本"
	@echo "  make run        - 编译并运行测试"
	@echo "  make test       - 快速测试运行"
	@echo "  make verbose    - 详细测试运行"
	@echo "  make clean      - 清理编译文件"
	@echo "  make check-systemc - 检查SystemC安装"
	@echo "  make info       - 显示构建信息"
	@echo "  make help       - 显示此帮助信息"

.PHONY: all debug clean run test verbose check-systemc info install-systemc help
