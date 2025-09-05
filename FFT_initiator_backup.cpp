/**
 * @file FFT_initiator.cpp  
 * @brief FFT_TLM Multi-Frame Test Program Implementation - Test Initiator inheriting from BaseInitiatorModel
 * 
 * Implements all methods declared in FFT_initiator.h, providing complete multi-frame FFT testing functionality
 * 
 * @version 1.0 - Initial implementation based on BaseInitiatorModel
 * @date 2025-08-31
 */

#include "FFT_initiator.h"
#include "util/const.h"
#include "util/tools.h"
#include <cmath>  // 为M_PI常量

using namespace std;
using namespace FFTTestUtils;

template <typename T>
void FFT_Initiator<T>::System_init_process(){
    cout << "System_init_process started at " << sc_time_stamp() << endl;
    // Initialize FFT configuration parameters  
    
    // 配置FFT测试点数 - 支持16点和M点FFT
    // 要测试M点FFT，将下面的注释切换
    M = 16;
    // real_single_fft_size = 4;  // 4点FFT测试 (默认)
    real_single_fft_size = M;          // M点FFT测试 (使用2D分解)
    
    test_frames_count = DEFAULT_TEST_FRAMES; //帧数
    
    // Initialize FFT size tracking variables
    single_frame_fft_size = real_single_fft_size;  // Set current single frame FFT size
    last_configured_fft_size = 0;                   // Initialize to 0 to force initial configuration
    
    cout << "  Configured for " << real_single_fft_size << "-point FFT testing" << endl;

    setup_dmi(AM_BASE_ADDR, am_dmi, "AM");
    setup_dmi(SM_BASE_ADDR, sm_dmi, "SM");
    setup_dmi(DDR_BASE_ADDR, ddr_dmi, "DDR");
    setup_dmi(GSM_BASE_ADDR, gsm_dmi, "GSM");
    // Initialize FFT system
    // 1. FFT system reset
    cout << "  Executing system reset..." << endl;
    send_fft_reset_transaction();
    
    FFTConfiguration config;
    config.fft_mode = true;
    config.fft_shift = 0;
    config.fft_conj_en = false;
    config.fft_size = FFT_TLM_N; //硬件规模，由util/const.h中定义，不变

    // Initialize stage_bypass_en vector for the configured FFT 
    int hardware_stages = static_cast<int>(std::log2(FFT_TLM_N));  // 硬件支持的stage数
    config.stage_bypass_en.resize(hardware_stages, false);        // 默认：所有stage启用
    send_fft_configure_transaction(config);

    // 3. Load twiddle factors
    cout << "  Loading twiddle factors..." << endl;
    send_fft_load_twiddles_transaction();
    
    // Wait for twiddle factor loading completion
    wait(sc_time(BaseInitiatorModel<T>::FFT_TWIDDLE_WAIT_CYCLES, SC_NS));
    
    test_initialization_done = true;

    FFT_init_process_done_event.notify();
}

// ====== FFT Frame Loop Process Implementation ======
/**
 * @brief Frame Loop Process - handles outer frame traversal (non-looping)
 * This function processes each frame initialization and calls 2D processing
 * Simplified version that delegates complex 2D logic to FFT_single_2D_process
 */
template <typename T>
void FFT_Initiator<T>::FFT_frame_loop_process() {
    cout << sc_time_stamp() << " [FFT-FRAME-LOOP] =================================" << endl;
    cout << "FFT Multi-Frame Test Starting..." << endl;
    wait(FFT_init_process_done_event);
    
    // Start multi-frame processing loop
    for (unsigned frame = 0; frame < test_frames_count; frame++) {
        current_frame_id = frame;
        
        cout << "\n" << sc_time_stamp() << " [FRAME " << frame + 1 
                << "/" << test_frames_count << "] ===========================================" << endl;
        
        // Reset frame-level state flags
        current_computation_done = false; 
        current_verification_done = false;
        
        // Check if we need 2D decomposition processing
        if (real_single_fft_size == M) {
            cout << "  [FFT-FRAME-LOOP] Triggering 2D decomposition processing..." << endl;
            N1 = 4;
            N2 = 4;
            // Trigger 2D processing - this will handle all the complex 2D decomposition logic
            single_2d_start_event.notify();
            wait(single_2d_done_event);
            
            cout << sc_time_stamp() << " [FFT-FRAME-LOOP] 2D decomposition completed for frame " 
                 << current_frame_id + 1 << endl;
        } else {
            cout << "  [FFT-FRAME-LOOP] Single frame processing (no 2D decomposition needed)..." << endl;
            
            // For non-2D cases, trigger single frame processing directly
            single_frame_start_event.notify();
            wait(single_frame_done_event);
            
            cout << sc_time_stamp() << " [FFT-FRAME-LOOP] Single frame processing completed for frame " 
                 << current_frame_id + 1 << endl;
        }
    }
    
    cout << sc_time_stamp() << " [FFT-FRAME-LOOP] All frame processing completed" << endl;
}


// ====== FFT Frame Data Generation Process Implementation ======
template <typename T>
void FFT_Initiator<T>::FFT_frame_generation_process() {
    while (true) {
        wait(fft_frame_prepare_event);
                 
        FFTConfiguration config;
        config.fft_mode = true;
        config.fft_shift = 0;
        config.fft_conj_en = false;
        config.fft_size = FFT_TLM_N; //硬件规模，由util/const.h中定义，不变
        config.fft_size_real = single_frame_fft_size;  //此次单帧的点数

        // Initialize stage_bypass_en vector for the configured FFT 
        int hardware_stages = static_cast<int>(std::log2(FFT_TLM_N));  // 硬件支持的stage数
        int required_stages = static_cast<int>(std::log2(config.fft_size_real));   // 实际FFT需要的stage数
        config.stage_bypass_en.resize(hardware_stages, false);        // 默认：所有stage启用
        
        // Configure bypass for smaller FFT: bypass early stages when config.fft_size_real < FFT_TLM_N
        if (config.fft_size_real < config.fft_size) {
            int bypass_stages = hardware_stages - required_stages;     // 需要bypass的stage数
            cout << "  FFT bypass configuration: " << config.fft_size_real << "-point FFT on " 
                << FFT_TLM_N << "-point hardware" << endl;
            cout << "  Bypassing " << bypass_stages << " early stages (Stage0";
            
            // Bypass early stages (Stage0, Stage1, ...)
            for (int i = 0; i < bypass_stages; i++) {
                config.stage_bypass_en[i] = true;
                if (i > 0) cout << ", Stage" << i;
            }
            cout << ")" << endl;
            cout << "  Active stages: Stage" << bypass_stages << " to Stage" << (hardware_stages-1) << endl;
        } else {
            cout << "  Full " << FFT_TLM_N << "-point FFT: all stages active" << endl;
        }

        //  FFT configuration - only reconfigure if single_frame_fft_size has changed
        if (single_frame_fft_size != last_configured_fft_size) {
            cout << "  FFT size changed from " << last_configured_fft_size 
                << " to " << single_frame_fft_size << " points, reconfiguring..." << endl;
        
        send_fft_configure_transaction(config);
        
        // Wait for configuration completion
        wait(sc_time(BaseInitiatorModel<T>::FFT_CONFIG_WAIT_CYCLES, SC_NS));
        
        // Update last configured size to current size
        last_configured_fft_size = single_frame_fft_size;
        cout << "  FFT configuration completed for " << single_frame_fft_size << "-point FFT" << endl;
        } else {
            cout << "  FFT configuration unchanged (" << single_frame_fft_size << "-point), skipping reconfiguration" << endl;
        }
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
             << "] Generating test data..." << endl;
        
        // Generate test data for current frame
        auto test_data = generate_test_sequence(config.fft_size_real, 
                                               DataGenType::SEQUENTIAL, 
                                               current_frame_id+1);

        // 完善全系统的数据访存和计算逻辑
        
        // 1.1 将test_data的数据写入到DDR中
        uint64_t fft_input_data_addr = DDR_BASE_ADDR + current_frame_id * TEST_FFT_SIZE * sizeof(complex<T>); // 为每帧预留空间
        vector<complex<T>> complex_test_data(test_data.begin(), test_data.end());
        write_complex_data_dmi_no_latency(fft_input_data_addr, complex_test_data, complex_test_data.size(), this->ddr_dmi);
        
        // 1.2 使用calculate_twiddle_factors计算旋转因子，写入DDR
        uint64_t twiddle_factors_addr = fft_input_data_addr + TEST_FFT_SIZE * sizeof(complex<T>); // 紧跟在数据后面
        auto twiddle_factors = calculate_twiddle_factors<float>(TEST_FFT_SIZE);
        vector<complex<T>> complex_twiddle_factors(twiddle_factors.begin(), twiddle_factors.end());
        write_complex_data_dmi_no_latency(twiddle_factors_addr, complex_twiddle_factors, complex_twiddle_factors.size(), this->ddr_dmi);
        
        // 2 使用DMA点对点传输，数据从DDR传输到AM中（分两次：数据和旋转因子）
        uint64_t am_data_addr = AM_BASE_ADDR + current_frame_id * TEST_FFT_SIZE * sizeof(complex<T>); // AM中为每帧预留更多空间
        uint64_t am_twiddle_addr = am_data_addr + TEST_FFT_SIZE * sizeof(complex<T>);
        
        ins::dma_p2p_trans(this->socket, 
                          fft_input_data_addr, 0, complex_test_data.size()*sizeof(complex<T>),1 ,
                          am_data_addr, 0, complex_test_data.size()*sizeof(complex<T>), 1);
        
        // 传输旋转因子
        ins::dma_p2p_trans(this->socket,
                          twiddle_factors_addr, 0, complex_twiddle_factors.size()*sizeof(complex<T>),1, 
                          am_twiddle_addr, 0, complex_twiddle_factors.size()*sizeof(complex<T>),1);
        
        // 3 使用read_from_dmi方法，从AM中读出数据，模拟AM向FFT_TLM的传输时间
        vector<complex<T>> data_read_from_AM;
        vector<complex<T>> twiddle_read_from_AM;
        
        // 读取测试数据
        ins::read_from_dmi<complex<T>>(am_data_addr, data_read_from_AM, this->am_dmi, complex_test_data.size());
        
        // 读取旋转因子
        ins::read_from_dmi<complex<T>>(am_twiddle_addr, twiddle_read_from_AM, this->am_dmi, complex_twiddle_factors.size());
        
        // 将用到的数据存放到这个向量中
        frame_input_data[current_frame_id] = data_read_from_AM;
        
        // Compute reference DFT results
        // vector<complex<float>> complex_test_data(test_data.begin(), test_data.end());
        vector<complex<float>> complex_reference = compute_reference_dft(complex_test_data);
        vector<complex<float>> reference_result(complex_reference.begin(), complex_reference.end());
        frame_reference_data[current_frame_id] = reference_result;
        
        // Display generated input data
        cout << "  Input: ";
        for (const auto& val : test_data) {
            cout << "(" << fixed << setprecision(1) 
                 << val.real << "," << val.imag << ") ";
        }
        cout << endl;
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
             << "] Test data generation completed" << endl;
        fft_frame_prepare_done_event.notify();
    }
}

// ====== FFT Computation Execution Process Implementation ======
template <typename T>
void FFT_Initiator<T>::FFT_computation_process() {
    while (true) {
        wait(fft_computation_start_event);
        
        cout << sc_time_stamp() << " [FFT-COMP] Starting event-driven 2D decomposition for frame " 
             << current_frame_id + 1 << endl;
        
        // Get input data for current frame
        vector<complex<T>> input_data = frame_input_data[current_frame_id];
        unsigned input_size = input_data.size();
        
        cout << "  Input size: " << input_size << " points" << endl;
        
        // ====== Direct Hardware FFT ======
        cout << "  Using " << input_size << "-point direct hardware FFT..." << endl;
        
        // Convert to complex<float> for FFT computation
        vector<complex<float>> complex_input(input_data.begin(), input_data.end());
        
        // Use direct FFT computation method from BaseInitiatorModel
        vector<complex<float>> complex_output = perform_fft(complex_input, single_frame_fft_size);
        
        // Convert back to template type and store results
        vector<complex<T>> output_data(complex_output.begin(), complex_output.end());
        frame_output_data[current_frame_id] = output_data;
        
        // Display output data
        cout << "  Output: ";
        for (const auto& val : output_data) {
            cout << "(" << fixed << setprecision(2) 
                    << val.real << "," << val.imag << ") ";
        }
        cout << endl;
        
        cout << sc_time_stamp() << " [FFT-COMP] Direct FFT computation completed successfully" << endl;
        fft_computation_done_event.notify();
    }
}

// ====== FFT Result Verification Process Implementation ======
template <typename T>
void FFT_Initiator<T>::FFT_verification_process() {
    while (true) {
        wait(fft_verification_start_event);
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
             << "] Verifying FFT results..." << endl;
        
        // Execute result verification
        bool verification_passed = verify_frame_result(current_frame_id);
    
        // Update test results
        frame_test_results[current_frame_id] = verification_passed;
        
        cout << "  DFT Comparison: " << (verification_passed ? "PASS" : "FAIL") << endl;
        
        // Mark current verification as done
        current_verification_done = true;
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
             << "] Result verification completed" << endl;
        fft_verification_done_event.notify();
    }
}

// ====== Helper Methods Implementation ======

template <typename T>
void FFT_Initiator<T>::initialize_fft_system() {
    cout << sc_time_stamp() << " [FFT-INITIATOR] Initializing FFT system..." << endl;
    
    cout << sc_time_stamp() << " [FFT-INITIATOR] FFT system initialization completed" << endl;
        
}

template <typename T>
bool FFT_Initiator<T>::verify_frame_result(unsigned frame_id) {
   
    // Initialize frame_test_results if not already done
    if (frame_test_results.empty()) {
        frame_test_results.resize(DEFAULT_TEST_FRAMES, false);
    }
    
    // Get FFT output and reference DFT results
    vector<complex<T>> fft_output = frame_output_data[frame_id];
    vector<complex<T>> reference_dft = frame_reference_data[frame_id];
    
    // Convert reference to complex<float> for comparison
    vector<complex<float>> reference_complex(reference_dft.begin(), reference_dft.end());
    
    if (fft_output.size() != reference_dft.size()) {
        cout << "  ERROR: Size mismatch - FFT:" << fft_output.size() 
             << " vs Reference:" << reference_dft.size() << endl;
        return false;
    }
    
    // Convert FFT output to standard complex format for comparison
    vector<complex<float>> fft_std_output(fft_output.begin(), fft_output.end());
    vector<complex<float>> fft_natural_order(fft_std_output.size());
    // Apply bit-reversal to FFT output to get natural order
    int fft_size = fft_std_output.size();
    for (size_t i = 0; i < fft_size/2; i++) {
        fft_natural_order[i] = fft_std_output[2*i];
        fft_natural_order[i+fft_size/2] = fft_std_output[2*i+1];
    }
    
    // Use the bit-reversed output for comparison
    fft_std_output = fft_natural_order;
    
    // Use relaxed tolerance for TLM simulation comparison
    const float tolerance = 0.1f;
    
    bool comparison_passed = compare_complex_sequences(fft_std_output, 
                                                      reference_complex, 
                                                      tolerance, 
                                                      false);
    
    return comparison_passed;
    cout << "comparison_passed" << comparison_passed << endl;
}


/**
 * @brief Twiddle Factor Compensation Process - applies compensation factors to all matrix elements
 */
template <typename T>
void FFT_Initiator<T>::FFT_twiddle_process() {
    while (true) {
        wait(twiddle_compensation_start_event);
        
        cout << sc_time_stamp() << " [FFT-TWIDDLE] Applying twiddle factor compensation" << endl;
        
        // Get matrices for current frame
        auto& G_matrix = frame_G_matrix[current_frame_id];
        auto& H_matrix = frame_H_matrix[current_frame_id];
        
        // Apply twiddle factor compensation: H(k2,n1) = W_M^(k2*n1) * G(k2,n1)
        for (unsigned k2 = 0; k2 < N2; k2++) {
            for (unsigned n1 = 0; n1 < N1; n1++) {
                // Compute twiddle factor W_M^(k2*n1)
                complex<float> twiddle = compute_twiddle_factor(k2, n1, M);
                
                // Apply compensation
                complex<float> G_val(G_matrix[k2][n1].real, G_matrix[k2][n1].imag);
                complex<float> H_val = twiddle * G_val;
                
                H_matrix[k2][n1] = complex<T>(H_val.real, H_val.imag);
            }
        }
        
        cout << sc_time_stamp() << " [FFT-TWIDDLE] Twiddle factor compensation completed" << endl;
        
        twiddle_compensation_done_event.notify();
    }
}

/**
 * @brief Row FFT Processing Process - handles hardware FFT calls for each row
 */
template <typename T>
void FFT_Initiator<T>::FFT_row_process() {
    while (true) {
        wait(row_fft_start_event);
        
        cout << sc_time_stamp() << " [FFT-ROW] Processing row " << current_row_id 
             << " with " << N1 << "-point hardware FFT" << endl;
        
        // Get matrices for current frame
        auto& H_matrix = frame_H_matrix[current_frame_id];
        auto& X_matrix = frame_X_matrix[current_frame_id];
        
        // Extract row data (N1=4 elements from row current_row_id)
        vector<complex<float>> row_data(N1);
        for (unsigned col = 0; col < N1; col++) {
            row_data[col] = complex<float>(H_matrix[current_row_id][col].real, 
                                         H_matrix[current_row_id][col].imag);
        }
        
        cout << "    Row " << current_row_id << " input: ";
        for (const auto& val : row_data) {
            cout << "(" << fixed << setprecision(2) 
                 << val.real << "," << val.imag << ") ";
        }
        cout << endl;
        
        // Perform N1-point hardware FFT on this row
        vector<complex<float>> row_fft_result = perform_fft(row_data, N1);
        
        cout << "    Row " << current_row_id << " output: ";
        for (const auto& val : row_fft_result) {
            cout << "(" << fixed << setprecision(2) 
                 << val.real << "," << val.imag << ") ";
        }
        cout << endl;
        
        // Store results back to X matrix
        for (unsigned col = 0; col < N1; col++) {
            X_matrix[current_row_id][col] = complex<T>(row_fft_result[col].real, 
                                                     row_fft_result[col].imag);
        }
        
        cout << sc_time_stamp() << " [FFT-ROW] Row " << current_row_id 
             << " processing completed" << endl;
        
        row_fft_done_event.notify();
    }
}

// ====== 2D FFT Decomposition Helper Methods Implementation ======
/**
 * @brief 计算补偿旋转因子 W_N^(k2*n1)
 */
template <typename T>
complex<float> FFT_Initiator<T>::compute_twiddle_factor(int k2, int n1, int N) {
    float angle = -2.0f * M_PI * k2 * n1 / N;
    return complex<float>(cos(angle), sin(angle));
}

/**
 * @brief 将一维向量重排为二维矩阵
 */
template <typename T>
vector<vector<complex<T>>> FFT_Initiator<T>::reshape_to_matrix(const vector<complex<T>>& input, int rows, int cols) {
    if (input.size() != rows * cols) {
        cout << "ERROR: reshape size mismatch - input size: " << input.size() 
             << ", expected: " << rows * cols << endl;
        return vector<vector<complex<T>>>(rows, vector<complex<T>>(cols, complex<T>(0, 0)));
    }
    
    vector<vector<complex<T>>> matrix(rows, vector<complex<T>>(cols));
    
    // 按行主序重排: input[n] -> matrix[n/cols][n%cols]
    for (int i = 0; i < input.size(); ++i) {
        int row = i / cols;
        int col = i % cols;
        matrix[row][col] = input[i];
    }
    
    return matrix;
}

/**
 * @brief 将二维矩阵重排为一维向量
 */
template <typename T>
vector<complex<T>> FFT_Initiator<T>::reshape_to_vector(const vector<vector<complex<T>>>& matrix) {
    if (matrix.empty() || matrix[0].empty()) {
        cout << "ERROR: empty matrix in reshape_to_vector" << endl;
        return vector<complex<T>>();
    }
    
    int rows = matrix.size();
    int cols = matrix[0].size();
    vector<complex<T>> output;
    output.reserve(rows * cols);
    
    // 按行主序展开: matrix[row][col] -> output[row*cols + col]
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            output.push_back(matrix[row][col]);
        }
    }
    
    return output;
}

// ====== Single Frame Processing SC_THREAD Implementation ======
/**
 * @brief Single Frame Processing Process - handles single frame processing with while(true) structure
 * This function controls the 62-72 line single frame processing flow:
 * - Frame data generation (fft_frame_prepare_event)
 * - FFT computation (fft_computation_start_event) - hardware simulation
 * - Result verification (fft_verification_start_event)
 */
template <typename T>
void FFT_Initiator<T>::FFT_single_frame_process() {
    while (true) {
        wait(single_frame_start_event);
        
        cout << sc_time_stamp() << " [SINGLE-FRAME] Starting single frame processing..." << endl;
        
        // Reset frame-level state flags
        current_computation_done = false; 
        current_verification_done = false;
        
        // Trigger frame data generation
        fft_frame_prepare_event.notify();
        wait(fft_frame_prepare_done_event);
        
        // Trigger FFT computation - hardware simulation
        fft_computation_start_event.notify();
        wait(fft_computation_done_event);

        // Trigger result verification
        fft_verification_start_event.notify();
        wait(fft_verification_done_event);
        
        cout << sc_time_stamp() << " [SINGLE-FRAME] Single frame processing completed" << endl;
        
        // Notify completion to 2D process
        single_frame_done_event.notify();
    }
}

// ====== Single 2D Processing SC_THREAD Implementation ======
/**
 * @brief Single 2D Processing Process - handles 2D decomposition with while(true) structure
 * This function encapsulates the three-stage 2D decomposition flow:
 * - Stage 1: Column FFT processing (N1 columns of N2-point FFT)
 * - Stage 2: Twiddle factor compensation
 * - Stage 3: Row FFT processing (N2 rows of N1-point FFT)
 * Each N1-point column FFT computation triggers FFT_single_frame_process() via notify
 */
template <typename T>
void FFT_Initiator<T>::FFT_single_2D_process() {
    while (true) {
        wait(single_2d_start_event);
        
        cout << sc_time_stamp() << " [SINGLE-2D] Starting 2D decomposition processing..." << endl;
        
        // ==================== BUG FIX: Initialize 2D matrices ====================
        // 在访问前为当前帧的中间矩阵分配内存，防止段错误
        // 这是导致段错误的核心原因，因为map在访问时会创建空向量
        cout << "  [SINGLE-2D] Initializing intermediate matrices for frame " << current_frame_id 
             << " (N1=" << N1 << ", N2=" << N2 << ")" << endl;
        
        //中间结果存储向量
        frame_data_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
        frame_G_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
        frame_H_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
        frame_X_matrix[current_frame_id].assign(N1, vector<complex<T>>(N2, complex<T>(0,0)));
        
        cout << "  [SINGLE-2D] Matrices initialized successfully." << endl;
        // ========================================================================


        // Wait for computation to initialize data structures
        wait(sc_time(10, SC_NS));  // Brief delay for computation setup
        
        // Check if we need 2D decomposition processing
        if (real_single_fft_size == M) {
            cout << "  [SINGLE-2D] Coordinating 2D decomposition stages..." << endl;
            
            // ====== Stage 1: Column FFT Processing ======
            cout << "  [Stage 1] Column FFT: Processing " << N1 << " columns of " << N2 << "-point FFT..." << endl;
            current_2d_stage = 1;
            
            // Process each column sequentially with event-driven control
            for (current_column_id = 0; current_column_id < N1; current_column_id++) {
                cout << "    Processing rowumn " << current_column_id + 1 << "/" << N1 << endl;
                single_frame_fft_size = N1;
                // Trigger single frame processing for this column (hardware simulation)
                single_frame_start_event.notify();
                wait(single_frame_done_event);
                
            }
            cout << "  [Stage 1] All column FFTs completed" << endl;
            
            // ====== Stage 2: Twiddle Factor Compensation ======
            cout << "  [Stage 2] Twiddle factor compensation..." << endl;
            current_2d_stage = 2;
            twiddle_compensation_start_event.notify();
            wait(twiddle_compensation_done_event);
            cout << "  [Stage 2] Twiddle compensation completed" << endl;
            
            // ====== Stage 3: Row FFT Processing ======
            cout << "  [Stage 3] Row FFT: Processing " << N2 << " rows of " << N1 << "-point hardware FFT..." << endl;
            current_2d_stage = 3;
            
            // Process each row sequentially with event-driven control
            for (current_row_id = 0; current_row_id < N2; current_row_id++) {
                cout << " Processing row " << current_row_id + 1 << "/" << N2 << endl;
                single_frame_fft_size = N2;
                // Trigger single frame processing for this row (hardware simulation)
                single_frame_start_event.notify();
                wait(single_frame_done_event);
                
            }
            cout << "  [Stage 3] All row FFTs completed" << endl;
            
            // Convert final matrix result to output vector
            auto final_matrix = frame_X_matrix[current_frame_id];
            vector<complex<T>> final_output = reshape_to_vector(final_matrix);
            frame_output_data[current_frame_id] = final_output;
            
            // Display output data
            cout << "  Final Output: ";
            for (const auto& val : final_output) {
                cout << "(" << fixed << setprecision(2) 
                        << val.real << "," << val.imag << ") ";
            }
            cout << endl;
            
            cout << sc_time_stamp() << " [SINGLE-2D] 2D decomposition completed for frame " 
                 << current_frame_id + 1 << endl;
        }
        
        // Notify completion to frame loop process
        single_2d_done_event.notify();
    }
}

// ====== Template Instantiation ======
// Explicitly instantiate only complex float type since FFT requires complex numbers
template class FFT_Initiator<float>;