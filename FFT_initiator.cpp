/**
 * @file FFT_initiator.cpp  
 * @brief 重构后的 FFT_TLM Multi-Frame Test Program Implementation
 * @version 2.0 - 重构版本，分离了2D处理逻辑和存储管理
 * @date 2025-01-10
 */

#include "FFT_initiator.h"
#include "util/const.h"
#include "util/tools.h"
#include <cmath>

using namespace std;
using namespace FFTTestUtils;

// ============================================
// 系统初始化部分
// ============================================

template <typename T>
void FFT_Initiator<T>::System_init_process(){
    cout << "====== System Initialization Started ======" << endl;
    cout << "Time: " << sc_time_stamp() << endl;
    
    // Step 1: 配置测试参数
    configure_test_parameters();
    
    // Step 2: 设置DMI接口
    setup_memory_interfaces();
    
    // Step 3: 初始化FFT硬件
    initialize_fft_hardware();
    
    test_initialization_done = true;
    FFT_init_process_done_event.notify();
    
    cout << "====== System Initialization Completed ======\n" << endl;
}

template <typename T>
void FFT_Initiator<T>::configure_test_parameters() {
    cout << "\n[CONFIG] Setting test parameters..." << endl;
    
    // 基础参数配置
    M = 16;  // 总FFT点数
    test_frames_count = DEFAULT_TEST_FRAMES;
    
    // 2D分解参数（当M=16时，分解为4x4）
    if (M == 16) {
        N1 = 4;  // 行数
        N2 = 4;  // 列数
        use_2d_decomposition = true;
        cout << "  - 2D decomposition enabled: " << M << " = " << N1 << " x " << N2 << endl;
    } else {
        use_2d_decomposition = false;
        cout << "  - Direct FFT mode: " << M << " points" << endl;
    }
    
    real_single_fft_size = M;
    single_frame_fft_size = real_single_fft_size;
    last_configured_fft_size = 0;
    
    cout << "  - Test frames: " << test_frames_count << endl;
    cout << "  - FFT size: " << real_single_fft_size << " points" << endl;
}

template <typename T>
void FFT_Initiator<T>::setup_memory_interfaces() {
    cout << "\n[MEMORY] Setting up DMI interfaces..." << endl;
    
    setup_dmi(AM_BASE_ADDR, am_dmi, "AM");
    setup_dmi(SM_BASE_ADDR, sm_dmi, "SM");
    setup_dmi(DDR_BASE_ADDR, ddr_dmi, "DDR");
    setup_dmi(GSM_BASE_ADDR, gsm_dmi, "GSM");
    
    cout << "  - All DMI interfaces configured" << endl;
}

template <typename T>
void FFT_Initiator<T>::initialize_fft_hardware() {
    cout << "\n[FFT-HW] Initializing FFT hardware..." << endl;
    
    // Step 1: 系统复位
    cout << "  - Executing system reset..." << endl;
    send_fft_reset_transaction();
    
    // Step 2: 配置FFT参数
    FFTConfiguration config = create_fft_configuration(FFT_TLM_N, real_single_fft_size);
    send_fft_configure_transaction(config);
    
    // Step 3: 加载旋转因子
    cout << "  - Loading twiddle factors..." << endl;
    send_fft_load_twiddles_transaction();
    
    // 等待硬件初始化完成
    wait(sc_time(BaseInitiatorModel<T>::FFT_TWIDDLE_WAIT_CYCLES, SC_NS));
}

// ============================================
// 主控制流程 - 帧循环处理
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_frame_loop_process() {
    cout << "\n====== FFT Multi-Frame Processing Started ======" << endl;
    wait(FFT_init_process_done_event);
    
    for (unsigned frame = 0; frame < test_frames_count; frame++) {
        current_frame_id = frame;
        
        cout << "\n========== FRAME " << frame + 1 << "/" << test_frames_count 
             << " ==========" << endl;
        
        // 重置帧状态
        reset_frame_state();
        
        // 根据配置选择处理模式
        if (use_2d_decomposition) {
            process_frame_2d_mode();
        } else {
            process_frame_direct_mode();
        }
        
        // 显示帧处理结果
        display_frame_result(frame);
    }
    
    cout << "\n====== All Frames Processing Completed ======" << endl;
    display_final_statistics();
}

template <typename T>
void FFT_Initiator<T>::reset_frame_state() {
    current_computation_done = false;
    current_verification_done = false;
}

template <typename T>
void FFT_Initiator<T>::process_frame_2d_mode() {
    cout << "[FRAME-2D] Using 2D decomposition mode" << endl;
    
    // 触发2D处理流程
    single_2d_start_event.notify();
    wait(single_2d_done_event);
}

template <typename T>
void FFT_Initiator<T>::process_frame_direct_mode() {
    cout << "[FRAME-DIRECT] Using direct FFT mode" << endl;
    
    // 触发直接处理流程
    single_frame_start_event.notify();
    wait(single_frame_done_event);
}

// ============================================
// 数据生成与存储管理
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_frame_generation_process() {
    while (true) {
        wait(fft_frame_prepare_event);
        
        cout << "\n[DATA-GEN] Generating frame " << current_frame_id + 1 << " data..." << endl;
        
        // Step 1: 配置FFT参数（如果需要）
        if (should_reconfigure_fft()) {
            reconfigure_fft_hardware();
        }
        
        // Step 2: 生成测试数据
        auto test_data = generate_frame_test_data();
        
        // Step 3: 数据搬移流程
        perform_data_movement(test_data);
        
        // Step 4: 计算参考结果
        compute_reference_results(test_data);
        
        cout << "[DATA-GEN] Frame data generation completed" << endl;
        fft_frame_prepare_done_event.notify();
    }
}

template <typename T>
bool FFT_Initiator<T>::should_reconfigure_fft() {
    return single_frame_fft_size != last_configured_fft_size;
}

template <typename T>
void FFT_Initiator<T>::reconfigure_fft_hardware() {
    cout << "  [CONFIG] Reconfiguring FFT: " << last_configured_fft_size 
         << " -> " << single_frame_fft_size << " points" << endl;
    
    FFTConfiguration config = create_fft_configuration(FFT_TLM_N, single_frame_fft_size);
    send_fft_configure_transaction(config);
    
    wait(sc_time(BaseInitiatorModel<T>::FFT_CONFIG_WAIT_CYCLES, SC_NS));
    last_configured_fft_size = single_frame_fft_size;
}

template <typename T>
vector<complex<T>> FFT_Initiator<T>::generate_frame_test_data() {
    // 生成测试序列
    auto test_data = generate_test_sequence(
        single_frame_fft_size, 
        DataGenType::SEQUENTIAL, 
        current_frame_id + 1
    );
    
    // 显示输入数据
    cout << "  Input: ";
    for (const auto& val : test_data) {
        cout << "(" << fixed << setprecision(1) 
             << val.real << "," << val.imag << ") ";
    }
    cout << endl;
    
    return test_data;
}

template <typename T>
void FFT_Initiator<T>::perform_data_movement(const vector<complex<T>>& test_data) {
    cout << "  [DMA] Performing data movement sequence..." << endl;
    
    // Step 1: 写入DDR
    uint64_t ddr_data_addr = calculate_ddr_address(current_frame_id);
    write_data_to_ddr(test_data, ddr_data_addr);
    
    // Step 2: 写入旋转因子到DDR
    uint64_t ddr_twiddle_addr = ddr_data_addr + TEST_FFT_SIZE * sizeof(complex<T>);
    write_twiddle_factors_to_ddr(ddr_twiddle_addr);
    
    // Step 3: DMA传输到AM
    uint64_t am_data_addr = calculate_am_address(current_frame_id);
    transfer_ddr_to_am(ddr_data_addr, am_data_addr, test_data.size());
    
    // Step 4: 从AM读取数据（模拟延迟）
    read_data_from_am(am_data_addr, test_data.size());
}

template <typename T>
void FFT_Initiator<T>::write_data_to_ddr(const vector<complex<T>>& data, uint64_t addr) {
    vector<complex<T>> complex_data(data.begin(), data.end());
    write_complex_data_dmi_no_latency(addr, complex_data, complex_data.size(), this->ddr_dmi);
}

template <typename T>
void FFT_Initiator<T>::write_twiddle_factors_to_ddr(uint64_t addr) {
    auto twiddle_factors = calculate_twiddle_factors<float>(TEST_FFT_SIZE);
    vector<complex<T>> complex_twiddles(twiddle_factors.begin(), twiddle_factors.end());
    write_complex_data_dmi_no_latency(addr, complex_twiddles, complex_twiddles.size(), this->ddr_dmi);
}

template <typename T>
void FFT_Initiator<T>::transfer_ddr_to_am(uint64_t src_addr, uint64_t dst_addr, size_t size) {
    // 数据传输
    ins::dma_p2p_trans(this->socket, 
                      src_addr, 0, size * sizeof(complex<T>), 1,
                      dst_addr, 0, size * sizeof(complex<T>), 1);
    
    // 旋转因子传输
    uint64_t twiddle_src = src_addr + TEST_FFT_SIZE * sizeof(complex<T>);
    uint64_t twiddle_dst = dst_addr + TEST_FFT_SIZE * sizeof(complex<T>);
    ins::dma_p2p_trans(this->socket,
                      twiddle_src, 0, size * sizeof(complex<T>), 1,
                      twiddle_dst, 0, size * sizeof(complex<T>), 1);
}

template <typename T>
void FFT_Initiator<T>::read_data_from_am(uint64_t addr, size_t size) {
    vector<complex<T>> data_read;
    ins::read_from_dmi<complex<T>>(addr, data_read, this->am_dmi, size);
    frame_input_data[current_frame_id] = data_read;
}

// ============================================
// 2D FFT处理流程
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_single_2D_process() {
    while (true) {
        wait(single_2d_start_event);
        
        cout << "\n[2D-FFT] Starting 2D decomposition..." << endl;
        
        // 初始化2D矩阵
        initialize_2d_matrices();
        
        // 三阶段处理
        process_2d_stage1_column_fft();
        process_2d_stage2_twiddle();
        process_2d_stage3_row_fft();
        
        // 结果整理
        finalize_2d_results();
        
        cout << "[2D-FFT] 2D decomposition completed" << endl;
        single_2d_done_event.notify();
    }
}

template <typename T>
void FFT_Initiator<T>::initialize_2d_matrices() {
    cout << "  [2D-INIT] Initializing matrices (N1=" << N1 << ", N2=" << N2 << ")" << endl;
    
    frame_data_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
    frame_G_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
    frame_H_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
    frame_X_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
}

template <typename T>
void FFT_Initiator<T>::process_2d_stage1_column_fft() {
    cout << "\n  [Stage 1] Column FFT Processing..." << endl;
    current_2d_stage = 1;
    
    for (current_column_id = 0; current_column_id < N1; current_column_id++) {
        cout << "    - Column " << current_column_id + 1 << "/" << N1 << endl;
        
        // 设置当前FFT大小并触发处理
        single_frame_fft_size = N2;  // 列FFT是N2点
        single_frame_start_event.notify();
        wait(single_frame_done_event);
    }
}

template <typename T>
void FFT_Initiator<T>::process_2d_stage2_twiddle() {
    cout << "\n  [Stage 2] Twiddle Factor Compensation..." << endl;
    current_2d_stage = 2;
    
    twiddle_compensation_start_event.notify();
    wait(twiddle_compensation_done_event);
}

template <typename T>
void FFT_Initiator<T>::process_2d_stage3_row_fft() {
    cout << "\n  [Stage 3] Row FFT Processing..." << endl;
    current_2d_stage = 3;
    
    for (current_row_id = 0; current_row_id < N2; current_row_id++) {
        cout << "    - Row " << current_row_id + 1 << "/" << N2 << endl;
        
        // 设置当前FFT大小并触发处理
        single_frame_fft_size = N1;  // 行FFT是N1点
        single_frame_start_event.notify();
        wait(single_frame_done_event);
        
    }
}

template <typename T>
void FFT_Initiator<T>::finalize_2d_results() {
    // 将最终矩阵转换为输出向量
    auto final_matrix = frame_X_matrix[current_frame_id];
    vector<complex<T>> final_output = reshape_to_vector(final_matrix);
    frame_output_data[current_frame_id] = final_output;
    
    // 显示结果
    cout << "\n  Final 2D Output: ";
    for (size_t i = 0; i < min(final_output.size(), size_t(8)); i++) {
        cout << "(" << fixed << setprecision(2) 
             << final_output[i].real << "," << final_output[i].imag << ") ";
    }
    if (final_output.size() > 8) cout << "...";
    cout << endl;
}

// ============================================
// FFT计算核心流程
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_computation_process() {
    while (true) {
        wait(fft_computation_start_event);
        
        cout << "\n[FFT-COMP] Starting computation..." << endl;
        
        // 获取输入数据
        vector<complex<T>> input_data = frame_input_data[current_frame_id];
        
        // 执行FFT计算
        vector<complex<float>> complex_input(input_data.begin(), input_data.end());
        vector<complex<float>> complex_output = perform_fft(complex_input, single_frame_fft_size);
        
        // 存储结果
        vector<complex<T>> output_data(complex_output.begin(), complex_output.end());
        frame_output_data[current_frame_id] = output_data;
        
        // 显示输出
        cout << "  Output: ";
        for (size_t i = 0; i < min(output_data.size(), size_t(8)); i++) {
            cout << "(" << fixed << setprecision(2) 
                 << output_data[i].real << "," << output_data[i].imag << ") ";
        }
        if (output_data.size() > 8) cout << "...";
        cout << endl;
        
        cout << "[FFT-COMP] Computation completed" << endl;
        fft_computation_done_event.notify();
    }
}

// ============================================
// 验证流程
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_verification_process() {
    while (true) {
        wait(fft_verification_start_event);
        
        cout << "\n[VERIFY] Verifying results..." << endl;
        
        bool verification_passed = verify_frame_result(current_frame_id);
        frame_test_results[current_frame_id] = verification_passed;
        
        cout << "  Result: " << (verification_passed ? "PASS ✓" : "FAIL ✗") << endl;
        
        current_verification_done = true;
        fft_verification_done_event.notify();
    }
}

// ============================================
// 辅助函数
// ============================================

template <typename T>
FFTConfiguration FFT_Initiator<T>::create_fft_configuration(size_t hw_size, size_t real_size) {
    FFTConfiguration config;
    config.fft_mode = true;
    config.fft_shift = 0;
    config.fft_conj_en = false;
    config.fft_size = hw_size;
    config.fft_size_real = real_size;
    
    // 配置bypass
    int hw_stages = static_cast<int>(std::log2(hw_size));
    int required_stages = static_cast<int>(std::log2(real_size));
    config.stage_bypass_en.resize(hw_stages, false);
    
    if (real_size < hw_size) {
        int bypass_stages = hw_stages - required_stages;
        cout << "  - Bypass configuration: " << bypass_stages << " stages" << endl;
        for (int i = 0; i < bypass_stages; i++) {
            config.stage_bypass_en[i] = true;
        }
    }
    
    return config;
}

template <typename T>
uint64_t FFT_Initiator<T>::calculate_ddr_address(unsigned frame_id) {
    return DDR_BASE_ADDR + frame_id * TEST_FFT_SIZE * sizeof(complex<T>) * 2;
}

template <typename T>
uint64_t FFT_Initiator<T>::calculate_am_address(unsigned frame_id) {
    return AM_BASE_ADDR + frame_id * TEST_FFT_SIZE * sizeof(complex<T>) * 2;
}

template <typename T>
void FFT_Initiator<T>::display_frame_result(unsigned frame_id) {
    cout << "\n[FRAME " << frame_id + 1 << "] Summary:" << endl;
    cout << "  - Processing mode: " << (use_2d_decomposition ? "2D Decomposition" : "Direct FFT") << endl;
    cout << "  - Verification: " << (frame_test_results[frame_id] ? "PASSED" : "FAILED") << endl;
}

template <typename T>
void FFT_Initiator<T>::display_final_statistics() {
    int passed = 0;
    for (bool result : frame_test_results) {
        if (result) passed++;
    }
    
    cout << "\n====== Final Statistics ======" << endl;
    cout << "Total frames: " << test_frames_count << endl;
    cout << "Passed: " << passed << endl;
    cout << "Failed: " << (test_frames_count - passed) << endl;
    cout << "Success rate: " << (100.0 * passed / test_frames_count) << "%" << endl;
}

// ============================================
// 其他必要的成员函数（保持原有实现）
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_twiddle_process() {
    while (true) {
        wait(twiddle_compensation_start_event);
        
        auto& G_matrix = frame_G_matrix[current_frame_id];
        auto& H_matrix = frame_H_matrix[current_frame_id];
        
        // 应用旋转因子补偿: H(k2,n1) = W_M^(k2*n1) * G(k2,n1)
        for (unsigned k2 = 0; k2 < N2; k2++) {
            for (unsigned n1 = 0; n1 < N1; n1++) {
                complex<float> twiddle = compute_twiddle_factor(k2, n1, M);
                complex<float> G_val(G_matrix[k2][n1].real, G_matrix[k2][n1].imag);
                complex<float> H_val = twiddle * G_val;
                H_matrix[k2][n1] = complex<T>(H_val.real, H_val.imag);
            }
        }
        
        cout << "    - Twiddle compensation applied" << endl;
        twiddle_compensation_done_event.notify();
    }
}


template <typename T>
void FFT_Initiator<T>::FFT_single_frame_process() {
    while (true) {
        wait(single_frame_start_event);
        
        // 触发数据生成
        fft_frame_prepare_event.notify();
        wait(fft_frame_prepare_done_event);
        
        // 触发FFT计算
        fft_computation_start_event.notify();
        wait(fft_computation_done_event);
        
        // 触发验证
        fft_verification_start_event.notify();
        wait(fft_verification_done_event);
        
        single_frame_done_event.notify();
    }
}

// 保留原有的辅助函数实现
template <typename T>
bool FFT_Initiator<T>::verify_frame_result(unsigned frame_id) {
    if (frame_test_results.empty()) {
        frame_test_results.resize(DEFAULT_TEST_FRAMES, false);
    }
    
    vector<complex<T>> fft_output = frame_output_data[frame_id];
    vector<complex<T>> reference_dft = frame_reference_data[frame_id];
    
    if (fft_output.size() != reference_dft.size()) {
        cout << "  ERROR: Size mismatch" << endl;
        return false;
    }
    
    vector<complex<float>> fft_std_output(fft_output.begin(), fft_output.end());
    vector<complex<float>> reference_complex(reference_dft.begin(), reference_dft.end());
    
    // 应用bit-reversal
    vector<complex<float>> fft_natural_order(fft_std_output.size());
    int fft_size = fft_std_output.size();
    for (size_t i = 0; i < fft_size/2; i++) {
        fft_natural_order[i] = fft_std_output[2*i];
        fft_natural_order[i+fft_size/2] = fft_std_output[2*i+1];
    }
    
    const float tolerance = 0.1f;
    return compare_complex_sequences(fft_natural_order, reference_complex, tolerance, false);
}

template <typename T>
void FFT_Initiator<T>::compute_reference_results(const vector<complex<T>>& test_data) {
    vector<complex<float>> complex_test_data(test_data.begin(), test_data.end());
    vector<complex<float>> complex_reference = compute_reference_dft(complex_test_data);
    frame_reference_data[current_frame_id] = vector<complex<T>>(complex_reference.begin(), 
                                                                 complex_reference.end());
}

template <typename T>
complex<float> FFT_Initiator<T>::compute_twiddle_factor(int k2, int n1, int N) {
    float angle = -2.0f * M_PI * k2 * n1 / N;
    return complex<float>(cos(angle), sin(angle));
}

template <typename T>
vector<vector<complex<T>>> FFT_Initiator<T>::reshape_to_matrix(const vector<complex<T>>& input, 
                                                               int rows, int cols) {
    if (input.size() != rows * cols) {
        return vector<vector<complex<T>>>(rows, vector<complex<T>>(cols, complex<T>(0, 0)));
    }
    
    vector<vector<complex<T>>> matrix(rows, vector<complex<T>>(cols));
    for (int i = 0; i < input.size(); ++i) {
        matrix[i / cols][i % cols] = input[i];
    }
    return matrix;
}

template <typename T>
vector<complex<T>> FFT_Initiator<T>::reshape_to_vector(const vector<vector<complex<T>>>& matrix) {
    if (matrix.empty() || matrix[0].empty()) {
        return vector<complex<T>>();
    }
    
    vector<complex<T>> output;
    for (const auto& row : matrix) {
        for (const auto& val : row) {
            output.push_back(val);
        }
    }
    return output;
}

// 模板实例化
template class FFT_Initiator<float>;