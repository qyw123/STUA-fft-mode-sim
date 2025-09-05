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
        SC_THREAD(FFT_frame_loop_process);     // Frame loop control process (renamed from FFT_main_process)
        SC_THREAD(FFT_frame_generation_process); // Frame data generation process
        SC_THREAD(FFT_computation_process);    // FFT computation process  
        SC_THREAD(FFT_verification_process);   // Result verification process - currently disabled
        
        // Register new hierarchical processing processes
        SC_THREAD(FFT_single_frame_process);   // Single frame processing (while(true) structure)
        SC_THREAD(FFT_single_2D_process);      // Single 2D decomposition processing (while(true) structure)
        
        // Register 2D FFT decomposition processes
        // SC_THREAD(FFT_column_process);         // Column FFT processing
        SC_THREAD(FFT_twiddle_process);        // Twiddle compensation processing
        // SC_THREAD(FFT_row_process);            // Row FFT processing
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
    
    // ====== 2D FFT Decomposition Events ======
    sc_event column_fft_start_event;        // Start processing a column  
    sc_event column_fft_done_event;         // Column FFT processing completed
    sc_event twiddle_compensation_start_event; // Start twiddle factor compensation
    sc_event twiddle_compensation_done_event;  // Twiddle compensation completed
    sc_event row_fft_start_event;           // Start processing a row
    sc_event row_fft_done_event;            // Row FFT processing completed

    // ====== New Hierarchical Processing Events ======
    sc_event single_frame_start_event;      // Start single frame processing (while(true) structure)
    sc_event single_frame_done_event;       // Single frame processing completed
    sc_event single_2d_start_event;         // Start 2D decomposition processing (while(true) structure)  
    sc_event single_2d_done_event;          // 2D decomposition processing completed

    
    // ====== Test Configuration Parameters ======
    // FFT parameter configuration (based on FFT_TLM_test.cpp)
    
    static constexpr unsigned TEST_NUM_PE = TEST_FFT_SIZE/2;       // 4 processing elements
    static constexpr unsigned TEST_FIFO_DEPTH = 8;   // FIFO depth
    static constexpr unsigned DEFAULT_TEST_FRAMES = 1; // Default test frame count
    
    // Configurable test parameters
    unsigned test_frames_count;           // Number of test frames
    unsigned current_frame_id;            // Current frame ID being processed
    unsigned real_single_fft_size;                    // FFT size
    unsigned M;                             //需要被分解处理的单帧点数
    unsigned single_frame_fft_size;      // Current single frame FFT size for hardware configuration
    unsigned last_configured_fft_size;   // Last configured FFT size to track changes
    
    // ====== 2D FFT Decomposition State Variables ======
    unsigned current_column_id;            // Current column being processed in 2D FFT
    unsigned current_row_id;               // Current row being processed in 2D FFT  
    unsigned current_2d_stage;             // Current 2D decomposition stage (1:column, 2:twiddle, 3:row)
    unsigned N1, N2;                       // 2D decomposition parameters (N1=rows, N2=cols)
    
    // ====== Test Data Management ======
    // Multi-frame test data storage
    map<unsigned, vector<complex<T>>> frame_input_data;   // Input data: frame_id -> input_data
    map<unsigned, vector<complex<T>>> frame_output_data;  // Output data: frame_id -> output_data
    map<unsigned, vector<complex<T>>> frame_reference_data; // Reference data: frame_id -> reference_dft
    
    // ====== 2D FFT Decomposition Intermediate Data ======
    // 2D decomposition intermediate data storage (per frame)
    map<unsigned, vector<vector<complex<T>>>> frame_data_matrix;  // Reshaped input matrix: frame_id -> matrix[row][col]
    map<unsigned, vector<vector<complex<T>>>> frame_G_matrix;     // Column FFT results: frame_id -> G_matrix[row][col]
    map<unsigned, vector<vector<complex<T>>>> frame_H_matrix;     // Twiddle compensated: frame_id -> H_matrix[row][col]  
    map<unsigned, vector<vector<complex<T>>>> frame_X_matrix;     // Row FFT results: frame_id -> X_matrix[row][col]
    
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
    void FFT_frame_loop_process();        // Frame loop control (renamed from FFT_main_process)
    void System_init_process();
    void FFT_frame_generation_process();  // Frame data generation
    void FFT_computation_process();       // FFT computation execution
    void FFT_verification_process();      // Result verification
    
    // ====== New Hierarchical Processing Process Declarations ======
    void FFT_single_frame_process();      // Single frame processing (while(true) structure)
    void FFT_single_2D_process();         // Single 2D decomposition processing (while(true) structure)
    
    // ====== 2D FFT Decomposition Process Declarations ======  
    // void FFT_column_process();            // Column FFT processing (4-point software FFT)
    void FFT_twiddle_process();           // Twiddle factor compensation processing
    void FFT_row_process();               // Row FFT processing (hardware FFT calls)
    
    // ====== Helper Method Declarations ======
    void initialize_fft_system();         // FFT system initialization
    bool verify_frame_result(unsigned frame_id);           // Verify single frame result
    
    // ====== 2D FFT Decomposition Helper Methods ======
    //vector<complex<float>> perform_4_point_fft_software(const vector<complex<float>>& input_4);
    complex<float> compute_twiddle_factor(int k2, int n1, int N);
    vector<vector<complex<T>>> reshape_to_matrix(const vector<complex<T>>& input, int rows, int cols);
    vector<complex<T>> reshape_to_vector(const vector<vector<complex<T>>>& matrix);

};

#endif // FFT_INITIATOR_H