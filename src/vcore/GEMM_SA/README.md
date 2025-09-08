# GEMM脉动阵列系统架构说明

## 🏗️ 脉动阵列拓扑结构

```
            FIFO_H[0]    FIFO_H[1]     FIFO_H[2]    ....   FIFO_H[n-1]
                 ↓            ↓            ↓                    ↓
FIFO_V[0] → PE[0][0]  →   PE[0][1]  →   PE[0][2] → ....   PE[0][n-1] → 丢弃
                 ↓            ↓            ↓                    ↓
FIFO_V[1] → PE[1][0]  →   PE[1][1]  →   PE[1][2] → ....   PE[1][n-1] → 丢弃
                 ↓            ↓            ↓                    ↓
FIFO_V[2] → PE[2][0]  →   PE[2][1]  →   PE[2][2] → ....   PE[2][n-1] → 丢弃
                 ↓            ↓            ↓                    ↓
                 .            .            .                    .
                 .            .            .                    .
                 .            .            .                    .
                 ↓            ↓            ↓                    ↓
FIFO_V[n-1] → PE[n-1][0] → PE[n-1][1] → PE[n-1][2] → ....   PE[n-1][n-1] → 丢弃
                 ↓            ↓              ↓                  ↓
            FIFO_O[0]      FIFO_O[1]     FIFO_O[2]    ....   FIFO_O[n-1]
```

## 🧮 数学计算模型

**核心计算公式**：
$$C_{ij} = \left( \sum_{k=1}^{n} A_{ik} B_{kj} \right) + D_{ij}$$

**计算目标**：`C = A×B + D`

## 🔄 脉动阵列计算工作流程

### 阶段1：PE阵列权重预加载
- 按列加载，每个时钟周期加载一列的PE内部权重
- A[i][j] → PE[j][i] (矩阵转置映射)

### 阶段2：输入数据准备
- 外部向FIFO_V[:]加载矩阵B
- FIFO_V[i] ← B[i][:] (按行加载)

### 阶段3：初始值准备  
- 外部向FIFO_H[:]加载矩阵D
- FIFO_H[i] ← D[i][:] (按行加载)

### 阶段4：计算启动
- 外部发出compute_start_i信号
- FIFO_V和FIFO_H形成"平行四边形"数据流模式
- 数据按对角线顺序流动

### 阶段5：PE阵列计算
- 各PE进行乘累加运算：`MAC = A×B + accumulator`

### 阶段6：结果收集
- FIFO_O[j]收集C矩阵的第j列数据
- 按对角线时序输出结果

### 阶段7：计算完成
- 监控最后一个FIFO_O数据完毕
- 从FIFO_O[:]中读出完整结果矩阵

## 📋 矩阵映射关系

| 矩阵 | 存储映射 | 说明 |
|------|----------|------|
| A矩阵 | A[i][j] → PE[j][i] | 转置映射，支持矩阵乘法 |
| B矩阵 | B[i][:] → FIFO_V[i] | 按行存储到垂直FIFO |
| D矩阵 | D[i][:] → FIFO_H[i] | 按行存储到水平FIFO |
| C矩阵 | C[:][j] → FIFO_O[j] | 按列从输出FIFO收集 |

---

# 🚀 GEMM_TLM模块接口和功能调用指南

## 📡 TLM2.0双向通信接口

### Socket接口
```cpp
// 命令接收接口
tlm_utils::simple_target_socket<GEMM_TLM> target_socket;

// 🚀 通知发送接口（新增）
tlm_utils::simple_initiator_socket<GEMM_TLM> initiator_socket;
```

### 连接示例
```cpp
// 双向socket连接
gemm_initiator->initiator_socket.bind(gemm_module->target_socket);      // 命令通道
gemm_module->initiator_socket.bind(gemm_initiator->notification_socket); // 通知通道
```

## 🎯 核心操作命令

### 1. 模块复位
```cpp
// 操作：RESET_MODULE
// 用途：初始化所有内部状态和缓冲区
// 延时：10ns
send_tlm_command(gemm_operation_t::RESET_MODULE);
```

### 2. 矩阵加载
```cpp
// 操作：LOAD_ALL_MATRICES  
// 用途：并行加载A、B、D三个矩阵
// 支持：变长矩阵(1×1到16×16)
parallel_matrix_data matrix_data;
matrix_data.matrix_A_ptr = A;
matrix_data.matrix_B_ptr = B; 
matrix_data.matrix_D_ptr = D;
matrix_data.M = M; matrix_data.K = K; matrix_data.N = N;

send_tlm_command(gemm_operation_t::LOAD_ALL_MATRICES, 
                reinterpret_cast<uint8_t*>(&matrix_data),
                sizeof(parallel_matrix_data));
```

### 3. 计算启动
```cpp
// 操作：START_COMPUTE
// 用途：启动脉动阵列计算
// 特性：自动状态机管理，支持完成检测
send_tlm_command(gemm_operation_t::START_COMPUTE);
```

### 4. 结果读取
```cpp
// 操作：READ_MATRIX_C
// 用途：读取计算结果矩阵C
// 输出：存储到指定内存地址
send_tlm_command(gemm_operation_t::READ_MATRIX_C,
                reinterpret_cast<uint8_t*>(result_C),
                sizeof(float) * M * N,
                tlm::TLM_READ_COMMAND);
```

## 🚀 流水线性能分析功能

### 1. 配置流水线参数
```cpp
// 操作：CONFIGURE_PIPELINE
PipelineConfig config = PipelineConfig::get_dual_buffer_config();
config.enable_detailed_stats = true;
config.enable_debug_trace = true;

send_tlm_command(gemm_operation_t::CONFIGURE_PIPELINE,
                reinterpret_cast<uint8_t*>(&config),
                sizeof(PipelineConfig));
```

### 2. 启用流水线模式
```cpp
// 操作：ENABLE_PIPELINE_MODE
send_tlm_command(gemm_operation_t::ENABLE_PIPELINE_MODE);
```

### 3. 多帧流水线分析
```cpp
// 操作：PROCESS_MULTI_FRAMES
// 用途：分析多帧数据的流水线性能
int frame_count = 343;
send_tlm_command(gemm_operation_t::PROCESS_MULTI_FRAMES,
                reinterpret_cast<uint8_t*>(&frame_count),
                sizeof(int));
```

### 4. 获取性能统计
```cpp
// 操作：GET_PIPELINE_STATS  
// 返回：UltraTimingStats性能数据
UltraTimingStats stats;
send_tlm_command(gemm_operation_t::GET_PIPELINE_STATS,
                reinterpret_cast<uint8_t*>(&stats),
                sizeof(UltraTimingStats),
                tlm::TLM_READ_COMMAND);
```

## 🎉 异步通知机制

### 1. 通知触发条件
- **计算完成**：每次GEMM计算完成时自动发送
- **状态转换**：从COMPUTING→RESULT_READY时触发
- **魔法数字**：0x12345678作为通知标识

### 2. 通知接收处理
```cpp
void notification_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    uint32_t* notification_data = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    if (*notification_data == 0x12345678) {
        cout << "🎉 接收到计算完成通知！" << endl;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    
    delay = sc_time(1, SC_NS);
}
```

### 3. 通知日志示例
```
880 ns: [GEMM_TLM-Notification] 发送计算完成通知
880 ns: [TLM-Notification] 🎉 接收到计算完成通知！
880 ns: [TLM-Notification] 魔法数字: 0x12345678
880 ns: [GEMM_TLM-Notification] ✅ 通知发送成功
```

## 📊 支持的矩阵规模

### 单帧模式
- **范围**：1×1 到 16×16
- **特点**：直接计算，无分块
- **性能**：最佳PE利用率

### 大矩阵分块模式
- **范围**：超过16×16的任意尺寸
- **策略**：5种智能分块算法
  - 行分块 (M > 16)
  - 列分块 (N > 16)  
  - 内维分块 (K > 16，需累加)
  - 2D分块 (M,N > 16)
  - 3D分块 (M,K,N > 16)
- **示例**：100×100矩阵 → 7×7×7 = 343个16×16分块

## ⚡ 使用示例

### 简单GEMM计算
```cpp
// 1. 复位模块
send_tlm_command(gemm_operation_t::RESET_MODULE);

// 2. 加载矩阵数据
send_matrix_commands(A, B, D, M, K, N);

// 3. 启动计算
send_tlm_command(gemm_operation_t::START_COMPUTE);

// 4. 等待通知 (异步)
// → 自动接收计算完成通知

// 5. 读取结果
send_tlm_command(gemm_operation_t::READ_MATRIX_C, C, M*N*sizeof(float));
```

### 流水线性能分析
```cpp
// 1. 配置流水线参数
PipelineConfig config = PipelineConfig::get_dual_buffer_config();
send_tlm_command(gemm_operation_t::CONFIGURE_PIPELINE, &config);

// 2. 启用流水线模式
send_tlm_command(gemm_operation_t::ENABLE_PIPELINE_MODE);

// 3. 多帧分析
send_tlm_command(gemm_operation_t::PROCESS_MULTI_FRAMES, &frame_count);

// 4. 获取性能报告
UltraTimingStats stats = get_pipeline_stats();
```

## 🔧 错误处理

### TLM响应状态
- `TLM_OK_RESPONSE`：操作成功
- `TLM_GENERIC_ERROR_RESPONSE`：一般错误
- `TLM_INCOMPLETE_RESPONSE`：操作未完成

### 异常处理示例
```cpp
try {
    initiator_socket->b_transport(trans, delay);
    if (trans.is_response_ok()) {
        // 操作成功
    } else {
        // 处理错误
    }
} catch (const std::exception& e) {
    // 异常处理
}
```