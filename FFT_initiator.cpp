/**
 * @file FFT_initiator.cpp  
 * @brief 重构后的 FFT_TLM Multi-Frame Test Program Implementation
 * @version 2.1 - 修复2D处理逻辑漏洞
 * @date 2025-01-10
 */

#include "FFT_initiator.h"
#include "util/const.h"
#include "util/tools.h"
#include "FFT_initiator_utils.h"
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
    test_frames_count = DEFAULT_TEST_FRAMES;
    

    //FFT_TLM_N = 8; //设计FFT阵列加速最大的一帧点数
    // 设置目标FFT点数 - 可以是任意值
    // 示例配置：
    TEST_FFT_SIZE = 16;  // 测试大点数FFT
    
    // 动态分析分解策略
    auto decomp_info = FFTInitiatorUtils::analyze_decomposition_strategy(TEST_FFT_SIZE, FFT_TLM_N);
    
    if (!decomp_info.is_valid) {
        cout << "  ERROR: Cannot decompose " << TEST_FFT_SIZE 
             << " points with FFT_TLM_N=" << FFT_TLM_N << endl;
        assert(false && "Invalid FFT size for decomposition");
    }
    
    decomposition_level = decomp_info.level;
    use_2d_decomposition = (decomposition_level > 0);
    
    cout << "  - Target FFT size: " << TEST_FFT_SIZE << " points" << endl;
    cout << "  - Hardware base size (FFT_TLM_N): " << FFT_TLM_N << endl;
    cout << "  - Decomposition level: " << decomposition_level << endl;
    
    if (use_2d_decomposition) {
        cout << "  - Decomposition strategy:" << endl;
        for (int i = 0; i < decomp_info.level_dims.size(); i++) {
            cout << "    Level " << i + 1 << ": " 
                 << decomp_info.level_dims[i].first << " × " 
                 << decomp_info.level_dims[i].second << endl;
        }
    }
    
    real_single_fft_size = TEST_FFT_SIZE;
    single_frame_fft_size = real_single_fft_size;
    last_configured_fft_size = 0;
    
    cout << "  - Test frames: " << test_frames_count << endl;
}

// ============================================
// decomposition helpers moved to utils

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
    FFTConfiguration config = FFTInitiatorUtils::create_fft_configuration(FFT_TLM_N, real_single_fft_size);
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
        
        reset_frame_state();
        
        // 根据分解层级选择处理模式
        if (decomposition_level == 0) {
            process_frame_direct_mode();
        } else if (decomposition_level == 1) {
            process_frame_level1_mode();
        } else if (decomposition_level == 2) {
            process_frame_level2_mode();
        }
        
        display_frame_result(frame);
    }
    
    cout << "\n====== All Frames Processing Completed ======" << endl;
    display_final_statistics();
    sc_stop();
}

// ============================================
// Level 1处理模式（单层2D分解）
// ============================================

template <typename T>
void FFT_Initiator<T>::process_frame_level1_mode() {
    cout << "[FRAME-L1] Using Level 1 (single 2D decomposition) mode" << endl;
    
    // 准备数据
    prepare_frame_data_once();
    
    // 获取Level 1的分解维度
    auto decomp_info = FFTInitiatorUtils::analyze_decomposition_strategy(TEST_FFT_SIZE, FFT_TLM_N);
    N1 = decomp_info.level_dims[0].first;
    N2 = decomp_info.level_dims[0].second;
    
    cout << "  Level 1 decomposition: " << N1 << " × " << N2 << endl;
    
    // 执行Level 1的2D处理
    execute_level1_2d_fft();
}

template <typename T>
void FFT_Initiator<T>::execute_level1_2d_fft() {
    cout << "\n[L1-2D] Starting Level 1 2D decomposition..." << endl;
    
    // 初始化矩阵
    initialize_2d_matrices();
    
    // 将输入数据重排为矩阵
    vector<complex<T>> input_data = frame_input_data[current_frame_id];
    frame_data_matrix[current_frame_id] = FFTInitiatorUtils::reshape_to_matrix(input_data, N2, N1);
    
    // Stage 1: 列FFT
    process_level1_column_fft();
    
    // Stage 2: 旋转因子
    process_level1_twiddle();
    
    // Stage 3: 行FFT
    process_level1_row_fft();
    
    // 整理结果
    finalize_2d_results();
    perform_final_verification();
}

// ============================================
// Level 2处理模式（双层2D分解）
// ============================================

template <typename T>
void FFT_Initiator<T>::process_frame_level2_mode() {
    cout << "[FRAME-L2] Using Level 2 (nested 2D decomposition) mode" << endl;
    
    // 准备数据
    prepare_frame_data_once();
    
    // 获取分解维度
    auto decomp_info = FFTInitiatorUtils::analyze_decomposition_strategy(TEST_FFT_SIZE, FFT_TLM_N);
    size_t L2_N1 = decomp_info.level_dims[0].first;  // Level 2维度
    size_t L2_N2 = decomp_info.level_dims[0].second;
    
    cout << "  Level 2 decomposition: " << L2_N1 << " × " << L2_N2 << endl;
    
    // 执行Level 2的2D处理
    execute_level2_2d_fft(L2_N1, L2_N2);
}

template <typename T>
void FFT_Initiator<T>::execute_level2_2d_fft(size_t L2_N1, size_t L2_N2) {
    cout << "\n[L2-2D] Starting Level 2 2D decomposition..." << endl;
    
    // 获取输入数据
    vector<complex<T>> input_data = frame_input_data[current_frame_id];
    
    // 重排为Level 2矩阵 (L2_N2 × L2_N1)
    auto L2_matrix = FFTInitiatorUtils::reshape_to_matrix(input_data, L2_N2, L2_N1);
    
    // 初始化Level 2中间矩阵
    vector<vector<complex<T>>> L2_G_matrix(L2_N2, vector<complex<T>>(L2_N1));
    vector<vector<complex<T>>> L2_H_matrix(L2_N2, vector<complex<T>>(L2_N1));
    vector<vector<complex<T>>> L2_X_matrix(L2_N2, vector<complex<T>>(L2_N1));
    
    // ====== Stage 1: Level 2列FFT ======
    cout << "\n[L2-Stage1] Processing " << L2_N1 << " columns, each " << L2_N2 << " points" << endl;
    
    for (size_t col = 0; col < L2_N1; col++) {
        if (col % 16 == 0) {  // 每16列输出一次进度
            cout << "  Processing columns " << col << "-" << min(col+15, L2_N1-1) 
                 << " / " << L2_N1 << endl;
        }
        
        // 提取列数据
        vector<complex<float>> column_data(L2_N2);
        for (size_t row = 0; row < L2_N2; row++) {
            column_data[row] = complex<float>(
                L2_matrix[row][col].real,
                L2_matrix[row][col].imag
            );
        }
        
        // 对这一列执行FFT（可能需要Level 1分解）
        vector<complex<float>> col_result = perform_adaptive_fft(column_data, L2_N2);
        
        // 存储结果
        for (size_t row = 0; row < L2_N2; row++) {
            L2_G_matrix[row][col] = complex<T>(col_result[row].real, col_result[row].imag);
        }
    }
    
    // ====== Stage 2: Level 2旋转因子 ======
    cout << "\n[L2-Stage2] Applying twiddle factors for " << TEST_FFT_SIZE << "-point FFT" << endl;
    
    for (size_t n2 = 0; n2 < L2_N2; n2++) {
        for (size_t k1 = 0; k1 < L2_N1; k1++) {
            complex<float> twiddle = FFTInitiatorUtils::compute_twiddle_factor(n2, k1, TEST_FFT_SIZE);
            complex<float> G_val(L2_G_matrix[n2][k1].real, L2_G_matrix[n2][k1].imag);
            complex<float> H_val = twiddle * G_val;
            L2_H_matrix[n2][k1] = complex<T>(H_val.real, H_val.imag);
        }
    }
    
    // ====== Stage 3: Level 2行FFT ======
    cout << "\n[L2-Stage3] Processing " << L2_N2 << " rows, each " << L2_N1 << " points" << endl;
    
    for (size_t row = 0; row < L2_N2; row++) {
        if (row % 16 == 0) {  // 每16行输出一次进度
            cout << "  Processing rows " << row << "-" << min(row+15, L2_N2-1) 
                 << " / " << L2_N2 << endl;
        }
        
        // 提取行数据
        vector<complex<float>> row_data(L2_N1);
        for (size_t col = 0; col < L2_N1; col++) {
            row_data[col] = complex<float>(
                L2_H_matrix[row][col].real,
                L2_H_matrix[row][col].imag
            );
        }
        
        // 对这一行执行FFT（可能需要Level 1分解）
        vector<complex<float>> row_result = perform_adaptive_fft(row_data, L2_N1);
        
        // 存储结果
        for (size_t col = 0; col < L2_N1; col++) {
            L2_X_matrix[row][col] = complex<T>(row_result[col].real, row_result[col].imag);
        }
    }
    
    // 重排回一维
    vector<complex<T>> final_output = FFTInitiatorUtils::reshape_to_vector(L2_X_matrix);
    frame_output_data[current_frame_id] = final_output;
    
    // 显示部分结果
    cout << "\n[L2-2D] Level 2 FFT completed. Output samples:" << endl;
    cout << "  First 8 points: ";
    for (size_t i = 0; i < min(size_t(8), final_output.size()); i++) {
        cout << "(" << fixed << setprecision(2) 
             << final_output[i].real << "," << final_output[i].imag << ") ";
    }
    cout << endl;
    
    perform_final_verification();
}

// ============================================
// 自适应FFT执行器（根据大小选择策略）
// ============================================

template <typename T>
vector<complex<float>> FFT_Initiator<T>::perform_adaptive_fft(
    const vector<complex<float>>& input, 
    size_t fft_size
) {
        // 添加输入验证
    if (input.size() != fft_size) {
        cout << "WARNING: Input size mismatch. Expected: " << fft_size 
             << ", Got: " << input.size() << endl;
    }
    // Level 0: 硬件直接处理
    if (fft_size <= FFT_TLM_N) {
        return perform_fft_core(input, fft_size);
    }
    
    // Level 1: 需要2D分解
    size_t level1_max = FFT_TLM_N * FFT_TLM_N;
    if (fft_size <= level1_max) {
        // 找到合适的分解
        size_t n1 = FFT_TLM_N;
        size_t n2 = fft_size / FFT_TLM_N;
        
        // 如果不能整除，尝试方形分解
        if (n1 * n2 != fft_size) {
            size_t sqrt_size = static_cast<size_t>(sqrt(fft_size));
            if (sqrt_size * sqrt_size == fft_size) {
                n1 = sqrt_size;
                n2 = sqrt_size;
            }
        }
        
        return perform_level1_2d_fft_internal(input, n1, n2, fft_size);
    }
    
    // 超出处理能力
    cout << "ERROR: FFT size " << fft_size << " exceeds adaptive processing capability" << endl;
    return input;  // 返回原始数据
}

template <typename T>
vector<complex<float>> FFT_Initiator<T>::perform_level1_2d_fft_internal(
    const vector<complex<float>>& input,
    size_t n1, size_t n2, size_t total_size
) {
    // 重排为矩阵
    vector<vector<complex<float>>> matrix(n2, vector<complex<float>>(n1));
    for (size_t i = 0; i < total_size; i++) {
        matrix[i / n1][i % n1] = input[i];
    }
    
    // Stage 1: 列FFT
    for (size_t col = 0; col < n1; col++) {
        vector<complex<float>> column(n2);
        for (size_t row = 0; row < n2; row++) {
            column[row] = matrix[row][col];
        }
        
        auto col_result = perform_fft_core(column, n2);
        
        for (size_t row = 0; row < n2; row++) {
            matrix[row][col] = col_result[row];
        }
    }
    
    // Stage 2: 旋转因子
    for (size_t n2_idx = 0; n2_idx < n2; n2_idx++) {
        for (size_t k1_idx = 0; k1_idx < n1; k1_idx++) {
            complex<float> twiddle = FFTInitiatorUtils::compute_twiddle_factor(n2_idx, k1_idx, total_size);
            matrix[n2_idx][k1_idx] = twiddle * matrix[n2_idx][k1_idx];
        }
    }
    
    // Stage 3: 行FFT
    for (size_t row = 0; row < n2; row++) {
        vector<complex<float>> row_data(n1);
        for (size_t col = 0; col < n1; col++) {
            row_data[col] = matrix[row][col];
        }
        
        auto row_result = perform_fft_core(row_data, n1);
        
        for (size_t col = 0; col < n1; col++) {
            matrix[row][col] = row_result[col];
        }
    }
    
    // 重排回一维
    vector<complex<float>> output(total_size);
    for (size_t i = 0; i < total_size; i++) {
        output[i] = matrix[i / n1][i % n1];
    }
    
    return output;
}

// ============================================
// Level 1专用处理函数
// ============================================

template <typename T>
void FFT_Initiator<T>::process_level1_column_fft() {
    cout << "\n  [L1-Stage1] Column FFT Processing..." << endl;
    
    auto& input_matrix = frame_data_matrix[current_frame_id];
    auto& G_matrix = frame_G_matrix[current_frame_id];
    
    for (size_t col = 0; col < N1; col++) {
        vector<complex<float>> column_data(N2);
        for (size_t row = 0; row < N2; row++) {
            column_data[row] = complex<float>(
                input_matrix[row][col].real,
                input_matrix[row][col].imag
            );
        }
        cout << sc_time_stamp() << "开始计算col_result"  << endl;
        auto col_result = perform_fft_core(column_data, N2);
        cout << sc_time_stamp() << "完成计算col_result"  << endl;
        for (size_t row = 0; row < N2; row++) {
            G_matrix[row][col] = complex<T>(col_result[row].real, col_result[row].imag);
        }
    }
    cout << "  [L1-Stage1] All column FFTs completed" << endl;
}

template <typename T>
void FFT_Initiator<T>::process_level1_twiddle() {
    cout << "\n  [L1-Stage2] Twiddle Factor Compensation..." << endl;
    
    auto& G_matrix = frame_G_matrix[current_frame_id];
    auto& H_matrix = frame_H_matrix[current_frame_id];
    
    for (size_t n2 = 0; n2 < N2; n2++) {
        for (size_t k1 = 0; k1 < N1; k1++) {
            complex<float> twiddle = FFTInitiatorUtils::compute_twiddle_factor(n2, k1, TEST_FFT_SIZE);
            complex<float> G_val(G_matrix[n2][k1].real, G_matrix[n2][k1].imag);
            complex<float> H_val = twiddle * G_val;
            H_matrix[n2][k1] = complex<T>(H_val.real, H_val.imag);
            wait(1,SC_NS);
        }
    }
    cout << "  [L1-Stage2] Twiddle compensation completed" << endl;
}

template <typename T>
void FFT_Initiator<T>::process_level1_row_fft() {
    cout << "\n  [L1-Stage3] Row FFT Processing..." << endl;
    
    auto& H_matrix = frame_H_matrix[current_frame_id];
    auto& X_matrix = frame_X_matrix[current_frame_id];
    
    for (size_t row = 0; row < N2; row++) {
        vector<complex<float>> row_data(N1);
        for (size_t col = 0; col < N1; col++) {
            row_data[col] = complex<float>(
                H_matrix[row][col].real,
                H_matrix[row][col].imag
            );
        }
        
        auto row_result = perform_fft_core(row_data, N1);
        
        for (size_t col = 0; col < N1; col++) {
            X_matrix[row][col] = complex<T>(row_result[col].real, row_result[col].imag);
        }
    }
    cout << "  [L1-Stage3] All row FFTs completed" << endl;
}

template <typename T>
void FFT_Initiator<T>::reset_frame_state() {
    current_computation_done = false;
    current_verification_done = false;
    frame_data_ready = false;  // 新增：数据准备状态标志
}

template <typename T>
void FFT_Initiator<T>::process_frame_2d_mode() {
    cout << "[FRAME-2D] Using 2D decomposition mode" << endl;
    
    // 先准备整帧的数据（只执行一次）
    prepare_frame_data_once();
    
    // 然后触发2D处理流程
    single_2d_start_event.notify();
    wait(single_2d_done_event);

    //补充一个读出计算结果的逻辑
    //read_out_frame_result();
}

template <typename T>
void FFT_Initiator<T>::process_frame_direct_mode() {
    cout << "[FRAME-DIRECT] Using direct FFT mode" << endl;
    
    // 触发直接处理流程
    single_frame_start_event.notify();
    wait(single_frame_done_event);
}

// ============================================
// 新增：一次性帧数据准备
// ============================================

template <typename T>
void FFT_Initiator<T>::prepare_frame_data_once() {
    if (frame_data_ready) {
        return;  // 数据已准备，避免重复
    }
    
    cout << "\n[DATA-PREP] Preparing frame " << current_frame_id + 1 << " data (one-time)..." << endl;
    
    // Step 1: 生成测试数据
    auto test_data = generate_frame_test_data();
    
    // Step 2: 数据搬移流程（DDR → AM）
    perform_data_movement(test_data);
    
    // Step 3: 计算参考结果
    compute_reference_results(test_data);
    
    // Step 4: 如果是2D模式，将数据重排为矩阵
    if (use_2d_decomposition) {
        vector<complex<T>> input_data = frame_input_data[current_frame_id];
        frame_data_matrix[current_frame_id] = FFTInitiatorUtils::reshape_to_matrix(input_data, N2, N1);
        cout << "  - Data reshaped to " << N2 << "x" << N1 << " matrix" << endl;
    }
    
    frame_data_ready = true;
    cout << "[DATA-PREP] Frame data preparation completed" << endl;
}

// ============================================
// 数据生成与存储管理（修改版）
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_frame_generation_process() {
    while (true) {
        wait(fft_frame_prepare_event);
        
        // 检查是否需要准备数据（对于2D模式，数据已在外部准备）
        if (!frame_data_ready) {
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
            
            frame_data_ready = true;
            cout << "[DATA-GEN] Frame data generation completed" << endl;
        } else {
            cout << "[DATA-GEN] Using pre-prepared frame data" << endl;
        }
        
        fft_frame_prepare_done_event.notify();
    }
}

// ============================================
// 2D FFT处理流程（修复版）
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_single_2D_process() {
    while (true) {
        wait(single_2d_start_event);
        
        cout << "\n[2D-FFT] Starting 2D decomposition..." << endl;
        
        // 初始化2D矩阵
        initialize_2d_matrices();

        //增加数据传输延时模拟,使用DMA的点对点传输的操作,仿真从AM读入到in_buf_vec的延时
        
        // 三阶段处理
        process_2d_stage1_column_fft();
        process_2d_stage2_twiddle();
        process_2d_stage3_row_fft();
        
        // 结果整理
        finalize_2d_results();
        
        // 执行最终验证
        perform_final_verification();
        
        cout << "[2D-FFT] 2D decomposition completed" << endl;
        single_2d_done_event.notify();
    }
}

template <typename T>
void FFT_Initiator<T>::initialize_2d_matrices() {
    cout << "  [2D-INIT] Initializing matrices (N1=" << N1 << ", N2=" << N2 << ")" << endl;
    
    frame_G_matrix[current_frame_id].assign(N2, vector<complex<T>>(N1, complex<T>(0,0)));
    frame_H_matrix[current_frame_id].assign(N2, vector<complex<T>>(N1, complex<T>(0,0)));
    frame_X_matrix[current_frame_id].assign(N2, vector<complex<T>>(N1, complex<T>(0,0)));
}

template <typename T>
void FFT_Initiator<T>::process_2d_stage1_column_fft() {
    cout << "\n  [Stage 1] Column FFT Processing..." << endl;
    current_2d_stage = 1;
    
    auto& input_matrix = frame_data_matrix[current_frame_id];
    auto& G_matrix = frame_G_matrix[current_frame_id];
    
    // 对每一列进行N2点FFT
    for (current_column_id = 0; current_column_id < N1; current_column_id++) {
        cout << "    - Column " << current_column_id + 1 << "/" << N1 << ": ";
        
        // 提取列数据
        vector<complex<float>> column_data(N2);
        for (unsigned row = 0; row < N2; row++) {
            column_data[row] = complex<float>(
                input_matrix[row][current_column_id].real,
                input_matrix[row][current_column_id].imag
            );
        }
        
        // 直接调用FFT计算核心（不触发数据生成）
        vector<complex<float>> column_fft_result = this->perform_fft_core(column_data, N2);
        
        // 存储结果到G矩阵
        for (unsigned row = 0; row < N2; row++) {
            G_matrix[row][current_column_id] = complex<T>(
                column_fft_result[row].real,
                column_fft_result[row].imag
            );
        }
        
        cout << "completed" << endl;
    }
    
    cout << "  [Stage 1] All column FFTs completed" << endl;
}

template <typename T>
void FFT_Initiator<T>::process_2d_stage2_twiddle() {
    cout << "\n  [Stage 2] Twiddle Factor Compensation..." << endl;
    current_2d_stage = 2;
    
    auto& G_matrix = frame_G_matrix[current_frame_id];
    auto& H_matrix = frame_H_matrix[current_frame_id];
    
    // 应用旋转因子补偿: H(n2,k1) = W_M^(n2*k1) * G(n2,k1)
    for (unsigned n2 = 0; n2 < N2; n2++) {
        for (unsigned k1 = 0; k1 < N1; k1++) {
            complex<float> twiddle = FFTInitiatorUtils::compute_twiddle_factor(n2, k1, FFT_TLM_N);
            complex<float> G_val(G_matrix[n2][k1].real, G_matrix[n2][k1].imag);
            complex<float> H_val = twiddle * G_val;
            H_matrix[n2][k1] = complex<T>(H_val.real, H_val.imag);
        }
    }
    
    cout << "  [Stage 2] Twiddle compensation completed" << endl;
    cout << "    H_matrix values:" << endl;
    for (unsigned r = 0; r < N2; ++r) {
        cout << "      Row " << r << ": ";
        for (unsigned c = 0; c < N1; ++c) {
            cout << "(" << fixed << setprecision(2) << H_matrix[r][c].real << "," << H_matrix[r][c].imag << ") ";
        }
        cout << endl;
    }

    
}

template <typename T>
void FFT_Initiator<T>::process_2d_stage3_row_fft() {
    cout << "\n  [Stage 3] Row FFT Processing..." << endl;
    current_2d_stage = 3;
    
    auto& H_matrix = frame_H_matrix[current_frame_id];
    auto& X_matrix = frame_X_matrix[current_frame_id];
    
    // 对每一行进行N1点FFT
    for (current_row_id = 0; current_row_id < N2; current_row_id++) {
        cout << "    - Row " << current_row_id + 1 << "/" << N2 << ": " << endl;
        
        // 提取行数据
        vector<complex<float>> row_data(N1);
        for (unsigned col = 0; col < N1; col++) {
            row_data[col] = complex<float>(
                H_matrix[current_row_id][col].real,
                H_matrix[current_row_id][col].imag
            );
        }
        
        // 直接调用FFT计算核心（不触发数据生成）
        vector<complex<float>> row_fft_result = this->perform_fft_core(row_data, N1);
        
        // 存储结果到X矩阵
        for (unsigned col = 0; col < N1; col++) {
            X_matrix[current_row_id][col] = complex<T>(
                row_fft_result[col].real,
                row_fft_result[col].imag
            );
        }
        
        cout << "completed" << endl;
    }
    
    cout << "  [Stage 3] All row FFTs completed" << endl;
}

// ============================================
// 新增：纯FFT计算核心（不含数据准备）
// ============================================

template <typename T>
vector<complex<float>> FFT_Initiator<T>::perform_fft_core(const vector<complex<float>>& input, size_t fft_size) {
    
    // 确保输入向量大小正确
    vector<complex<float>> adjusted_input = input;
    if (adjusted_input.size() != fft_size) {
        adjusted_input.resize(fft_size);
    }

    // 检查是否需要重新配置硬件
    //很奇怪,每次计算必须配置一次,否则input_buf的读取无法启动
    // if (fft_size != last_configured_fft_size) {
    FFTConfiguration config = FFTInitiatorUtils::create_fft_configuration(FFT_TLM_N, fft_size);
    send_fft_configure_transaction(config);
    wait(sc_time(BaseInitiatorModel<T>::FFT_CONFIG_WAIT_CYCLES, SC_NS));
    // last_configured_fft_size = fft_size;
    // }
    // 调用基类的FFT执行函数（硬件仿真）
    return perform_fft(adjusted_input, fft_size);
}

// ============================================
// FFT计算核心流程（修改版）
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_computation_process() {
    while (true) {
        wait(fft_computation_start_event);
        
        cout << "\n[FFT-COMP] Starting computation..." << endl;
        
        // 对于2D模式的列/行处理，数据已经在各自的stage函数中处理
        // 这里只处理直接模式的计算
        if (!use_2d_decomposition || current_2d_stage == 0) {
            // 获取输入数据
            vector<complex<T>> input_data = frame_input_data[current_frame_id];
            
            // 执行FFT计算
            vector<complex<float>> complex_input(input_data.begin(), input_data.end());
            vector<complex<float>> complex_output = this->perform_fft_core(complex_input, single_frame_fft_size);
            
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
        }
        
        cout << "[FFT-COMP] Computation completed" << endl;
        fft_computation_done_event.notify();
    }
}

// ============================================
// 单帧处理流程（修改版）
// ============================================

template <typename T>
void FFT_Initiator<T>::FFT_single_frame_process() {
    while (true) {
        wait(single_frame_start_event);
        
        cout << "\n[SINGLE-FRAME] Starting single frame processing..." << endl;
        
        // 触发数据生成（如果尚未准备）
        if (!frame_data_ready) {
            fft_frame_prepare_event.notify();
            wait(fft_frame_prepare_done_event);
        }
        
        // 触发FFT计算
        fft_computation_start_event.notify();
        wait(fft_computation_done_event);
        
        // 触发验证（仅在非2D模式或2D处理完成时）
        if (!use_2d_decomposition) {
            fft_verification_start_event.notify();
            wait(fft_verification_done_event);
        }
        
        cout << "[SINGLE-FRAME] Single frame processing completed" << endl;
        single_frame_done_event.notify();
    }
}

// ============================================
// 辅助函数实现
// ============================================

template <typename T>
void FFT_Initiator<T>::finalize_2d_results() {
    // 将最终矩阵转换为输出向量
    auto final_matrix = frame_X_matrix[current_frame_id];
    vector<complex<T>> final_output = FFTInitiatorUtils::reshape_to_vector(final_matrix);
    frame_output_data[current_frame_id] = final_output;
    
    // 显示结果
    cout << "\n  Final 2D Output: ";
    for (size_t i = 0; i < min(final_output.size(), size_t(8)); i++) {
        cout << "(" << fixed << setprecision(2) 
             << final_output[i].real << "," << final_output[i].imag << ") ";
    }
    if (final_output.size() > 16) cout << "...";
    cout << endl;
}

template <typename T>
void FFT_Initiator<T>::perform_final_verification() {
    cout << "\n[2D-VERIFY] Performing final verification..." << endl;
    
    bool verification_passed = verify_frame_result(current_frame_id);
    frame_test_results[current_frame_id] = verification_passed;
    
    cout << "  Result: " << (verification_passed ? "PASS ✓" : "FAIL ✗") << endl;
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
// 原有辅助函数保持不变
// ============================================

template <typename T>
bool FFT_Initiator<T>::should_reconfigure_fft() {
    return single_frame_fft_size != last_configured_fft_size;
}

template <typename T>
void FFT_Initiator<T>::reconfigure_fft_hardware() {
    cout << "  [CONFIG] Reconfiguring FFT: " << last_configured_fft_size 
         << " -> " << single_frame_fft_size << " points" << endl;
    
    FFTConfiguration config = FFTInitiatorUtils::create_fft_configuration(FFT_TLM_N, single_frame_fft_size);
    send_fft_configure_transaction(config);
    
    wait(sc_time(BaseInitiatorModel<T>::FFT_CONFIG_WAIT_CYCLES, SC_NS));
    last_configured_fft_size = single_frame_fft_size;
}

template <typename T>
vector<complex<T>> FFT_Initiator<T>::generate_frame_test_data() {
    // 生成测试序列
    auto test_data = generate_test_sequence(
        real_single_fft_size,  // 使用完整的M点数据
        DataGenType::RANDOM, 
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
    uint64_t ddr_data_addr = FFTInitiatorUtils::calculate_ddr_address(current_frame_id, TEST_FFT_SIZE, DDR_BASE_ADDR);
    write_data_to_ddr(test_data, ddr_data_addr);
    
    // Step 2: 写入旋转因子到DDR
    uint64_t ddr_twiddle_addr = ddr_data_addr + TEST_FFT_SIZE * sizeof(complex<T>);
    write_twiddle_factors_to_ddr(ddr_twiddle_addr);
    
    // Step 3: DMA传输到AM
    uint64_t am_data_addr = FFTInitiatorUtils::calculate_am_address(current_frame_id, TEST_FFT_SIZE, AM_BASE_ADDR);
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
    auto twiddle_factors = calculate_twiddle_factors<float>(FFT_TLM_N);
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

// config + address helpers moved to utils

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

template <typename T>
void FFT_Initiator<T>::compute_reference_results(const vector<complex<T>>& test_data) {
    vector<complex<float>> complex_test_data(test_data.begin(), test_data.end());
    vector<complex<float>> complex_reference = compute_reference_dft(complex_test_data);
    frame_reference_data[current_frame_id] = vector<complex<T>>(complex_reference.begin(), 
                                                                 complex_reference.end());
}

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
    

    return compare_complex_sequences(fft_output, reference_dft,1e-3f,false);
    // 后续的比较逻辑可以根据需要添加，例如调用 compare_complex_sequences
}


// twiddle + reshape helpers moved to utils

// 模板实例化
template class FFT_Initiator<float>;
