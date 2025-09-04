/**
 * @file FFT_TLM.cpp  
 * @brief FFT Processing Element Array TLM Wrapper Implementation
 * 
 * This file contains the implementation of the FFT_TLM module, providing
 * TLM-2.0 wrapper functionality for the PEA_FFT module.
 * 
 * @version 1.0 - Initial TLM wrapper implementation
 * @date 2025-08-31
 */

#ifndef FFT_TLM_CPP
#define FFT_TLM_CPP

#include "../include/FFT_TLM.h"
#include "../utils/fft_test_utils.h"
#include "../utils/config.h"
#include <iostream>
#include <iomanip>

// ========== TLM Interface Implementation ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N, FIFO_DEPTH>::b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
    access_mutex.lock();
    
    // Extract extension and data
    FFTExtension* ext = trans.get_extension<FFTExtension>();
    uint8_t* data_ptr = trans.get_data_ptr();
    unsigned int data_len = trans.get_data_length();
    tlm::tlm_command cmd = trans.get_command();
    uint64_t addr = trans.get_address();
    
    cout << sc_time_stamp() << " [FFT_TLM] Received TLM transaction: ";
    
    // Process commands based on extension
    if (ext != nullptr) {
        switch (ext->cmd) {
            case FFTCommand::RESET_FFT_ARRAY:
                cout << "RESET_FFT_ARRAY" << endl;
                reset_fft_system_impl(delay);
                break;
                
            case FFTCommand::CONFIGURE_FFT_MODE:
                cout << "CONFIGURE_FFT_MODE" << endl;
                configure_fft_mode_impl(delay, data_ptr, data_len);
                break;
                
            case FFTCommand::LOAD_TWIDDLE_FACTORS:
                cout << "LOAD_TWIDDLE_FACTORS" << endl;
                twiddle_load_complete_event.notify();
                break;
                
            case FFTCommand::WRITE_INPUT_DATA:
                cout << "WRITE_INPUT_DATA" << endl;
                write_input_data_impl(delay, data_ptr, data_len);
                break;
                
            case FFTCommand::START_FFT_PROCESSING:
                cout << "START_FFT_PROCESSING" << endl;
                start_fft_processing_impl(delay);
                break;
                
            case FFTCommand::READ_OUTPUT_DATA:
                cout << "READ_OUTPUT_DATA" << endl;
                read_output_data_impl(delay, data_ptr, data_len);
                break;
                
            case FFTCommand::CHECK_PIPELINE_STATUS:
                cout << "CHECK_PIPELINE_STATUS" << endl;
                check_pipeline_status_impl(delay, data_ptr, data_len);
                break;
                
            case FFTCommand::SET_FFT_PARAMETERS:
                cout << "SET_FFT_PARAMETERS" << endl;
                set_fft_parameters_impl(delay, data_ptr, data_len);
                break;
                
            default:
                cout << "UNKNOWN_COMMAND" << endl;
                trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                access_mutex.unlock();
                return;
        }
    } else {
        cout << "ERROR: No FFT extension found" << endl;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        access_mutex.unlock();
        return;
    }
    
    // Set successful response
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    access_mutex.unlock();
}

// ========== Internal Signal Connection ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::connect_internal_signals() {
    cout << sc_time_stamp() << " [FFT_TLM] Connecting internal signals to PEA_FFT..." << endl;
    
    // Connect control signals
    pea_fft_core->clk_i(internal_clk);
    pea_fft_core->rst_i(internal_rst);
    
    // Connect input buffer interface
    pea_fft_core->wr_start_i(wr_start_i);
    
    for (int i = 0; i < NUM_FIFOS; ++i) {
        pea_fft_core->data_i_vec[i](data_i_vec[i]);
        pea_fft_core->wr_en_i[i](wr_en_i[i]);
        pea_fft_core->wr_ready_o_vec[i](wr_ready_o_vec[i]);
    }
    
    // Connect FFT processing control
    pea_fft_core->fft_mode_i(fft_mode_i);
    pea_fft_core->fft_shift_i(fft_shift_i);
    pea_fft_core->fft_conj_en_i(fft_conj_en_i);
    //pea_fft_core->fft_size_real(fft_size_real);
    
    // // Debug: Print array sizes
    // cout << sc_time_stamp() << " [FFT_TLM] DEBUG: N=" << N << ", log2_const(N)=" << log2_const(N) << endl;
    // cout << sc_time_stamp() << " [FFT_TLM] DEBUG: stage_bypass_en.size()=" << stage_bypass_en.size() << endl;
    // cout << sc_time_stamp() << " [FFT_TLM] DEBUG: pea_fft_core->stage_bypass_en.size()=" << pea_fft_core->stage_bypass_en.size() << endl;
    
    for(int i = 0; i < static_cast<size_t>(log2_const(N)); i++){
        //cout << sc_time_stamp() << " [FFT_TLM] DEBUG: Binding stage_bypass_en[" << i << "]" << endl;
        pea_fft_core->stage_bypass_en[i](stage_bypass_en[i]);
    }
    
    pea_fft_core->fft_start_i(fft_start_i);
    pea_fft_core->input_ready_o(input_ready_o);
    pea_fft_core->input_empty_o(input_empty_o);
    
    // Connect output buffer interface
    pea_fft_core->rd_start_i(rd_start_i);
    pea_fft_core->output_ready_o(output_ready_o);
    pea_fft_core->output_empty_o(output_empty_o);
    for (int i = 0; i < NUM_FIFOS; ++i) {
        pea_fft_core->data_o_vec[i](data_o_vec[i]);
        pea_fft_core->rd_valid_o_vec[i](rd_valid_o_vec[i]);
        pea_fft_core->wr_ready_out_vec[i](wr_ready_out_vec[i]);
    }
    
    // Connect twiddle factor interface
    pea_fft_core->tw_load_en(tw_load_en);
    pea_fft_core->tw_stage_idx(tw_stage_idx);
    pea_fft_core->tw_pe_idx(tw_pe_idx);
    pea_fft_core->tw_data(tw_data);
    
    cout << sc_time_stamp() << " [FFT_TLM] Internal signal connections completed" << endl;
}

// ========== SC_THREAD Process Registration ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::register_thread_processes() {
    cout << sc_time_stamp() << " [FFT_TLM] Registering SC_THREAD processes..." << endl;
    
    // Register core processing threads
    SC_THREAD(reset_fft_system);
    SC_THREAD(configure_fft_mode);
    SC_THREAD(load_twiddle_factors);
    SC_THREAD(write_input_buffer);
    SC_THREAD(process_fft_computation);
    SC_THREAD(read_output_buffer);
    SC_THREAD(monitor_pipeline_status);
    
    
    cout << sc_time_stamp() << " [FFT_TLM] SC_THREAD processes registered" << endl;
}

// ========== Monitor Process Registration ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::register_monitor_processes() {
    cout << sc_time_stamp() << " [FFT_TLM] Registering monitor processes..." << endl;
    
    // Monitor input buffer status
    SC_METHOD(monitor_input_ready);
    sensitive << input_ready_o;
    dont_initialize();
    
    // Monitor output buffer status
    SC_METHOD(monitor_output_ready);
    sensitive << output_ready_o;
    dont_initialize();
    
    cout << sc_time_stamp() << " [FFT_TLM] Monitor processes registered" << endl;
}

// ========== Reset FFT System SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::reset_fft_system() {
    // Initial startup delay to avoid initialization conflicts
    FFTTestUtils::wait_cycles(FFT_INIT_STARTUP_CYCLES, clock_period);
    
    while (true) {
        try {
            wait(reset_complete_event);
            
            cout << sc_time_stamp() << " [FFT_TLM] Executing system reset..." << endl;
            
            // Clear all control signals
            clear_all_control_signals();
            
            // Execute reset sequence
            internal_rst.write(false);
            FFTTestUtils::wait_cycles(FFT_RESET_ASSERT_CYCLES, clock_period);
            
            internal_rst.write(true);
            FFTTestUtils::wait_cycles(FFT_RESET_DEASSERT_CYCLES, clock_period);
            
            // Reset status flags
            system_initialized = false;
            config_loaded = false;
            twiddles_loaded = false;
            pipeline_busy = false;
            
            // Clear data
            current_data = FFTData(NUM_FIFOS);
            
            cout << sc_time_stamp() << " [FFT_TLM] System reset completed" << endl;
        } catch (...) {
            cout << "ERROR: Exception in reset_fft_system thread" << endl;
            break;
        }
    }
}

// ========== Configure FFT Mode SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::configure_fft_mode() {
    
    while (true) {
            wait(config_complete_event);
        
            cout << sc_time_stamp() << " [FFT_TLM] Configuring FFT mode..." << endl;
            
            // Apply current configuration to signals
            fft_mode_i.write(current_config.fft_mode);
            fft_shift_i.write(current_config.fft_shift);
            fft_conj_en_i.write(current_config.fft_conj_en);
            fft_size_real.write(current_config.fft_size_real);
            for(int i=0; i<static_cast<size_t>(log2_const(N)); i++){
                stage_bypass_en[i].write(current_config.stage_bypass_en[i]);
            }
            wait(1,SC_NS);
            cout << "wait 成功" << endl;

            FFTTestUtils::wait_cycles(FFT_CONFIG_SETUP_CYCLES, clock_period);
            
            config_loaded = true;
            
            cout << sc_time_stamp() << " [FFT_TLM] FFT mode configuration completed" << endl;
            cout << "  Mode: " << (current_config.fft_mode ? "FFT" : "GEMM") << endl;
            cout << "  Shift: " << current_config.fft_shift << endl;
            cout << "  Conjugate: " << (current_config.fft_conj_en ? "Enabled" : "Disabled") << endl;
            cout << "  fft_size: " << dec << current_config.fft_size << endl;
            cout << "  fft_size_real: " << dec << current_config.fft_size_real << endl;
    }
}

// ========== Load Twiddle Factors SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::load_twiddle_factors() {
    FFTTestUtils::wait_cycles(FFT_INIT_STARTUP_CYCLES, clock_period);  // Initial delay
    
    while (true) {
            wait(twiddle_load_complete_event);
            
            cout << sc_time_stamp() << " [FFT_TLM] Loading standard" << current_config.fft_size_real <<"-point FFT twiddle factors..." << endl;
            
            load_standard_twiddles();
            
            // Stabilization wait
            FFTTestUtils::wait_cycles(FFT_TWIDDLE_STABILIZE_CYCLES, clock_period);
            
            twiddles_loaded = true;
            
            cout << sc_time_stamp() << " [FFT_TLM] Twiddle factors loaded and stabilized" << endl;

    }
}

// ========== Write Input Buffer SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::write_input_buffer() {
    while (true) {
        wait(input_write_complete_event);
        
        cout << sc_time_stamp() << " [FFT_TLM] Writing input data..." << endl;
        cout << "NUM_FIFOS = " << NUM_FIFOS<< endl;
        
        // Get actual FFT size from current configuration  
        unsigned actual_fft_size = current_config.fft_size_real;
        cout << "Actual FFT size: " << actual_fft_size << "-point" << endl;
        
        // Clear all write enables first
        for (int i = 0; i < NUM_FIFOS; ++i) {
            data_i_vec[i].write(T(0));
            wr_en_i[i].write(false);
        }

        
        // Enable symmetric distribution across both groups
        for (unsigned j = 0; j < actual_fft_size/2; j++) {
            // Group0: FIFO[j] real
            data_i_vec[j].write(current_data.input_data[j]);
            wr_en_i[j].write(true);
            // Group0: FIFO[j] imag
            data_i_vec[j+N/2].write(current_data.input_data[j+actual_fft_size/2]);
            wr_en_i[j+N/2].write(true);
            // Group1: FIFO[N + j] real
            data_i_vec[N + j].write(current_data.input_data[j+actual_fft_size]);
            wr_en_i[N + j].write(true);
            // Group1: FIFO[N + j] imag
            data_i_vec[N + j + N/2].write(current_data.input_data[j+actual_fft_size*3/2]);
            wr_en_i[N + j + N/2].write(true);
        }
        
        // Start write process
        wr_start_i.write(true);
        
        FFTTestUtils::wait_cycles(FFT_INPUT_WRITE_SETUP_CYCLES, clock_period);
        
        // Debug output after 1-cycle delay to match write enable timing
        cout << "Write enable pattern: ";
        for (int i = 0; i < NUM_FIFOS; ++i) {
            cout << (wr_en_i[i].read() ? "1" : "0");
            if (i == N - 1) cout << "|";  // Group separator
        }
        cout << " (Group0|Group1 symmetric for " << actual_fft_size << "-point FFT)" << endl;
        
        wr_start_i.write(false);
        for (int i = 0; i < NUM_FIFOS; ++i) {
            wr_en_i[i].write(false);
        }
        
        FFTTestUtils::wait_cycles(FFT_INPUT_WRITE_HOLD_CYCLES, clock_period);
        
        // Wait for input buffer ready
        int timeout = 50;
        while (!input_ready_o.read() && timeout > 0) {
            FFTTestUtils::wait_cycles(FFT_START_PULSE_CYCLES, clock_period);
            timeout--;
        }
        
        if (timeout > 0) {
            cout << sc_time_stamp() << " [FFT_TLM] Input data written successfully" << endl;
        } else {
            cout << sc_time_stamp() << " [FFT_TLM] Input buffer write timeout" << endl;
        }
    }
}

// ========== Process FFT Computation SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::process_fft_computation() {
    while (true) {
        wait(fft_processing_complete_event);
        
        cout << sc_time_stamp() << " [FFT_TLM] Starting FFT processing..." << endl;
        
        // Wait for input buffer ready
        int timeout = 100;
        while (!input_ready_o.read() && timeout > 0) {
            FFTTestUtils::wait_cycles(FFT_START_PULSE_CYCLES, clock_period);
            timeout--;
        }
        
        if (timeout <= 0) {
            cout << "ERROR: Input buffer not ready for FFT processing" << endl;
            continue;
        }
        
        // Start FFT processing
        fft_start_i.write(true);
        cout << sc_time_stamp() << ": fft_start_i = " << fft_start_i << endl;
        FFTTestUtils::wait_cycles(FFT_START_PULSE_CYCLES, clock_period);
        cout << sc_time_stamp() << ": fft_start_i = " << fft_start_i << endl;
        // Keep fft_start_i active for a few cycles
        FFTTestUtils::wait_cycles(FFT_START_ACTIVE_CYCLES, clock_period);
        cout << sc_time_stamp() << ": fft_start_i = " << fft_start_i << endl;
        fft_start_i.write(false);
        
        // Calculate and wait for pipeline processing completion
        const int TOTAL_CYCLES = FFT_INPUT_BUFFER_CYCLES + FFT_PIPELINE_PROCESSING_CYCLES + 
                               FFT_OUTPUT_BUFFER_CYCLES + FFT_PIPELINE_MARGIN_CYCLES;
        
        cout << "  Pipeline latency estimation: " << TOTAL_CYCLES << " cycles" << endl;
        FFTTestUtils::wait_cycles(TOTAL_CYCLES, clock_period);
        
        cout << sc_time_stamp() << " [FFT_TLM] FFT processing completed" << endl;
    }
}

// ========== Read Output Buffer SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::read_output_buffer() {
    while (true) {
        wait(output_read_complete_event);
        
        cout << sc_time_stamp() << " [FFT_TLM] Reading output data..." << endl;
        
        // Start output read
        rd_start_i.write(true);
        
        // Wait for output buffer to process the rd_start signal and prepare data
        FFTTestUtils::wait_cycles(FFT_OUTPUT_READ_SETUP_CYCLES, clock_period); // Give output buffer time to respond to rd_start
        unsigned actual_fft_size = current_config.fft_size_real;
        // Sample output data after the output buffer has had time to respond
        const int group_stride = 2 * (N / actual_fft_size); // e.g., N=16: s=2,4,8,16 as size shrinks


        for (int i = 0; i < actual_fft_size; ++i) {
            // Map logical index i -> physical lane index src inside the real-half
            int src = (i / 2) * group_stride + (i % 2);
            // Real half
            current_data.output_data[i] = data_o_vec[src].read();
            //cout << "output_data[" << i << "] = " << data_o_vec[src].read() << endl;
            current_data.output_valid[i] = rd_valid_o_vec[src].read();
            // Imag half mirrors real half with +N offset
            current_data.output_data[i + actual_fft_size] = data_o_vec[src+N].read();
            //cout << "output_data[" << i + N << "] = " << data_o_vec[src+N].read() << endl;
            current_data.output_valid[i + actual_fft_size] = rd_valid_o_vec[src+N].read();
        }
        /*
        我的希望赋值结果是output_data[0:actual_fft_size]是real,output_data[16:16+actual_fft_size]是real
        N=16时，actual_fft_size=16，正确顺序，data_o_vec[0:15]是real,data_o_vec[16:31]是imag;
        N=16时，actual_fft_size=8，赋值错误，
            data_o_vec[0:1],data_o_vec[4:5]，data_o_vec[8:9]，data_o_vec[12:13]是real,
            data_o_vec[16:17],data_o_vec[20:21]，data_o_vec[24:25]，data_o_vec[28:29]是imag;
        N=16时，actual_fft_size=4，赋值错误，
            data_o_vec[0:1]，data_o_vec[8:9]是real,
            data_o_vec[16:17]，data_o_vec[24:25]是imag;
        N=16时，actual_fft_size=2，赋值错误，
            data_o_vec[0:1]是real,
            data_o_vec[16:17]是imag;
        */
        
        current_data.processing_complete = true;
        
        cout << sc_time_stamp() << " [FFT_TLM] Output data captured" << endl;
        
        // Keep rd_start_i active for a bit longer to ensure complete read
        FFTTestUtils::wait_cycles(FFT_OUTPUT_READ_HOLD_CYCLES, clock_period);
        rd_start_i.write(false);
    }
}

// ========== Monitor Pipeline Status SC_THREAD ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::monitor_pipeline_status() {
    FFTTestUtils::wait_cycles(FFT_INIT_STARTUP_CYCLES, clock_period);  // Initial delay
    
    while (true) {
        try {
            FFTTestUtils::wait_cycles(FFT_PIPELINE_MONITOR_CYCLES, clock_period);  // Check every 10 cycles
            
            // Monitor various status signals and update flags
            bool input_ready = input_ready_o.read();
            bool output_ready = output_ready_o.read();
            bool input_empty = input_empty_o.read();
            bool output_empty = output_empty_o.read();
            
            // Update pipeline busy status based on buffer states
            pipeline_busy = !input_empty || !output_empty;
        } catch (...) {
            cout << "ERROR: Exception in monitor_pipeline_status thread" << endl;
            break;
        }
    }
}



// ========== TLM Command Implementation Methods ==========

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::reset_fft_system_impl(sc_time& delay) {
    cout << sc_time_stamp() << " [FFT_TLM] Executing system reset..." << endl;
    
    // Reset all control signals directly without triggering SC_THREAD events
    internal_rst.write(true);  // Assert reset
    
    // Reset status flags
    system_initialized = false;
    config_loaded = false;
    twiddles_loaded = false;
    pipeline_busy = false;
    
    // Clear data
    current_data = FFTData(NUM_FIFOS);
    
    cout << sc_time_stamp() << " [FFT_TLM] System reset completed" << endl;
    
    delay += clock_period * FFT_TLM_RESET_CYCLES;
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::configure_fft_mode_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len) {
    if (data_len >= sizeof(FFTConfiguration)) {
        FFTConfiguration* config = reinterpret_cast<FFTConfiguration*>(data_ptr);
        current_config = *config;
        config_complete_event.notify();
        delay += clock_period * FFT_TLM_CONFIG_CYCLES;
    }
}


template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::write_input_data_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len) {
        T* input_data = reinterpret_cast<T*>(data_ptr);
        
        // Copy input data
        for (int i = 0; i < NUM_FIFOS; ++i) {
            current_data.input_data[i] = input_data[i];
            current_data.input_valid[i] = true;
        }
        
        input_write_complete_event.notify();
        delay += clock_period * FFT_TLM_INPUT_CYCLES;
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::start_fft_processing_impl(sc_time& delay) {
    fft_processing_complete_event.notify();
    delay += clock_period * FFT_TLM_PROCESSING_CYCLES;
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::read_output_data_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len) {
    // Trigger the SC_THREAD to actually read from output buffer
    output_read_complete_event.notify();
    
    // Wait for the SC_THREAD to complete the actual reading
    FFTTestUtils::wait_cycles(FFT_TLM_OUTPUT_CYCLES, clock_period);
    
    T* output_data = reinterpret_cast<T*>(data_ptr);
    
    for (int i = 0; i < NUM_FIFOS; ++i) {
        output_data[i] = current_data.output_data[i];
    }
    
    delay += clock_period * FFT_TLM_OUTPUT_CYCLES;
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::check_pipeline_status_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len) {
    if (data_len >= sizeof(bool)) {
        bool* status = reinterpret_cast<bool*>(data_ptr);
        *status = !pipeline_busy;
        delay += clock_period * FFT_TLM_STATUS_CYCLES;
    }
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::set_fft_parameters_impl(sc_time& delay, uint8_t* data_ptr, unsigned int data_len) {
    // Implementation for setting FFT parameters
    delay += clock_period * FFT_TLM_PARAM_CYCLES;
}

// ========== Utility Method Implementations ==========

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::clear_all_control_signals() {
    wr_start_i.write(false);
    
    fft_start_i.write(false);
    rd_start_i.write(false);
    tw_load_en.write(false);
    
    fft_mode_i.write(true);
    fft_shift_i.write(0);
    fft_conj_en_i.write(false);
    fft_size_real.write(0);
    for(int i = 0; i < static_cast<size_t>(log2_const(N)); i++){
        stage_bypass_en[i].write(false);
    }
    tw_stage_idx.write(0);
    tw_pe_idx.write(0);
    tw_data.write(complex<T>(0, 0));
    
    for (int i = 0; i < NUM_FIFOS; ++i) {
        data_i_vec[i].write(0.0f);
        wr_en_i[i].write(false);
    }
}


template<typename T, unsigned N,  int FIFO_DEPTH>
bool FFT_TLM<T, N,  FIFO_DEPTH>::check_input_buffer_ready() {
    return input_ready_o.read();
}

template<typename T, unsigned N,  int FIFO_DEPTH>
bool FFT_TLM<T, N,  FIFO_DEPTH>::check_output_buffer_ready() {
    return output_ready_o.read();
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::load_single_twiddle(unsigned stage, unsigned pe, const complex<T>& twiddle) {
    tw_stage_idx.write(stage);
    tw_pe_idx.write(pe);
    tw_data.write(twiddle);
    tw_load_en.write(true);
    
    FFTTestUtils::wait_cycles(FFT_TWIDDLE_LOAD_CYCLES, clock_period);
    tw_load_en.write(false);
    FFTTestUtils::wait_cycles(FFT_TWIDDLE_LOAD_CYCLES, clock_period);
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::load_standard_twiddles() {
    // This uses utility functions from fft_test_utils.h
    auto twiddles = FFTTestUtils::generate_fft_twiddles(N);
    int fft_size = current_config.fft_size_real;
    
    for (unsigned stage = log2_const(N)-log2(fft_size); stage < log2_const(N); stage++) {
        for (unsigned pe = 0; pe < NUM_PE; pe++) {
            load_single_twiddle(stage, pe, twiddles[stage][pe]);
        }
    }
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::monitor_input_ready() {
    if (input_ready_o.read()) {
        cout << sc_time_stamp() << " [FFT_TLM] Input buffer ready detected" << endl;
    }
}

template<typename T, unsigned N,  int FIFO_DEPTH>
void FFT_TLM<T, N,  FIFO_DEPTH>::monitor_output_ready() {
    if (output_ready_o.read()) {
        cout << sc_time_stamp() << " [FFT_TLM] Output buffer ready detected" << endl;
    }
}

// ====== 模板显式实例化 ======
template class FFT_TLM<float, 8, 8>;
template class FFT_TLM<float, 16, 8>;
template class FFT_TLM<float, 32, 8>;
template class FFT_TLM<float, 64, 8>;

#endif // FFT_TLM_CPP