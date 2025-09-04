/**
 * @file GEMM_TLM.h
 * @brief GEMM_TLM Ultra并行优化版本
 * 
 * 设计目标：
 * - 提供Ultra并行GEMM运算功能
 * - 支持A+B+D三矩阵并行加载
 * - 基于多SC_THREAD架构实现高性能并行处理
 * - 标准TLM2.0协议支持
 */

#ifndef GEMM_TLM_H
#define GEMM_TLM_H

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "PEA.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <iomanip>

using namespace std;

// 🚀 Ultra并行优化：三矩阵并行加载数据结构（支持变长矩阵）
struct parallel_matrix_data {
    float* matrix_A_ptr;
    float* matrix_B_ptr; 
    float* matrix_D_ptr;
    int M, K, N;  // 矩阵尺寸：A[M][K], B[K][N], D[M][N] (支持M≤SIZE, K≤SIZE, N≤SIZE)
    // 🚀 新增：实际矩阵尺寸(用于验证和优化，保持向后兼容)
    int actual_M, actual_K, actual_N;
    
    // 构造函数：默认actual_尺寸等于逻辑尺寸
    parallel_matrix_data() : M(0), K(0), N(0), actual_M(0), actual_K(0), actual_N(0) {}
    parallel_matrix_data(int m, int k, int n) : M(m), K(k), N(n), actual_M(m), actual_K(k), actual_N(n) {}
};

// 🚀 双缓冲流水线配置结构
struct PipelineConfig {
    // 缓冲区配置
    int buffer_count;           // 缓冲区数量 (2=双缓冲, 3=三缓冲, 4=四缓冲)
    bool enable_pipeline_mode;  // 是否启用流水线模式
    
    // 并行度控制
    int max_parallel_frames;    // 最大并行处理帧数
    int pipeline_depth;         // 流水线深度 (阶段数)
    
    // 性能优化参数
    double load_balance_factor; // 负载均衡因子 (0.0-1.0)
    bool enable_overlap_opt;    // 是否启用重叠优化
    bool enable_prefetch;       // 是否启用预取优化
    
    // 调试和监控
    bool enable_detailed_stats; // 是否启用详细统计
    bool enable_debug_trace;    // 是否启用调试跟踪
    int trace_verbosity;        // 跟踪详细程度 (0-3)
    
    // 构造函数：默认双缓冲配置
    PipelineConfig() :
        buffer_count(2),
        enable_pipeline_mode(false),
        max_parallel_frames(2),
        pipeline_depth(3),  // Load -> Compute -> Read
        load_balance_factor(1.0),
        enable_overlap_opt(true),
        enable_prefetch(false),
        enable_detailed_stats(true),
        enable_debug_trace(false),
        trace_verbosity(1) {}
    
    // 预定义配置模式
    static PipelineConfig get_dual_buffer_config() {
        PipelineConfig config;
        config.buffer_count = 2;
        config.max_parallel_frames = 2;
        config.enable_pipeline_mode = true;
        return config;
    }
    
    static PipelineConfig get_triple_buffer_config() {
        PipelineConfig config;
        config.buffer_count = 3;
        config.max_parallel_frames = 3;
        config.enable_pipeline_mode = true;
        config.enable_prefetch = true;
        return config;
    }
    
    static PipelineConfig get_high_performance_config() {
        PipelineConfig config;
        config.buffer_count = 4;
        config.max_parallel_frames = 4;
        config.enable_pipeline_mode = true;
        config.enable_overlap_opt = true;
        config.enable_prefetch = true;
        config.load_balance_factor = 0.8; // 稍微保守的负载均衡
        return config;
    }
    
    // 配置验证
    bool validate() const {
        if (buffer_count < 2 || buffer_count > 8) return false;
        if (max_parallel_frames < 1 || max_parallel_frames > buffer_count) return false;
        if (pipeline_depth < 2 || pipeline_depth > 5) return false;
        if (load_balance_factor < 0.0 || load_balance_factor > 1.0) return false;
        if (trace_verbosity < 0 || trace_verbosity > 3) return false;
        return true;
    }
    
    // 打印配置信息
    void print_config() const {
        cout << "🚀 Pipeline Configuration:" << endl;
        cout << "  ├─ Buffer Count: " << buffer_count << endl;
        cout << "  ├─ Pipeline Mode: " << (enable_pipeline_mode ? "Enabled" : "Disabled") << endl;
        cout << "  ├─ Max Parallel Frames: " << max_parallel_frames << endl;
        cout << "  ├─ Pipeline Depth: " << pipeline_depth << endl;
        cout << "  ├─ Load Balance Factor: " << load_balance_factor << endl;
        cout << "  ├─ Overlap Optimization: " << (enable_overlap_opt ? "Enabled" : "Disabled") << endl;
        cout << "  ├─ Prefetch: " << (enable_prefetch ? "Enabled" : "Disabled") << endl;
        cout << "  ├─ Detailed Stats: " << (enable_detailed_stats ? "Enabled" : "Disabled") << endl;
        cout << "  └─ Debug Trace: " << (enable_debug_trace ? "Enabled" : "Disabled") << endl;
    }
};

// 🚀 Ultra延时统计结构
struct UltraTimingStats {
    // === 基础顺序执行统计 ===
    sc_time load_start_time;      // 加载开始时间戳
    sc_time compute_start_time;   // 计算开始时间戳  
    sc_time read_start_time;      // 读取开始时间戳
    
    sc_time load_hardware_time;   // 硬件加载实际耗时
    sc_time compute_hardware_time;// PE计算实际耗时
    sc_time read_hardware_time;   // 读取实际耗时
    
    sc_time tlm_overhead_time;    // TLM通信开销
    sc_time total_execution_time; // 总执行时间
    
    // === 🚀 双缓冲并行统计扩展 ===
    sc_time pipeline_stage_time;     // 流水线单阶段时间 = max(load, compute, read)
    sc_time pipeline_startup_latency;// 流水线启动延时 = load + compute + read
    sc_time pipeline_steady_latency; // 稳态延时 = max(load, compute, read)
    
    double overlap_efficiency;       // 重叠效率 = (顺序时间 - 双缓冲时间) / 顺序时间 * 100%
    double pipeline_utilization;     // 流水线利用率 = 计算时间 / 流水线阶段时间 * 100%
    double throughput_improvement;   // 吞吐率提升 = 顺序时间 / 双缓冲阶段时间
    
    // 多帧处理统计
    int processed_frame_count;       // 已处理帧数
    sc_time multi_frame_total_time;  // 多帧总时间
    sc_time average_frame_latency;   // 平均每帧延时
    
    // 🚀 新增：变长矩阵性能统计
    double pe_utilization;           // PE利用率 = (有效PE数 / 总PE数) * 100%
    double memory_efficiency;        // 内存传输效率 = (实际传输元素 / 理论最大元素) * 100%
    int actual_matrix_M, actual_matrix_K, actual_matrix_N;  // 实际矩阵尺寸
    int total_pe_count;              // 总PE数量
    int effective_pe_count;          // 有效PE数量
    
    // 构造函数初始化
    UltraTimingStats() : 
        load_start_time(sc_time(0, SC_NS)),
        compute_start_time(sc_time(0, SC_NS)),
        read_start_time(sc_time(0, SC_NS)),
        load_hardware_time(sc_time(0, SC_NS)),
        compute_hardware_time(sc_time(0, SC_NS)),
        read_hardware_time(sc_time(0, SC_NS)),
        tlm_overhead_time(sc_time(0, SC_NS)),
        total_execution_time(sc_time(0, SC_NS)),
        // 双缓冲字段初始化
        pipeline_stage_time(sc_time(0, SC_NS)),
        pipeline_startup_latency(sc_time(0, SC_NS)),
        pipeline_steady_latency(sc_time(0, SC_NS)),
        overlap_efficiency(0.0),
        pipeline_utilization(0.0),
        throughput_improvement(1.0),
        processed_frame_count(0),
        multi_frame_total_time(sc_time(0, SC_NS)),
        average_frame_latency(sc_time(0, SC_NS)),
        // 🚀 新增字段初始化
        pe_utilization(0.0),
        memory_efficiency(0.0),
        actual_matrix_M(0), actual_matrix_K(0), actual_matrix_N(0),
        total_pe_count(0),
        effective_pe_count(0) {}
        
    // 重置所有时间统计
    void reset() {
        *this = UltraTimingStats();
    }
    
    // 计算总执行时间
    void calculate_total_time() {
        total_execution_time = load_hardware_time + compute_hardware_time + read_hardware_time + tlm_overhead_time;
    }
    
    // 🚀 计算双缓冲流水线延时
    void calculate_pipeline_timing() {
        // 流水线阶段时间 = 最长的单个阶段时间
        pipeline_stage_time = sc_time(std::max({
            load_hardware_time.to_double(),
            compute_hardware_time.to_double(), 
            read_hardware_time.to_double()
        }), SC_NS);
        
        // 启动延时 = 所有阶段顺序执行时间
        pipeline_startup_latency = load_hardware_time + compute_hardware_time + read_hardware_time;
        
        // 稳态延时 = 流水线阶段时间
        pipeline_steady_latency = pipeline_stage_time;
        
        // 重叠效率计算
        if (total_execution_time > sc_time(0, SC_NS)) {
            double sequential_time = total_execution_time.to_double();
            double pipeline_time = pipeline_stage_time.to_double();
            overlap_efficiency = ((sequential_time - pipeline_time) / sequential_time) * 100.0;
        }
        
        // 流水线利用率 = PE计算时间 / 流水线阶段时间
        if (pipeline_stage_time > sc_time(0, SC_NS)) {
            pipeline_utilization = (compute_hardware_time.to_double() / pipeline_stage_time.to_double()) * 100.0;
        }
        
        // 吞吐率提升 = 顺序时间 / 流水线阶段时间
        if (pipeline_stage_time > sc_time(0, SC_NS)) {
            throughput_improvement = total_execution_time.to_double() / pipeline_stage_time.to_double();
        }
    }
    
    // 🚀 多帧处理统计更新
    void update_multi_frame_stats(int frame_count, sc_time total_time) {
        processed_frame_count = frame_count;
        multi_frame_total_time = total_time;
        if (frame_count > 0) {
            average_frame_latency = sc_time(total_time.to_double() / frame_count, SC_NS);
        }
    }
    
    // 🚀 新增：计算变长矩阵性能统计
    void calculate_variable_matrix_stats(int M, int K, int N, int pe_array_size) {
        actual_matrix_M = M;
        actual_matrix_K = K; 
        actual_matrix_N = N;
        
        total_pe_count = pe_array_size * pe_array_size;
        effective_pe_count = M * N;  // 结果矩阵C的尺寸
        
        // PE利用率 = 有效PE数 / 总PE数
        if (total_pe_count > 0) {
            pe_utilization = (double)effective_pe_count / total_pe_count * 100.0;
        }
        
        // 内存传输效率 = 实际传输元素 / 理论最大元素
        int actual_elements = M * K + K * N + M * N + M * N;  // A + B + D + C
        int theoretical_elements = 4 * pe_array_size * pe_array_size;  // 4个满矩阵
        if (theoretical_elements > 0) {
            memory_efficiency = (double)actual_elements / theoretical_elements * 100.0;
        }
    }
    
    // 🚀 新增：打印变长矩阵性能报告
    void print_variable_matrix_stats() const {
        cout << "\n📊 变长矩阵性能统计报告:" << endl;
        cout << "  ├─ 实际矩阵尺寸: A[" << actual_matrix_M << "×" << actual_matrix_K 
             << "] × B[" << actual_matrix_K << "×" << actual_matrix_N << "] = C[" 
             << actual_matrix_M << "×" << actual_matrix_N << "]" << endl;
        cout << "  ├─ PE阵列信息: " << total_pe_count << " 总PEs, " << effective_pe_count << " 有效PEs" << endl;
        cout << "  ├─ PE利用率: " << std::fixed << std::setprecision(1) << pe_utilization << "%" << endl;
        cout << "  ├─ 内存传输效率: " << std::fixed << std::setprecision(1) << memory_efficiency << "%" << endl;
        cout << "  ├─ 计算密度: " << (effective_pe_count > 0 ? (actual_matrix_K / (double)effective_pe_count) : 0.0) << " 乘法/PE" << endl;
        cout << "  └─ 数据重用率: " << (actual_matrix_K > 1 ? ((double)actual_matrix_K - 1) / actual_matrix_K * 100.0 : 0.0) << "%" << endl;
    }
};

// GEMM操作类型枚举
enum class gemm_operation_t {
    // === 基础操作 ===
    LOAD_ALL_MATRICES,      // 🚀 Ultra并行优化：并行加载A+B+D三个矩阵
    START_COMPUTE,          // 开始计算
    READ_MATRIX_C,          // 读取结果矩阵C
    GET_STATUS,             // 查询状态
    RESET_MODULE,           // 模块复位
    
    // === 🚀 双缓冲流水线操作扩展 ===
    CONFIGURE_PIPELINE,     // 配置流水线参数
    ENABLE_PIPELINE_MODE,   // 启用双缓冲流水线模式
    PROCESS_MULTI_FRAMES,   // 批量多帧处理
    GET_PIPELINE_STATS,     // 获取流水线性能统计
};

// TLM扩展包
struct gemm_payload_extension : public tlm::tlm_extension<gemm_payload_extension> {
    gemm_operation_t operation;
    int matrix_row, matrix_col;
    float* data_ptr;
    bool blocking_mode;
    
    gemm_payload_extension() : 
        operation(gemm_operation_t::GET_STATUS), 
        matrix_row(0), matrix_col(0), 
        data_ptr(nullptr), blocking_mode(true) {}
    
    virtual tlm::tlm_extension_base* clone() const {
        gemm_payload_extension* ext = new gemm_payload_extension();
        ext->operation = this->operation;
        ext->matrix_row = this->matrix_row;
        ext->matrix_col = this->matrix_col;
        ext->data_ptr = this->data_ptr;
        ext->blocking_mode = this->blocking_mode;
        return ext;
    }
    
    virtual void copy_from(tlm::tlm_extension_base const& ext) {
        const gemm_payload_extension& src = static_cast<const gemm_payload_extension&>(ext);
        this->operation = src.operation;
        this->matrix_row = src.matrix_row;
        this->matrix_col = src.matrix_col;
        this->data_ptr = src.data_ptr;
        this->blocking_mode = src.blocking_mode;
    }
};

template<typename T = float, int SIZE = 4>
SC_MODULE(GEMM_TLM) {
    // TLM目标接口
    tlm_utils::simple_target_socket<GEMM_TLM> target_socket;
    
    // 内部时钟和复位信号
    sc_clock clk{"clk", 10, SC_NS};
    sc_signal<bool> rst{"rst"};
    
    // 嵌入的PEA实例
    PEA<T, SIZE, 32> *pea_core;
    
    // 内部矩阵缓冲区
    T matrix_A_buffer[SIZE][SIZE];
    T matrix_B_buffer[SIZE][SIZE];
    T matrix_D_buffer[SIZE][SIZE];
    T matrix_C_buffer[SIZE][SIZE];
    
    // 状态机枚举
    enum state_t { 
        IDLE,                    // 空闲状态
        LOADING_PARALLEL,        // 并行加载状态
        COMPUTING,               // 计算中
        RESULT_READY,            // 结果就绪
        ERROR_STATE,             // 错误状态
        // 🚀 双缓冲流水线状态扩展
        PIPELINE_LOADING,        // 流水线加载阶段
        PIPELINE_COMPUTING,      // 流水线计算阶段  
        PIPELINE_READING,        // 流水线读取阶段
        PIPELINE_MULTI_FRAME,    // 多帧流水线处理
        PIPELINE_SWITCHING,      // 流水线缓冲区切换
        PIPELINE_FINALIZING      // 流水线结束处理
    };
    
    state_t current_state;
    
    // 内部连接信号
    // 权重加载接口(A矩阵)
    sc_vector<sc_vector<sc_signal<T>>> w_data_sig{"w_data_sig", SIZE};
    sc_signal<bool> w_load_start_sig{"w_load_start_sig"};
    sc_signal<bool> w_load_en_sig{"w_load_en_sig"};
    sc_signal<bool> w_load_done_sig{"w_load_done_sig"};
    
    // B矩阵输入接口
    sc_vector<sc_signal<T>> b_data_sig{"b_data_sig", SIZE};
    sc_signal<bool> b_wr_start_sig{"b_wr_start_sig"};
    sc_signal<bool> b_wr_en_sig{"b_wr_en_sig"};
    sc_vector<sc_signal<bool>> b_wr_ready_sig{"b_wr_ready_sig", SIZE};
    
    // D矩阵输入接口
    sc_vector<sc_signal<T>> d_data_sig{"d_data_sig", SIZE};
    sc_signal<bool> d_wr_start_sig{"d_wr_start_sig"};
    sc_signal<bool> d_wr_en_sig{"d_wr_en_sig"};
    sc_vector<sc_signal<bool>> d_wr_ready_sig{"d_wr_ready_sig", SIZE};
    
    // 计算控制接口
    sc_signal<bool> compute_start_sig{"compute_start_sig"};
    sc_signal<bool> compute_done_sig{"compute_done_sig"};
    
    // C矩阵输出接口
    sc_vector<sc_signal<bool>> c_rd_start_sig{"c_rd_start_sig", SIZE};
    sc_vector<sc_signal<T>> c_data_sig{"c_data_sig", SIZE};
    sc_vector<sc_signal<bool>> c_valid_sig{"c_valid_sig", SIZE};
    sc_vector<sc_signal<bool>> c_ready_sig{"c_ready_sig", SIZE};
    
    // 🚀 新增：矩阵尺寸信号（传递给PEA模块）
    sc_signal<int> matrix_M_sig{"matrix_M_sig"};
    sc_signal<int> matrix_N_sig{"matrix_N_sig"};
    sc_signal<int> matrix_K_sig{"matrix_K_sig"};
    
    // 🚀 Ultra并行优化：全局矩阵指针
    T* global_A_ptr = nullptr;
    T* global_B_ptr = nullptr;
    T* global_D_ptr = nullptr;
    int matrix_M, matrix_K, matrix_N;//矩阵尺寸
    
    // 🚀 Ultra并行优化：线程完成标志位
    bool load_A_finished = false;
    bool load_B_finished = false; 
    bool load_D_finished = false;
    
    // 事件通知
    sc_event reset_trigger_event;
    sc_event computation_done_event;
    sc_event error_occurred_event;
    
    // 🚀 简化并行事件控制
    sc_event load_A_start, load_A_complete;
    sc_event load_B_start, load_B_complete;
    sc_event load_D_start, load_D_complete;
    sc_event all_matrices_loaded;
    
    // 内部控制变量
    bool computation_complete;
    sc_time total_computation_time;
    
    // 🚀 连续检测compute_done_sig状态变量
    bool compute_done_prev = false;
    bool compute_done_double_checked = false;
    
    // 互斥锁保护访问
    sc_mutex access_mutex;

    // 🚀 Ultra延时统计相关
    UltraTimingStats current_timing_stats;
    sc_time operation_start_timestamp;
    bool enable_detailed_timing = true;
    int computation_count = 0;

    // 🚀 双缓冲流水线相关
    PipelineConfig pipeline_config;          // 流水线配置参数
    bool pipeline_mode_enabled = false;      // 流水线模式开关
    int current_pipeline_stage = 0;          // 当前流水线阶段 (0=Load, 1=Compute, 2=Read)
    
    // 多帧处理状态
    int total_frames_to_process = 1;         // 总处理帧数
    int current_frame_index = 0;             // 当前处理帧索引
    std::vector<UltraTimingStats> frame_stats_history; // 各帧统计历史
    
    // 流水线缓冲区管理
    struct FrameBuffer {
        T matrix_A[SIZE][SIZE];
        T matrix_B[SIZE][SIZE]; 
        T matrix_D[SIZE][SIZE];
        T matrix_C[SIZE][SIZE];
        bool buffer_ready = false;
        bool buffer_in_use = false;
        int frame_id = -1;
    };
    std::vector<FrameBuffer> pipeline_buffers; // 流水线缓冲区
    
    // 流水线事件和同步
    sc_event pipeline_stage_complete[3];     // 各阶段完成事件 [Load, Compute, Read]
    sc_event pipeline_frame_complete;        // 单帧流水线完成事件
    sc_event multi_frame_complete;           // 多帧处理完成事件
    sc_mutex pipeline_mutex;                 // 流水线互斥锁

    SC_HAS_PROCESS(GEMM_TLM);
    
    GEMM_TLM(sc_module_name name);
    
    // 基础功能函数
    void connect_pea_signals();
    void initialize_matrix_buffers();
    void initialize_all_signals();
    void state_machine_control();
    void monitor_computation();
    void reset_sequence();
    
    // 🚀 优化：矩阵验证通用函数
    bool validate_matrix_dimensions(int M, int K, int N, const char* context = "");
    
    // TLM阻塞传输接口
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
    
    // 🚀 Ultra并行控制线程
    void load_A_thread();
    void load_B_thread();
    void load_D_thread();
    
    // 🚀 优化：加载线程通用模板函数
    template<typename LoadFunc>
    void generic_load_thread(sc_event& start_event, sc_event& complete_event, 
                           bool& finished_flag, LoadFunc load_function, const char* thread_name);
    
    // 🚀 Ultra并行GEMM计算API
    sc_time ultra_gemm_compute(T A[SIZE][SIZE], T B[SIZE][SIZE], T D[SIZE][SIZE], T C[SIZE][SIZE]);
    
    // 🚀 基础串行API（兼容性）
    sc_time load_matrix_A(T A[SIZE][SIZE]);
    sc_time load_matrix_B(T B[SIZE][SIZE]);
    sc_time load_matrix_D(T D[SIZE][SIZE]);
    sc_time gemm_compute(T A[SIZE][SIZE], T B[SIZE][SIZE], T D[SIZE][SIZE], T C[SIZE][SIZE]);
    
    // 辅助函数
    sc_time compute_gemm();
    sc_time read_result_C(T C[SIZE][SIZE]);
    bool is_ready();
    bool is_computing();
    
    // 🚀 Ultra延时统计方法 (Ultra-Enhanced)
    UltraTimingStats get_timing_stats() const;

    // 🚀 双缓冲流水线方法
    void configure_pipeline(const PipelineConfig& config);
    bool enable_pipeline_mode();
    
    // 🚀 核心流水线算法
    void calculate_pipeline_timing();
    sc_time simulate_multi_frame_execution(int frame_count);
    void analyze_overlap_potential();
    
    // 🚀 流水线统计和报告
    UltraTimingStats get_pipeline_stats() const;

    
    // 🚀 优化：延迟时间常量定义（修复constexpr问题）
    static const sc_time DEFAULT_DELAY;
    static const sc_time COMPUTE_EXTRA_DELAY;
    static const sc_time RESET_DELAY;
    
    // 析构函数
    ~GEMM_TLM() {
        delete pea_core;
    }
};
#include "../src/pipeline_simulation.cpp"
#endif // GEMM_TLM_H