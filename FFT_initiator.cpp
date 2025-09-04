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

using namespace std;
using namespace FFTTestUtils;

template <typename T>
void FFT_Initiator<T>::System_init_process(){
    cout << "System_init_process started at " << sc_time_stamp() << endl;
    // Initialize FFT configuration parameters  
    real_single_fft_size = TEST_FFT_SIZE; // FFT的点长
    test_frames_count = DEFAULT_TEST_FRAMES; //帧数

    setup_dmi(AM_BASE_ADDR, am_dmi, "AM");
    setup_dmi(SM_BASE_ADDR, sm_dmi, "SM");
    setup_dmi(DDR_BASE_ADDR, ddr_dmi, "DDR");
    setup_dmi(GSM_BASE_ADDR, gsm_dmi, "GSM");
    // Initialize FFT system
    initialize_fft_system();

    FFT_init_process_done_event.notify();
}

// ====== FFT Main Control Process Implementation ======
template <typename T>
void FFT_Initiator<T>::FFT_main_process() {
    cout << sc_time_stamp() << " [FFT-INITIATOR] =================================" << endl;
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
        
        // Trigger frame data generation
        fft_frame_prepare_event.notify();
        wait(fft_frame_prepare_done_event);
        
        // Trigger FFT computation
        fft_computation_start_event.notify();
        wait(fft_computation_done_event);

        // Trigger result verification
        fft_verification_start_event.notify();
        wait(fft_verification_done_event);
        
    }
    
    cout << sc_time_stamp() << " [FFT-INITIATOR] All frame processing completed" << endl;
    
}


// ====== FFT Frame Data Generation Process Implementation ======
template <typename T>
void FFT_Initiator<T>::FFT_frame_generation_process() {
    while (true) {
        wait(fft_frame_prepare_event);
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
             << "] Generating test data..." << endl;
        
        // Generate test data for current frame
        auto test_data = generate_test_sequence(real_single_fft_size, 
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
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
             << "] Starting FFT computation..." << endl;
        
        // Get input data for current frame
        vector<complex<T>> input_data = frame_input_data[current_frame_id];
        
        // Convert to complex<float> for FFT computation
        vector<complex<float>> complex_input(input_data.begin(), input_data.end());
        
        // Use FFT computation method from BaseInitiatorModel
        vector<complex<float>> complex_output = perform_fft(complex_input, 
                                                                        real_single_fft_size);
        // Convert back to template type
        vector<complex<T>> output_data(complex_output.begin(), complex_output.end());
        
        // Store output results
        frame_output_data[current_frame_id] = output_data;
        
        // Display output data
        cout << "  Output: ";
        for (const auto& val : output_data) {
            cout << "(" << fixed << setprecision(2) 
                    << val.real << "," << val.imag << ") ";
        }
        cout << endl;
        
        cout << sc_time_stamp() << " [FRAME " << current_frame_id + 1 
                << "] FFT computation completed successfully" << endl;
        
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
    // 1. FFT system reset
    cout << "  Executing system reset..." << endl;
    send_fft_reset_transaction();
    
    // 2. FFT configuration
    cout << "  Configuring FFT parameters..." << endl;
    FFTConfiguration config;
    config.fft_mode = true;
    config.fft_shift = 0;
    config.fft_conj_en = false;
    config.fft_size = FFT_TLM_N; //硬件规模
    config.fft_size_real = real_single_fft_size;

    // Initialize stage_bypass_en vector for the configured FFT 
    int hardware_stages = static_cast<int>(std::log2(FFT_TLM_N));  // 硬件支持的stage数
    int required_stages = static_cast<int>(std::log2(real_single_fft_size));   // 实际FFT需要的stage数
    config.stage_bypass_en.resize(hardware_stages, false);        // 默认：所有stage启用
    
    // Configure bypass for smaller FFT: bypass early stages when real_single_fft_size < FFT_TLM_N
    if (real_single_fft_size < config.fft_size) {
        int bypass_stages = hardware_stages - required_stages;     // 需要bypass的stage数
        cout << "  FFT bypass configuration: " << real_single_fft_size << "-point FFT on " 
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
    send_fft_configure_transaction(config);
    
    // Wait for configuration completion
    wait(sc_time(BaseInitiatorModel<T>::FFT_CONFIG_WAIT_CYCLES, SC_NS));
    
    // 3. Load twiddle factors
    cout << "  Loading twiddle factors..." << endl;
    send_fft_load_twiddles_transaction();
    
    // Wait for twiddle factor loading completion
    wait(sc_time(BaseInitiatorModel<T>::FFT_TWIDDLE_WAIT_CYCLES, SC_NS));
    
    test_initialization_done = true;
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



// ====== Template Instantiation ======
// Explicitly instantiate only complex float type since FFT requires complex numbers
template class FFT_Initiator<float>;