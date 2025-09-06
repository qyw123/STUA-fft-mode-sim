/**
 * @file FFT_initiator.h
 * @brief FFT_TLM Multi-Frame Test Program - Test Initiator inheriting from BaseInitiatorModel
 * 
 * This program implements system-level FFT_TLM testing by inheriting from util/base_initiator_modle.h,
 * testing the multi-frame FFT computation functionality from src/vcore/FFT_SA/testbench/FFT_TLM_test.cpp.
 * 
 * Test Features:
 * - Inherits TLM interface and FFT methods from BaseInitiatorModel
 * - Multi-frame continuous FFT processing (configurable frame count)
 * - Event-driven test flow control
 * - Fully automated data generation, computation and verification
 * - Complete test statistics and reporting
 * 
 * @version 1.0 - Initial implementation based on BaseInitiatorModel
 * @date 2025-08-31
 */

#ifndef FFT_INITIATOR_H
#define FFT_INITIATOR_H

#include "util/base_initiator_modle.h"
#include "src/vcore/FFT_SA/utils/fft_test_utils.h"
#include "src/vcore/FFT_SA/utils/complex_types.h"
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace FFTTestUtils;

/**
 * @brief FFT_TLM Multi-Frame Test Initiator Class
 * 
 * Inherits from BaseInitiatorModel and utilizes its FFT methods for system-level FFT testing
 */
template <typename T>
class FFT_Initiator : public BaseInitiatorModel<T> {
public:
    SC_HAS_PROCESS(FFT_Initiator);
    
    // 构造函数
    FFT_Initiator(sc_module_name name) : BaseInitiatorModel<T>(name) {
        // SystemC进程注册
        SC_THREAD(System_init_process);
        SC_THREAD(FFT_frame_loop_process);
        SC_THREAD(FFT_frame_generation_process);
        SC_THREAD(FFT_computation_process);
        SC_THREAD(FFT_verification_process);
        SC_THREAD(FFT_twiddle_process);
        SC_THREAD(FFT_row_process);
        SC_THREAD(FFT_single_frame_process);
        SC_THREAD(FFT_single_2D_process);
    }
    
    // ====== 成员变量（数据存储） ======
    
    // 测试参数
    unsigned M;                      // 总FFT点数
    unsigned N1, N2;                 // 2D分解维度
    unsigned test_frames_count;      // 测试帧数
    unsigned real_single_fft_size;   // 实际单帧FFT大小
    unsigned single_frame_fft_size;  // 当前单帧FFT大小
    unsigned last_configured_fft_size; // 上次配置的FFT大小
    bool use_2d_decomposition;       // 是否使用2D分解
    
    // 当前处理状态
    unsigned current_frame_id;       // 当前帧ID
    unsigned current_column_id;      // 当前列ID（2D处理用）
    unsigned current_row_id;         // 当前行ID（2D处理用）
    int current_2d_stage;            // 当前2D处理阶段
    bool current_computation_done;   // 计算完成标志
    bool current_verification_done;  // 验证完成标志
    bool test_initialization_done;   // 初始化完成标志
    
    // ====== 数据存储容器 ======
    
    // 帧数据存储（使用map按帧ID索引）
    std::map<unsigned, std::vector<std::complex<T>>> frame_input_data;     // 输入数据
    std::map<unsigned, std::vector<std::complex<T>>> frame_output_data;    // 输出数据
    std::map<unsigned, std::vector<std::complex<T>>> frame_reference_data; // 参考数据
    
    // 2D处理中间矩阵存储
    std::map<unsigned, std::vector<std::vector<std::complex<T>>>> frame_data_matrix;  // 原始数据矩阵
    std::map<unsigned, std::vector<std::vector<std::complex<T>>>> frame_G_matrix;     // 列FFT后的矩阵
    std::map<unsigned, std::vector<std::vector<std::complex<T>>>> frame_H_matrix;     // 旋转因子补偿后
    std::map<unsigned, std::vector<std::vector<std::complex<T>>>> frame_X_matrix;     // 行FFT后的最终结果
    
    // 测试结果
    std::vector<bool> frame_test_results;  // 各帧验证结果
    
    // ====== SystemC事件 ======
    
    // 主流程控制事件
    sc_event FFT_init_process_done_event;      // 初始化完成
    sc_event single_frame_start_event;         // 单帧处理开始
    sc_event single_frame_done_event;          // 单帧处理完成
    sc_event single_2d_start_event;            // 2D处理开始
    sc_event single_2d_done_event;             // 2D处理完成
    
    // 子流程事件
    sc_event fft_frame_prepare_event;          // 帧数据准备
    sc_event fft_frame_prepare_done_event;     // 帧数据准备完成
    sc_event fft_computation_start_event;      // FFT计算开始
    sc_event fft_computation_done_event;       // FFT计算完成
    sc_event fft_verification_start_event;     // 验证开始
    sc_event fft_verification_done_event;      // 验证完成
    
    // 2D处理事件
    sc_event twiddle_compensation_start_event; // 旋转因子补偿开始
    sc_event twiddle_compensation_done_event;  // 旋转因子补偿完成
    sc_event row_fft_start_event;              // 行FFT开始
    sc_event row_fft_done_event;               // 行FFT完成
    sc_event column_fft_start_event;           // 列FFT开始（如果需要）
    sc_event column_fft_done_event;            // 列FFT完成（如果需要）
    
    // ====== 成员函数声明 ======
    
    // SystemC进程
    void System_init_process();
    void FFT_frame_loop_process();
    void FFT_frame_generation_process();
    void FFT_computation_process();
    void FFT_verification_process();
    void FFT_twiddle_process();
    void FFT_row_process();
    void FFT_single_frame_process();
    void FFT_single_2D_process();
    
    // 辅助函数
    void configure_test_parameters();
    void setup_memory_interfaces();
    void initialize_fft_hardware();
    void reset_frame_state();
    void process_frame_2d_mode();
    void process_frame_direct_mode();
    
    // 数据管理函数
    bool should_reconfigure_fft();
    void reconfigure_fft_hardware();
    std::vector<complex<T>> generate_frame_test_data();
    void perform_data_movement(const std::vector<complex<T>>& test_data);
    void write_data_to_ddr(const std::vector<complex<T>>& data, uint64_t addr);
    void write_twiddle_factors_to_ddr(uint64_t addr);
    void transfer_ddr_to_am(uint64_t src_addr, uint64_t dst_addr, size_t size);
    void read_data_from_am(uint64_t addr, size_t size);
    void compute_reference_results(const std::vector<complex<T>>& test_data);
    
    // 2D处理函数
    void initialize_2d_matrices();
    void process_2d_stage1_column_fft();
    void process_2d_stage2_twiddle();
    void process_2d_stage3_row_fft();
    void finalize_2d_results();
    
    // 工具函数
    std::complex<float> compute_twiddle_factor(int k2, int n1, int N);
    std::vector<std::vector<std::complex<T>>> reshape_to_matrix(
        const std::vector<std::complex<T>>& input, int rows, int cols);
    std::vector<std::complex<T>> reshape_to_vector(
        const std::vector<std::vector<std::complex<T>>>& matrix);
    
    // 配置和地址计算
    FFTConfiguration create_fft_configuration(size_t hw_size, size_t real_size);
    uint64_t calculate_ddr_address(unsigned frame_id);
    uint64_t calculate_am_address(unsigned frame_id);
    
    // 显示函数
    void display_frame_result(unsigned frame_id);
    void display_final_statistics();
    
    // 验证函数
    bool verify_frame_result(unsigned frame_id);
};

#endif // FFT_INITIATOR_H