/**
 * @file FFT_initiator.h
 * @brief 重构后的 FFT_TLM Multi-Frame Test Program - Test Initiator
 * 
 * 该程序实现了系统级的FFT_TLM测试，继承自 util/base_initiator_modle.h。
 * 新版本重构了代码结构，将2D分解逻辑和数据管理流程分离，提高了代码的可读性和可维护性。
 * 
 * Test Features:
 * - Inherits TLM interface and FFT methods from BaseInitiatorModel
 * - Multi-frame continuous FFT processing (configurable frame count)
 * - Event-driven test flow control
 * - Fully automated data generation, DMA transfer, computation, and verification
 * - Complete test statistics and reporting
 * 
 * @version 2.0 - Refactored version with clear separation of concerns
 * @date 2025-01-10
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
        SC_THREAD(FFT_frame_loop_process);       // 帧循环主控制进程
        SC_THREAD(FFT_frame_generation_process); // 帧数据生成与搬移进程
        SC_THREAD(FFT_computation_process);      // FFT硬件计算进程
        SC_THREAD(FFT_verification_process);     // 结果验证进程
        
        // Register hierarchical and decomposition processes
        SC_THREAD(FFT_single_frame_process);     // 单帧（直接模式）处理流程
        SC_THREAD(FFT_single_2D_process);        // 单帧（2D分解模式）处理流程
        //_THREAD(FFT_twiddle_process);          // 2D分解中的旋转因子补偿进程
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
    sc_event fft_frame_prepare_event;            // Trigger frame data generation
    sc_event fft_frame_prepare_done_event;       // Frame data generation completed
    sc_event fft_computation_start_event;        // Trigger FFT computation
    sc_event fft_computation_done_event;         // FFT computation completed
    sc_event fft_verification_start_event;       // Trigger verification
    sc_event fft_verification_done_event;        // Verification completed
    
    // ====== 2D FFT Decomposition Events ======
    sc_event twiddle_compensation_start_event; // Start twiddle factor compensation
    sc_event twiddle_compensation_done_event;  // Twiddle compensation completed

    // ====== Hierarchical Processing Events ======
    sc_event single_frame_start_event;      // Start single frame processing (while(true) structure)
    sc_event single_frame_done_event;       // Single frame processing completed
    sc_event single_2d_start_event;         // Start 2D decomposition processing (while(true) structure)  
    sc_event single_2d_done_event;          // 2D decomposition processing completed

    
    // ====== Test Configuration Parameters ======
    // FFT parameter configuration (based on FFT_TLM_test.cpp)
    int  TEST_FFT_SIZE = 8;
    static constexpr unsigned DEFAULT_TEST_FRAMES = 1; // Default test frame count
    
    // Configurable test parameters
    unsigned test_frames_count;           // Number of test frames
    unsigned current_frame_id;            // Current frame ID being processed
    unsigned real_single_fft_size;        // FFT size for the entire frame
    unsigned M;                           // Total points for FFT (e.g., 16)
    unsigned single_frame_fft_size;      // Current single frame FFT size for hardware configuration
    unsigned last_configured_fft_size;   // Last configured FFT size to track changes
    bool use_2d_decomposition;            // Flag to control processing mode
    bool frame_data_ready;                // Flag to indicate if frame data is ready
    
    // ====== 2D FFT Decomposition State Variables ======
    // These are used by the 2D process to track its internal state
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
    void FFT_frame_loop_process();
    void System_init_process();
    void FFT_frame_generation_process();
    void FFT_computation_process();
    void FFT_verification_process();
    
    // ====== Hierarchical Processing Process Declarations ======
    void FFT_single_frame_process();
    void FFT_single_2D_process();
    
    // ====== 2D FFT Decomposition Process Declarations ======
    //void FFT_twiddle_process();
    
    // ====== Helper Method Declarations ======
    // Initialization helpers
    void configure_test_parameters();
    void setup_memory_interfaces();
    void initialize_fft_hardware();
    FFTConfiguration create_fft_configuration(size_t hw_size, size_t real_size);

    // Frame loop helpers
    void reset_frame_state();
    void process_frame_2d_mode();
    void process_frame_direct_mode();
    void display_frame_result(unsigned frame_id);
    void display_final_statistics();

    // Data generation and movement helpers
    bool should_reconfigure_fft();
    void reconfigure_fft_hardware();
    vector<complex<T>> generate_frame_test_data();
    void prepare_frame_data_once();
    void perform_data_movement(const vector<complex<T>>& test_data);
    void write_data_to_ddr(const vector<complex<T>>& data, uint64_t addr);
    void write_twiddle_factors_to_ddr(uint64_t addr);
    void transfer_ddr_to_am(uint64_t src_addr, uint64_t dst_addr, size_t size);
    void read_data_from_am(uint64_t addr, size_t size);
    uint64_t calculate_ddr_address(unsigned frame_id);
    uint64_t calculate_am_address(unsigned frame_id);

    // 2D FFT decomposition helpers
    void initialize_2d_matrices();
    void process_2d_stage1_column_fft();
    void process_2d_stage2_twiddle();
    void process_2d_stage3_row_fft();
    void finalize_2d_results();
    
    // Verification and math helpers
    void compute_reference_results(const vector<complex<T>>& test_data);
    bool verify_frame_result(unsigned frame_id);
    vector<complex<float>> perform_fft_core(const vector<complex<float>>& input, size_t fft_size);
    void perform_final_verification();
    
    complex<float> compute_twiddle_factor(int k2, int n1, int N);
    vector<vector<complex<T>>> reshape_to_matrix(const vector<complex<T>>& input, int rows, int cols);
    vector<complex<T>> reshape_to_vector(const vector<vector<complex<T>>>& matrix);

};

#endif // FFT_INITIATOR_H