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
template <typename T = float>
struct FFT_Initiator : public BaseInitiatorModel<T> {
    
    // ====== Constructor and SystemC Process Registration ======
    SC_CTOR(FFT_Initiator) : BaseInitiatorModel<T>("FFT_Initiator") {
        // Register main SystemC processes
        SC_THREAD(System_init_process);
        SC_THREAD(FFT_main_process);           // Main control process
        SC_THREAD(FFT_frame_generation_process); // Frame data generation process
        SC_THREAD(FFT_computation_process);    // FFT computation process  
        SC_THREAD(FFT_verification_process);   // Result verification process - currently disabled
    }

public:
    // ====== Inherit BaseInitiatorModel Interfaces ======
    // Following gemm.h pattern (lines 153-159)
    using BaseInitiatorModel<T>::socket;
    using BaseInitiatorModel<T>::am_dmi;
    using BaseInitiatorModel<T>::sm_dmi;
    using BaseInitiatorModel<T>::ddr_dmi;
    using BaseInitiatorModel<T>::gsm_dmi;
    using BaseInitiatorModel<T>::setup_dmi;
    using BaseInitiatorModel<T>::write_complex_data_dmi_no_latency;
    using BaseInitiatorModel<T>::write_data_dmi_no_latency;
    using BaseInitiatorModel<T>::read_complex_data_dmi_no_latency;
    
    // Use FFT methods from BaseInitiatorModel
    using BaseInitiatorModel<T>::perform_fft;
    using BaseInitiatorModel<T>::send_fft_reset_transaction;
    using BaseInitiatorModel<T>::send_fft_configure_transaction;
    using BaseInitiatorModel<T>::send_fft_load_twiddles_transaction;
    
    // ====== Test Control Events ======
    sc_event FFT_init_process_done_event;        // Start FFT testing
    sc_event fft_frame_prepare_event;       // Frame data ready
    sc_event fft_frame_prepare_done_event;
    sc_event fft_computation_start_event;  // FFT computation completed
    sc_event fft_computation_done_event;  // FFT computation completed
    sc_event fft_verification_start_event; // Verification completed
    sc_event fft_verification_done_event;

    
    // ====== Test Configuration Parameters ======
    // FFT parameter configuration (based on FFT_TLM_test.cpp)
    
    static constexpr unsigned TEST_NUM_PE = TEST_FFT_SIZE/2;       // 4 processing elements
    static constexpr unsigned TEST_FIFO_DEPTH = 8;   // FIFO depth
    static constexpr unsigned DEFAULT_TEST_FRAMES = 1; // Default test frame count
    
    // Configurable test parameters
    unsigned test_frames_count;           // Number of test frames
    unsigned current_frame_id;            // Current frame ID being processed
    unsigned real_single_fft_size;                    // FFT size
    
    // ====== Test Data Management ======
    // Multi-frame test data storage
    map<unsigned, vector<complex<T>>> frame_input_data;   // Input data: frame_id -> input_data
    map<unsigned, vector<complex<T>>> frame_output_data;  // Output data: frame_id -> output_data
    map<unsigned, vector<complex<T>>> frame_reference_data; // Reference data: frame_id -> reference_dft
    
    // Test result statistics
    vector<bool> frame_test_results;      // Test result for each frame
    int total_frames_tested;              // Total number of frames tested
    int frames_passed;                    // Number of frames that passed
    int frames_failed;                    // Number of frames that failed
    
    // ====== Test State Control ======
    bool test_initialization_done;       // Initialization completed flag
    bool current_frame_ready;            // Current frame data ready flag
    bool current_computation_done;       // Current computation completed flag
    bool current_verification_done;      // Current verification completed flag
    bool all_tests_completed;            // All tests completed flag
    

private:
    // ====== SystemC Process Declarations ======
    void FFT_main_process();              // Main control flow
    void System_init_process();
    void FFT_frame_generation_process();  // Frame data generation
    void FFT_computation_process();       // FFT computation execution
    void FFT_verification_process();      // Result verification
    
    // ====== Helper Method Declarations ======
    void initialize_fft_system();         // FFT system initialization
    bool verify_frame_result(unsigned frame_id);           // Verify single frame result

};

#endif // FFT_INITIATOR_H