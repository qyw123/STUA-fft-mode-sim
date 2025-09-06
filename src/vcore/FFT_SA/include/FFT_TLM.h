/**
 * @file FFT_TLM.h
 * @brief FFT Processing Element Array TLM Wrapper - SystemC TLM-2.0 Interface
 * 
 * This module provides a TLM-2.0 wrapper for the PEA_FFT module, offering
 * high-level transaction-level modeling interface for SOC integration.
 * 
 * Key Features:
 * - TLM-2.0 standard compliant interface
 * - 16-way parallel data input/output
 * - Twiddle factor management
 * - Pipeline status monitoring
 * - Event-driven asynchronous operation
 * 
 * @version 1.0 - Initial TLM wrapper implementation
 * @date 2025-08-31
 */

#ifndef FFT_TLM_H
#define FFT_TLM_H

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "pea_fft.h"
#include "fft_multi_stage.h"  // For log2_const function
#include "../utils/complex_types.h"
#include "../utils/config.h"
#include <vector>
#include <iostream>

using namespace std;

// ========== FFT Command Definitions ==========
enum class FFTCommand {
    RESET_FFT_ARRAY,        // Reset FFT array system
    CONFIGURE_FFT_MODE,     // Configure FFT mode parameters
    LOAD_TWIDDLE_FACTORS,   // Load twiddle factors (batch operation)
    WRITE_INPUT_DATA,       // Write 16-way parallel input data
    START_FFT_PROCESSING,   // Start FFT computation pipeline
    READ_OUTPUT_DATA,       // Read 16-way parallel output data
    CHECK_PIPELINE_STATUS,  // Check pipeline processing status
    SET_FFT_PARAMETERS     // Set FFT size and configuration
};

// ========== TLM Extension for FFT-specific Information ==========
struct FFTExtension : public tlm::tlm_extension<FFTExtension> {
    FFTCommand cmd;           // Command type
    uint32_t stage_idx;       // Stage index (for twiddle factors)
    uint32_t pe_idx;          // Processing Element index
    uint32_t data_size;       // Data size in elements
    
    FFTExtension() : cmd(FFTCommand::RESET_FFT_ARRAY), 
                    stage_idx(0), pe_idx(0), data_size(0) {}
    
    virtual tlm::tlm_extension_base* clone() const {
        FFTExtension* ext = new FFTExtension();
        ext->cmd = this->cmd;
        ext->stage_idx = this->stage_idx;
        ext->pe_idx = this->pe_idx;
        ext->data_size = this->data_size;
        return ext;
    }
    
    virtual void copy_from(tlm::tlm_extension_base const& ext) {
        const FFTExtension& src = static_cast<const FFTExtension&>(ext);
        this->cmd = src.cmd;
        this->stage_idx = src.stage_idx;
        this->pe_idx = src.pe_idx;
        this->data_size = src.data_size;
    }
};

// ========== Data Structures for FFT Operations ==========
struct FFTData {
    vector<float> input_data;     // 16-way parallel input (real/imag interleaved)
    vector<float> output_data;    // 16-way parallel output
    vector<bool> input_valid;     // Input data validity
    vector<bool> output_valid;    // Output data validity
    bool processing_complete;     // Processing completion flag
    
    FFTData(unsigned size = 16) : 
        input_data(size, 0.0f), output_data(size, 0.0f),
        input_valid(size, false), output_valid(size, false),
        processing_complete(false) {}
};

struct FFTConfiguration {
    bool fft_mode;               // FFT mode enable
    sc_uint<4> fft_shift;        // FFT shift control
    bool fft_conj_en;            // FFT conjugate enable
    vector<bool> stage_bypass_en;        // Stage bypass enable
    unsigned fft_size;           // FFT size (N) 真实的阵列规模
    unsigned fft_size_real;
    
    FFTConfiguration() : fft_mode(true), fft_shift(0), fft_conj_en(false),
                        stage_bypass_en(), fft_size(8) , fft_size_real(32){
        // Initialize stage_bypass_en with appropriate size (assume log2(fft_size) stages)
        int num_stages = static_cast<int>(std::log2(fft_size));
        stage_bypass_en.resize(num_stages, false);
    }
};

// ========== Main FFT TLM Module ==========
template<typename T = float,
         unsigned N = 8,
         int FIFO_DEPTH = 8>
SC_MODULE(FFT_TLM) {
    // ========== Template Constants ==========
    static constexpr int NUM_PE = N/2;
    static constexpr int NUM_FIFOS = NUM_PE * 4;  // 16 FIFOs for 8-point FFT
    
    // ========== Event Notification Address Constants ==========
    static constexpr uint64_t FFT_EVENT_BASE_ADDR = 0xFFFF0000;     // 事件通知基地址
    static constexpr uint64_t FFT_INPUT_READY_ADDR = 0xFFFF0001;    // 输入数据写入完成事件地址
    static constexpr uint64_t FFT_RESULT_READY_ADDR = 0xFFFF0002;   // FFT计算完成事件地址
    static constexpr uint64_t FFT_OUTPUT_READY_ADDR = 0xFFFF0003;   // 输出数据读取完成事件地址
    
    // ========== TLM Interfaces ==========
    tlm_utils::multi_passthrough_target_socket<FFT_TLM, 512> spu2fft_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<FFT_TLM, 512> fft2vcore_init_socket;
    
    // ========== Internal Clock and Reset ==========
    sc_clock internal_clk{"internal_clk", sc_time(1, SC_NS)};
    sc_signal<bool> internal_rst{"internal_rst"};
    
    // ========== Internal PEA_FFT Interface Signals ==========
    // Input Buffer Interface
    sc_vector<sc_signal<T>> data_i_vec{"data_i_vec", NUM_FIFOS};
    sc_signal<bool> wr_start_i{"wr_start_i"};
     sc_vector<sc_signal<bool>> wr_en_i{"wr_en_i", NUM_FIFOS};
    sc_vector<sc_signal<bool>> wr_ready_o_vec{"wr_ready_o_vec", NUM_FIFOS};
    
    // FFT Processing Control
    sc_signal<bool> fft_mode_i{"fft_mode_i"};
    sc_signal<sc_uint<4>> fft_shift_i{"fft_shift_i"};
    sc_signal<bool> fft_conj_en_i{"fft_conj_en_i"};
    sc_vector<sc_signal<bool>> stage_bypass_en{"stage_bypass_en", static_cast<size_t>(log2_const(N))};
    sc_signal<bool> fft_start_i{"fft_start_i"};
    sc_signal<bool> input_ready_o{"input_ready_o"};
    sc_signal<bool> input_empty_o{"input_empty_o"};
    sc_signal<int> fft_size_real{"fft_size_real"};
    
    // Output Buffer Interface
    sc_signal<bool> rd_start_i{"rd_start_i"};
    sc_signal<bool> output_ready_o{"output_ready_o"};
    sc_signal<bool> output_empty_o{"output_empty_o"};
    sc_vector<sc_signal<T>> data_o_vec{"data_o_vec", NUM_FIFOS};
    sc_vector<sc_signal<bool>> rd_valid_o_vec{"rd_valid_o_vec", NUM_FIFOS};
    sc_vector<sc_signal<bool>> wr_ready_out_vec{"wr_ready_out_vec", NUM_FIFOS};
    
    // Twiddle Factor Interface
    sc_signal<bool> tw_load_en{"tw_load_en"};
    sc_signal<sc_uint<8>> tw_stage_idx{"tw_stage_idx"};
    sc_signal<sc_uint<8>> tw_pe_idx{"tw_pe_idx"};
    sc_signal<complex<T>> tw_data{"tw_data"};
    
    // ========== Internal PEA_FFT Instance ==========
    PEA_FFT<T, N, FIFO_DEPTH>* pea_fft_core;
    
    // ========== Control and Status Management ==========
    sc_mutex access_mutex;                    // Protect concurrent access
    FFTConfiguration current_config;          // Current FFT configuration
    
    // ========== Timing Configuration ==========
    sc_time clock_period;                     // Clock period for cycle-based timing
    
    // ========== Event-Driven Synchronization ==========
    sc_event reset_complete_event;
    sc_event config_complete_event;
    sc_event twiddle_load_complete_event;
    sc_event input_write_complete_event;
    sc_event fft_processing_complete_event;
    sc_event output_read_complete_event;
    sc_event output_read_complete_done_event;
    
    // ========== Single Data Storage ==========
    FFTData current_data;
    
    // ========== Status Flags ==========
    bool system_initialized;
    bool config_loaded;
    bool twiddles_loaded;
    bool pipeline_busy;
    
    // ========== Constructor ==========
    SC_HAS_PROCESS(FFT_TLM);
    
    FFT_TLM(sc_module_name name, sc_time clock_period = sc_time(1, SC_NS))
        : sc_module(name)
        , spu2fft_target_socket("spu2fft_target_socket")
        , fft2vcore_init_socket("fft2vcore_init_socket")
        , internal_clk("internal_clk", clock_period)
        , clock_period(clock_period)
        , system_initialized(false)
        , config_loaded(false)
        , twiddles_loaded(false)
        , pipeline_busy(false)
    {
        // Register TLM interface callback
        spu2fft_target_socket.register_b_transport(this, &FFT_TLM::b_transport);
        
        // Create internal PEA_FFT instance
        pea_fft_core = new PEA_FFT<T, N,  FIFO_DEPTH>("pea_fft_core");
        
        // Initialize control signals with default values
        
        // Connect internal signals to PEA_FFT module
        connect_internal_signals();
        
        // Register SC_THREAD processes
        register_thread_processes();
        
        // Register monitoring processes
        register_monitor_processes();
    }
    
    // ========== Destructor ==========
    ~FFT_TLM() {
        delete pea_fft_core;
    }
    
    // ========== TLM Interface Implementation ==========
    void b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay);
    
    // ========== Core SC_THREAD Processes ==========
    void reset_fft_system();
    void configure_fft_mode();
    void load_twiddle_factors();
    void write_input_buffer();
    void process_fft_computation();
    void read_output_buffer();
    void monitor_pipeline_status();

    
    // ========== Internal Helper Methods ==========
    void connect_internal_signals();
    void register_thread_processes();
    void register_monitor_processes();
    void clear_all_control_signals();
    void wait_cycles(int n);
    bool check_input_buffer_ready();
    bool check_output_buffer_ready();
    
    // ========== TLM Command Implementation Methods ==========
    void reset_fft_system_impl(sc_time& delay);
    void configure_fft_mode_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len);
    void load_twiddle_factors_impl(sc_time& delay);
    void write_input_data_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len);
    void start_fft_processing_impl(sc_time& delay);
    void read_output_data_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len);
    void check_pipeline_status_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len);
    void set_fft_parameters_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len);
    
    // ========== Monitor Methods ==========
    void monitor_input_ready();
    void monitor_output_ready();
    
    // ========== Event Notification Methods ==========
    void send_event_notification(uint64_t event_addr);
    void send_input_ready_notification();
    void send_result_ready_notification();
    void send_output_ready_notification();
    
    // ========== Twiddle Factor Management ==========
    void load_single_twiddle(unsigned stage, unsigned pe, const complex<T>& twiddle);
    void load_standard_twiddles();
    
    // ========== Status and Configuration ==========
    void set_fft_configuration(const FFTConfiguration& config);
    FFTConfiguration get_fft_configuration() const { return current_config; }
    bool is_pipeline_ready() const { return !pipeline_busy; }
};

// Include template implementations
#include "../src/FFT_TLM.cpp"

#endif // FFT_TLM_H