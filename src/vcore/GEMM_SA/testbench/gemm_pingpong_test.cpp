/**
 * @file gemm_pingpong_test.cpp
 * @brief GEMM_TLM简化验证测试 - Think Ultra版本
 * 
 * 测试目标：
 * 1. 智能判断矩阵大小，自动选择单帧或分块模式
 * 2. 简化架构，删除冗余的模式选择逻辑
 * 3. 统一TLM接口，提供一致的测试体验
 */

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "GEMM_TLM.h"
#include "matrix_test_utils.h"
#include "large_matrix_block_control.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>

using namespace std;

const int PEA_SIZE = 16;

// 🚀 Think Ultra 简化配置
const int DEFAULT_M = 100;
const int DEFAULT_K = 100;
const int DEFAULT_N = 100;


// 🚀 TLM2.0标准Initiator - 通过transaction与GEMM_TLM通信
SC_MODULE(GEMM_TLM_INITIATOR) {
    tlm_utils::simple_initiator_socket<GEMM_TLM_INITIATOR> initiator_socket;
    
    // 🚀 新增：接收通知的target socket
    tlm_utils::simple_target_socket<GEMM_TLM_INITIATOR> notification_socket;

    SC_CTOR(GEMM_TLM_INITIATOR) : 
        initiator_socket("initiator_socket"),
        notification_socket("notification_socket") {
        // 注册通知接收回调
        notification_socket.register_b_transport(this, &GEMM_TLM_INITIATOR::notification_b_transport);
    }
    
    // 🚀 统一GEMM执行函数
    sc_time execute_gemm_via_tlm(float* A, float* B, float* D, float* C, int M, int K, int N) {
        sc_time total_delay = sc_time(0, SC_NS);
        
        cout << sc_time_stamp() << ": [TLM] 开始执行GEMM: A[" << M << "×" << K << "] × B[" << K << "×" << N << "] + D[" << M << "×" << N << "]" << endl;
        
        // 矩阵加载
        total_delay += send_matrix_commands(A, B, D, M, K, N);
        
        // 启动计算
        total_delay += send_compute_command();
        
        // 读取结果
        total_delay += send_read_command(C, M, N);
        
        cout << sc_time_stamp() << ": [TLM] GEMM完成，总耗时: " << total_delay << endl;
        return total_delay;
    }
    
    // 🚀 统一矩阵加载函数
    sc_time send_matrix_commands(float* A, float* B, float* D, int M, int K, int N) {
        cout << sc_time_stamp() << ": [TLM] 开始矩阵加载 A[" << M << "×" << K << "] B[" << K << "×" << N << "] D[" << M << "×" << N << "]" << endl;
        
        // 准备矩阵数据结构
        parallel_matrix_data matrix_data;
        matrix_data.matrix_A_ptr = A;
        matrix_data.matrix_B_ptr = B;
        matrix_data.matrix_D_ptr = D;
        matrix_data.M = M;
        matrix_data.K = K;
        matrix_data.N = N;
        matrix_data.actual_M = M;
        matrix_data.actual_K = K;
        matrix_data.actual_N = N;
        
        sc_time delay = send_tlm_command(gemm_operation_t::LOAD_ALL_MATRICES, 
                                       reinterpret_cast<uint8_t*>(&matrix_data),
                                       sizeof(parallel_matrix_data),
                                       tlm::TLM_WRITE_COMMAND);
        
        cout << sc_time_stamp() << ": [TLM] 矩阵加载完成，耗时: " << delay << endl;
        return delay;
    }
    
private:
    // 🚀 通用TLM命令发送函数
    sc_time send_tlm_command(gemm_operation_t operation, 
                            uint8_t* data_ptr = nullptr, 
                            size_t data_length = 0,
                            tlm::tlm_command cmd = tlm::TLM_WRITE_COMMAND,
                            sc_time extra_delay = sc_time(0, SC_NS)) {
        
        tlm::tlm_generic_payload payload;
        gemm_payload_extension* ext = new gemm_payload_extension();
        sc_time delay = sc_time(10, SC_NS);
        
        // 设置操作类型和数据
        ext->operation = operation;
        payload.set_extension(ext);
        payload.set_command(cmd);
        
        if (data_ptr) {
            payload.set_data_ptr(data_ptr);
            payload.set_data_length(data_length);
        } else {
            payload.set_data_length(0);
        }
        
        // 发送TLM事务
        initiator_socket->b_transport(payload, delay);
        
        // 清理资源
        payload.clear_extension(ext);
        delete ext;
        
        return delay + extra_delay;
    }
    
    sc_time send_compute_command() {
        cout << sc_time_stamp() << ": [TLM] 发送计算启动命令" << endl;
        
        sc_time delay = send_tlm_command(gemm_operation_t::START_COMPUTE, nullptr, 0, 
                                       tlm::TLM_WRITE_COMMAND, sc_time(100, SC_NS));
        
        cout << sc_time_stamp() << ": [TLM] 等待计算完成..." << endl;
        return delay;
    }
    
    sc_time send_read_command(float* C, int M, int N) {
        cout << sc_time_stamp() << ": [TLM] 发送结果读取命令" << endl;
        
        sc_time delay = send_tlm_command(gemm_operation_t::READ_MATRIX_C, 
                                       reinterpret_cast<uint8_t*>(C), 
                                       sizeof(float) * M * N, 
                                       tlm::TLM_READ_COMMAND);
        
        cout << sc_time_stamp() << ": [TLM] 结果读取命令完成" << endl;
        return delay;
    }

public:
    // 🚀 通过TLM安全重置模块状态
    sc_time send_reset_command() {
        cout << sc_time_stamp() << ": [TLM] 发送模块重置命令" << endl;
        
        sc_time delay = send_tlm_command(gemm_operation_t::RESET_MODULE);
        
        cout << sc_time_stamp() << ": [TLM] 模块重置命令完成" << endl;
        return delay;
    }
    
    // 🚀 多帧流水线性能分析
    sc_time process_multi_frames(int frame_count) {
        cout << sc_time_stamp() << ": [TLM] 开始多帧流水线性能分析，帧数: " << frame_count << endl;
        
        sc_time delay = send_tlm_command(gemm_operation_t::PROCESS_MULTI_FRAMES, 
                                       reinterpret_cast<uint8_t*>(&frame_count),
                                       sizeof(int),
                                       tlm::TLM_WRITE_COMMAND);
        
        cout << sc_time_stamp() << ": [TLM] 多帧流水线分析完成，耗时: " << delay << endl;
        return delay;
    }
    
    // 🚀 获取流水线统计数据
    UltraTimingStats get_pipeline_stats() {
        cout << sc_time_stamp() << ": [TLM] 获取流水线统计数据" << endl;
        
        UltraTimingStats stats;
        sc_time delay = send_tlm_command(gemm_operation_t::GET_PIPELINE_STATS,
                                       reinterpret_cast<uint8_t*>(&stats),
                                       sizeof(UltraTimingStats),
                                       tlm::TLM_READ_COMMAND);
        
        cout << sc_time_stamp() << ": [TLM] 流水线统计数据获取完成" << endl;
        return stats;
    }
    
    // 🚀 配置流水线参数
    sc_time configure_pipeline(const PipelineConfig& config) {
        cout << sc_time_stamp() << ": [TLM] 配置流水线参数" << endl;
        
        sc_time delay = send_tlm_command(gemm_operation_t::CONFIGURE_PIPELINE,
                                       reinterpret_cast<uint8_t*>(const_cast<PipelineConfig*>(&config)),
                                       sizeof(PipelineConfig),
                                       tlm::TLM_WRITE_COMMAND);
        
        cout << sc_time_stamp() << ": [TLM] 流水线配置完成，耗时: " << delay << endl;
        return delay;
    }
    
    // 🚀 启用流水线模式
    sc_time enable_pipeline_mode() {
        cout << sc_time_stamp() << ": [TLM] 启用流水线模式" << endl;
        
        sc_time delay = send_tlm_command(gemm_operation_t::ENABLE_PIPELINE_MODE,
                                       nullptr, 0,
                                       tlm::TLM_WRITE_COMMAND);
        
        cout << sc_time_stamp() << ": [TLM] 流水线模式启用完成，耗时: " << delay << endl;
        return delay;
    }
    
    // 🚀 新增：通知接收处理方法
    void notification_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        uint32_t* notification_data = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
        
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND && 
            trans.get_data_length() == sizeof(uint32_t) &&
            notification_data && *notification_data == 0x12345678) {
            
            cout << sc_time_stamp() << ": [TLM-Notification] 🎉 接收到计算完成通知！" << endl;
            cout << sc_time_stamp() << ": [TLM-Notification] 魔法数字: 0x" << std::hex << *notification_data << std::dec << endl;
            
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else {
            cout << sc_time_stamp() << ": [TLM-Notification] ⚠️ 接收到未知通知" << endl;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        }
        
        delay = sc_time(1, SC_NS);  // 通知处理延时
    }
};

// 🚀 多帧流水线性能分析输出函数
void print_performance_analysis(const UltraTimingStats& pipeline_stats) {
    cout << "\n🚀 多帧流水线性能分析报告" << endl;
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
    
    // 基础执行统计
    cout << "📊 基础执行统计:" << endl;
    cout << "  ├─ 加载时间: " << pipeline_stats.load_hardware_time << endl;
    cout << "  ├─ 计算时间: " << pipeline_stats.compute_hardware_time << endl;
    cout << "  ├─ 读取时间: " << pipeline_stats.read_hardware_time << endl;
    cout << "  └─ 总执行时间: " << pipeline_stats.total_execution_time << endl;
    
    // 流水线性能统计
    cout << "\n🚀 流水线性能统计:" << endl;
    cout << "  ├─ 流水线阶段时间: " << pipeline_stats.pipeline_stage_time << endl;
    cout << "  ├─ 启动延时: " << pipeline_stats.pipeline_startup_latency << endl;
    cout << "  ├─ 稳态延时: " << pipeline_stats.pipeline_steady_latency << endl;
    cout << "  ├─ 重叠效率: " << std::fixed << std::setprecision(1) 
         << pipeline_stats.overlap_efficiency << "%" << endl;
    cout << "  ├─ 流水线利用率: " << pipeline_stats.pipeline_utilization << "%" << endl;
    cout << "  └─ 吞吐率提升: " << std::setprecision(2) 
         << pipeline_stats.throughput_improvement << "x" << endl;
    
    // 多帧处理统计
    if (pipeline_stats.processed_frame_count > 0) {
        cout << "\n📈 多帧处理统计:" << endl;
        cout << "  ├─ 处理帧数: " << pipeline_stats.processed_frame_count << endl;
        cout << "  ├─ 多帧总时间: " << pipeline_stats.multi_frame_total_time << endl;
        cout << "  └─ 平均每帧延时: " << pipeline_stats.average_frame_latency << endl;
    }
    
    // 变长矩阵性能统计
    if (pipeline_stats.total_pe_count > 0) {
        cout << "\n🎯 PE利用率分析:" << endl;
        cout << "  ├─ 总PE数量: " << pipeline_stats.total_pe_count << endl;
        cout << "  ├─ 有效PE数量: " << pipeline_stats.effective_pe_count << endl;
        cout << "  ├─ PE利用率: " << std::fixed << std::setprecision(1) 
             << pipeline_stats.pe_utilization << "%" << endl;
        cout << "  └─ 内存传输效率: " << pipeline_stats.memory_efficiency << "%" << endl;
    }
    
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
}

SC_MODULE(GEMM_PINGPONG_TESTBENCH) {
    // TLM2.0标准组件
    GEMM_TLM<float, PEA_SIZE> *gemm_module;
    GEMM_TLM_INITIATOR *gemm_initiator;
    
    // 🚀 Think Ultra 简化数据
    sc_time total_execution_time;
    int test_M, test_K, test_N;  // 测试矩阵尺寸
    
    // 🚀 流水线性能分析时间戳
    sc_time pipeline_start_time;
    sc_time pipeline_end_time;
    sc_time pipeline_total_time;
    
    // 🚀 动态帧数/分块数 (与实际分块数一致)
    int actual_frame_count;
    
    SC_CTOR(GEMM_PINGPONG_TESTBENCH) : test_M(DEFAULT_M), test_K(DEFAULT_K), test_N(DEFAULT_N), actual_frame_count(1) {
        // 🚀 创建TLM2.0标准组件
        gemm_module = new GEMM_TLM<float, PEA_SIZE>("gemm_module");
        gemm_initiator = new GEMM_TLM_INITIATOR("gemm_initiator");
        
        // 🚀 TLM2.0标准socket连接
        gemm_initiator->initiator_socket.bind(gemm_module->target_socket);
        
        // 🚀 新增：连接通知socket（双向通信）
        gemm_module->initiator_socket.bind(gemm_initiator->notification_socket);
        
        // 🚀 统一执行线程
        SC_THREAD(run_gemm_test);
    }
    
    // 🚀 Think Ultra 统一GEMM测试执行
    void run_gemm_test() {
        cout << "========================================" << endl;
        cout << "🚀 Think Ultra 简化GEMM测试开始" << endl;
        cout << "  测试矩阵: A[" << test_M << "×" << test_K << "] × B[" << test_K << "×" << test_N << "]" << endl;
        cout << "========================================" << endl;
        
        // 系统初始化
        wait(10, SC_NS);
        gemm_initiator->send_reset_command();
        wait(10, SC_NS);
        
        sc_time start_time = sc_time_stamp();
        
        // 智能选择执行路径：检查是否需要分块
        bool needs_blocking = (test_M > PEA_SIZE || test_K > PEA_SIZE || test_N > PEA_SIZE);
        
        if (needs_blocking) {
            cout << "📊 检测到大矩阵，启动分块模式" << endl;
            run_large_matrix_gemm();
        } else {
            cout << "📊 使用单帧模式" << endl;
            actual_frame_count = 1; // 单帧模式
            run_single_frame_gemm();
        }
        
        total_execution_time = sc_time_stamp() - start_time;
        
        // 🚀 阶段3: 多帧流水线性能分析
        cout << "\n🚀 阶段3: 多帧流水线性能分析" << endl;
        
        // 🚀 Step 3.1: 配置流水线参数
        cout << "  Step 3.1: 配置流水线参数..." << endl;
        PipelineConfig pipeline_config = PipelineConfig::get_dual_buffer_config();
        pipeline_config.enable_detailed_stats = true;
        pipeline_config.enable_debug_trace = true;
        gemm_initiator->configure_pipeline(pipeline_config);
        
        // 🚀 Step 3.2: 启用流水线模式  
        cout << "  Step 3.2: 启用流水线模式..." << endl;
        gemm_initiator->enable_pipeline_mode();
        
        // 等待配置生效
        wait(5, SC_NS);
        
        // 🚀 Step 3.3: 执行多帧流水线分析 (使用实际分块数)
        cout << "  Step 3.3: 执行多帧流水线分析，帧数: " << actual_frame_count << " (与分块数一致)..." << endl;
        pipeline_start_time = sc_time_stamp();
        gemm_initiator->process_multi_frames(actual_frame_count);  // 分析实际帧数的流水线性能
        
        // 🚀 流水线结束时间标记
        pipeline_end_time = sc_time_stamp();
        
        // 🚀 获取真实的流水线统计数据
        UltraTimingStats pipeline_stats = gemm_initiator->get_pipeline_stats();
        
        // 🚀 Think Ultra: 精确流水线时间公式计算
        // 公式: pipeline_total_time = 启动延时 + 稳态延时 * (流水线分析帧数 - 1)
        if (actual_frame_count >= 1) {
            sc_time steady_time_contribution = sc_time(
                pipeline_stats.pipeline_steady_latency.to_double() * (actual_frame_count - 1), 
                SC_PS
            );
            pipeline_total_time = pipeline_stats.pipeline_startup_latency + steady_time_contribution;
            
            cout << "  🧮 流水线时间计算公式:" << endl;
            cout << "    启动延时: " << pipeline_stats.pipeline_startup_latency << endl;
            cout << "    稳态延时: " << pipeline_stats.pipeline_steady_latency << endl;
            cout << "    帧数: " << actual_frame_count << endl;
            cout << "    稳态贡献: " << steady_time_contribution 
                 << " = " << pipeline_stats.pipeline_steady_latency << " × (" << actual_frame_count << " - 1)" << endl;
            cout << "    总时间: " << pipeline_total_time 
                 << " = " << pipeline_stats.pipeline_startup_latency << " + " << steady_time_contribution << endl;
        } else {
            // 异常情况处理
            pipeline_total_time = sc_time(10, SC_NS);
            cout << "  ⚠️  异常帧数 (" << actual_frame_count << ")，使用默认时间: " << pipeline_total_time << endl;
        }
        
        // 🚀 详细性能统计输出
        print_performance_analysis(pipeline_stats);
        
        cout << "\n========================================" << endl;
        cout << "🎯 Think Ultra GEMM测试完成!" << endl;
        cout << "  基础执行时间: " << total_execution_time << endl;
        cout << "  流水线分析时间: " << pipeline_total_time << endl;
        cout << "  流水线分析帧数: " << actual_frame_count << " (与实际分块数一致)" << endl;
        cout << "========================================" << endl;
        
        sc_stop();
    }
    
private:
    // 🚀 单帧GEMM执行
    void run_single_frame_gemm() {
        cout << "🔄 执行单帧GEMM计算..." << endl;
        
        // 创建测试数据
        auto test_data = create_test_matrices(test_M, test_K, test_N);
        
        // 执行GEMM
        sc_time gemm_time = gemm_initiator->execute_gemm_via_tlm(
            test_data.A.data(), test_data.B.data(), test_data.D.data(), 
            test_data.result_C.data(), test_M, test_K, test_N);
        
        // 验证结果
        bool passed = verify_result(test_data, "单帧GEMM");
        
        cout << "✅ 单帧测试完成 - " << (passed ? "通过" : "失败") << ", 耗时: " << gemm_time << endl;
    }
    
    // 🚀 大矩阵分块GEMM执行
    void run_large_matrix_gemm() {
        cout << "🔄 执行大矩阵分块GEMM计算..." << endl;
        
        try {
            // 创建大矩阵测试数据
            auto test_data = create_test_matrices(test_M, test_K, test_N);
            
            // 执行分块计算
            auto multi_frame_result = LargeMatrixController16::execute_large_gemm(
                test_data.A.data(), test_data.B.data(), test_data.D.data(),
                test_data.result_C.data(), test_M, test_K, test_N);
            
            if (!multi_frame_result) {
                cout << "❌ 大矩阵分块执行失败，使用默认单帧模式" << endl;
                actual_frame_count = 1; // 失败时回退到单帧
                return;
            }
            
            // 🚀 设置实际分块数用于流水线分析
            actual_frame_count = multi_frame_result->get_frame_count();
            cout << "✅ 大矩阵成功分解为 " << actual_frame_count << " 个计算块" << endl;
            
            // 逐块执行TLM处理
            process_blocks(*multi_frame_result);
            
            // 重构结果
            auto plan = LargeMatrixController16::generate_block_plan(test_M, test_K, test_N);
            LargeMatrixController16::reconstruct_large_result(*multi_frame_result, plan, test_data.result_C.data());
            
            // 验证结果
            bool passed = verify_result(test_data, "分块GEMM");
            
            cout << "✅ 分块测试完成 - " << (passed ? "通过" : "失败") << ", 总块数: " << multi_frame_result->get_frame_count() << endl;
            
        } catch (const std::exception& e) {
            cout << "❌ 大矩阵测试异常: " << e.what() << endl;
            actual_frame_count = 1; // 异常时回退到单帧
        }
    }
    
    // 🚀 处理分块数据
    void process_blocks(const MultiFrameMatrixSet<PEA_SIZE>& blocks) {
        cout << "🔄 处理 " << blocks.get_frame_count() << " 个分块..." << endl;
        
        for (int i = 0; i < blocks.get_frame_count(); i++) {
            const auto* matrix_set = blocks.get_frame(i);
            if (!matrix_set) continue;
            
            cout << "  ⚡ Block " << (i+1) << "/" << blocks.get_frame_count() 
                 << ": [" << matrix_set->M << "×" << matrix_set->K << "×" << matrix_set->N << "]" << endl;
            
            auto* non_const_matrix_set = const_cast<MatrixSet<PEA_SIZE>*>(matrix_set);
            gemm_initiator->execute_gemm_via_tlm(
                non_const_matrix_set->A_ptr(),
                non_const_matrix_set->B_ptr(),
                non_const_matrix_set->D_ptr(),
                non_const_matrix_set->C_ptr(),
                matrix_set->M, matrix_set->K, matrix_set->N
            );
            
            if (i < blocks.get_frame_count() - 1) {
                wait(5, SC_NS);  // 块间延时
            }
        }
        
        cout << "✅ 所有分块处理完成" << endl;
    }
    
    // 🚀 测试数据结构
    struct TestData {
        std::vector<float> A, B, D;
        std::vector<float> result_C;
        std::vector<float> expected_C;
    };
    
    // 🚀 创建测试矩阵
    TestData create_test_matrices(int M, int K, int N) {
        cout << "📊 创建测试数据 [" << M << "×" << K << "×" << N << "]..." << endl;
        
        TestData data;
        data.A.resize(M * K);
        data.B.resize(K * N);
        data.D.resize(M * N);
        data.result_C.resize(M * N);
        data.expected_C.resize(M * N);
        
        // 生成简单测试数据
        for (int i = 0; i < M * K; i++) {
            data.A[i] = static_cast<float>((i % 5) + 1);  // 1-5循环
        }
        for (int i = 0; i < K * N; i++) {
            data.B[i] = static_cast<float>((i % 3) + 1);  // 1-3循环
        }
        for (int i = 0; i < M * N; i++) {
            data.D[i] = static_cast<float>(i % 2);        // 0-1循环
        }
        
        // 计算CPU参考结果
        calculate_cpu_reference(data, M, K, N);
        
        cout << "✅ 测试数据创建完成" << endl;
        return data;
    }
    
    // 🚀 计算CPU参考结果
    void calculate_cpu_reference(TestData& data, int M, int K, int N) {
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                float sum = 0.0f;
                for (int k = 0; k < K; k++) {
                    sum += data.A[i * K + k] * data.B[k * N + j];
                }
                data.expected_C[i * N + j] = sum + data.D[i * N + j];
            }
        }
    }
    
    // 🚀 结果验证
    bool verify_result(const TestData& data, const std::string& test_name) {
        cout << "🔍 验证 " << test_name << " 结果...";
        
        const float tolerance = 1e-3f;
        int error_count = 0;
        
        for (size_t i = 0; i < data.result_C.size(); i++) {
            float diff = std::abs(data.result_C[i] - data.expected_C[i]);
            if (diff > tolerance) {
                error_count++;
            }
        }
        
        if (error_count == 0) {
            cout << " ✅ 全部正确!" << endl;
            return true;
        } else {
            cout << " ❌ 发现 " << error_count << " 个错误" << endl;
            return false;
        }
    }

public:
    ~GEMM_PINGPONG_TESTBENCH() {
        delete gemm_initiator;
        delete gemm_module;
    }
};

int sc_main(int argc, char* argv[]) {
    cout << "🚀 GEMM简化验证测试启动" << endl;
    
    GEMM_PINGPONG_TESTBENCH tb("testbench");
    
    // 运行仿真
    sc_start();
    
    cout << "✅ 测试完成" << endl;
    return 0;
}