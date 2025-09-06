/**
 * @file out_buf_vec_fft.cpp
 * @brief FFT output buffer vector module implementation with real/imaginary separation
 */

#ifndef OUT_BUF_VEC_FFT_CPP
#define OUT_BUF_VEC_FFT_CPP

#include "../include/out_buf_vec_fft.h"
#include <iostream>


// ========== Constructor Implementation ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::OUT_BUF_VEC_FFT(sc_module_name name) :
    sc_module(name),
    data_i_y0("data_i_y0", NUM_PE),
    data_i_y1("data_i_y1", NUM_PE),
    data_i_y0_v("data_i_y0_v", NUM_PE),
    data_i_y1_v("data_i_y1_v", NUM_PE),
    data_o_vec("data_o_vec", NUM_FIFOS),
    rd_valid_o_vec("rd_valid_o_vec", NUM_FIFOS),
    wr_ready_o_vec("wr_ready_o_vec", NUM_FIFOS),
    fifo_array("fifo_array", NUM_FIFOS),
    data_ready_vec("data_ready_vec", NUM_FIFOS),
    internal_data_i_vec("internal_data_i_vec", NUM_FIFOS),
    internal_wr_en_vec("internal_wr_en_vec", NUM_FIFOS),
    internal_rd_start_vec("internal_rd_start_vec", NUM_FIFOS)
{
    std::cout << sc_time_stamp() << ": [" << this->name() 
              << "] Initializing OUT_BUF_VEC_FFT module (NUM_PE=" << NUM_PE 
              << ", FIFO_DEPTH=" << FIFO_DEPTH << ")" << std::endl;

    // ========== FIFO Array Connections ==========
    for (int i = 0; i < NUM_FIFOS; ++i) {
        // Basic control signal connections
        fifo_array[i].clk_i(clk_i);
        fifo_array[i].rst_i(rst_i);
        
        // Write port connections
        fifo_array[i].data_i(internal_data_i_vec[i]);
        fifo_array[i].wr_start_i(wr_start_i);
        fifo_array[i].wr_en_i(internal_wr_en_vec[i]);
        fifo_array[i].wr_ready_o(wr_ready_o_vec[i]);
        
        // Read port connections
        fifo_array[i].data_o(data_o_vec[i]);
        fifo_array[i].rd_start_i(internal_rd_start_vec[i]);
        fifo_array[i].rd_valid_o(rd_valid_o_vec[i]);
        
        // Status connections
        fifo_array[i].data_ready_o(data_ready_vec[i]);
    }
    
    // ========== Process Registration ==========
    SC_THREAD(complex_decompose_driver);
    sensitive << clk_i.pos();
    dont_initialize();
    
    SC_THREAD(write_control_driver);
    sensitive << clk_i.pos();
    dont_initialize();
    
    SC_THREAD(read_control_driver);
    sensitive << clk_i.pos();
    dont_initialize();
    
    SC_METHOD(buffer_status_monitor);
    sensitive << clk_i.pos();
    for (int i = 0; i < NUM_FIFOS; ++i) {
        sensitive << data_ready_vec[i];
    }
    dont_initialize();
    
    // ========== Initial State ==========
    reset_internal_state();
    
    std::cout << sc_time_stamp() << ": [" << this->name() 
              << "] OUT_BUF_VEC_FFT initialization completed" << std::endl;
}

// ========== Internal State Reset Method ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::reset_internal_state() {
    is_writing = false;
    is_reading = false;
    wr_start_prev = false;
    rd_start_prev = false;
    ready_fifo_count = 0;
}

// ========== Complex Decomposition Driver Process ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::complex_decompose_driver() {
    if (rst_i.read() == 0) {
        reset_internal_state();
        
        // Clear all internal signals
        for (int i = 0; i < NUM_FIFOS; ++i) {
            internal_data_i_vec[i].write(0);
            internal_wr_en_vec[i].write(false);
        }
        
        std::cout << sc_time_stamp() << ": [" << this->name() 
                  << "] Reset: cleared all internal write signals" << std::endl;
    }
    
    while (true) {
        wait(); // Wait for clock rising edge
        
        // Clear all write enable signals first
        for (int i = 0; i < NUM_FIFOS; ++i) {
            internal_wr_en_vec[i].write(false);
        }
        
        // ========== Decompose Complex Inputs with Interleaved Y0/Y1 Mapping ==========
        // New mapping: Front half FIFOs (0-7) for real parts, back half (8-15) for imaginary parts
        // Order: Y0[0],Y1[0],Y0[1],Y1[1],Y0[2],Y1[2],Y0[3],Y1[3] for both real and imag
        
        for (int i = 0; i < NUM_PE; ++i) {
            // Process Y0[i]
            if (data_i_y0_v[i].read()) {
                complex<T> complex_val = data_i_y0[i].read();
                
                // Calculate FIFO indices: Y0[i] → index i*2 (real), i*2 + NUM_FIFOS/2 (imag)
                int real_fifo_idx = i * 2;  // 0, 2, 4, 6 for Y0[0-3]
                int imag_fifo_idx = real_fifo_idx + NUM_FIFOS / 2;  // 8, 10, 12, 14
                
                // Write real part to front half FIFO
                internal_data_i_vec[real_fifo_idx].write(complex_val.real);
                internal_wr_en_vec[real_fifo_idx].write(true);
                
                // Write imaginary part to back half FIFO
                internal_data_i_vec[imag_fifo_idx].write(complex_val.imag);
                internal_wr_en_vec[imag_fifo_idx].write(true);
                
                // std::cout << sc_time_stamp() << ": [" << this->name() 
                //           << "] Y0[" << i << "] decomposed: re=" << complex_val.real 
                //           << " → FIFO[" << real_fifo_idx << "], im=" << complex_val.imag
                //           << " → FIFO[" << imag_fifo_idx << "]" << std::endl;
            }
            
            // Process Y1[i]
            if (data_i_y1_v[i].read()) {
                complex<T> complex_val = data_i_y1[i].read();
                
                // Calculate FIFO indices: Y1[i] → index i*2+1 (real), i*2+1 + NUM_FIFOS/2 (imag)
                int real_fifo_idx = i * 2 + 1;  // 1, 3, 5, 7 for Y1[0-3]
                int imag_fifo_idx = real_fifo_idx + NUM_FIFOS / 2;  // 9, 11, 13, 15
                
                // Write real part to front half FIFO
                internal_data_i_vec[real_fifo_idx].write(complex_val.real);
                internal_wr_en_vec[real_fifo_idx].write(true);
                
                // Write imaginary part to back half FIFO
                internal_data_i_vec[imag_fifo_idx].write(complex_val.imag);
                internal_wr_en_vec[imag_fifo_idx].write(true);
                
                // std::cout << sc_time_stamp() << ": [" << this->name() 
                //           << "] Y1[" << i << "] decomposed: re=" << complex_val.real 
                //           << " → FIFO[" << real_fifo_idx << "], im=" << complex_val.imag 
                //           << " → FIFO[" << imag_fifo_idx << "]" << std::endl;
            }
        }
    }
}

// ========== Write Control Driver Process ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::write_control_driver() {
    while (true) {
        wait(); // Wait for clock rising edge
        
        bool wr_start_curr = wr_start_i.read();
        
        // ========== Detect wr_start rising edge ==========
        if (!wr_start_prev && wr_start_curr) {
            is_writing = true;
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] Detected wr_start rising edge, start writing" << std::endl;
        }
        
        // ========== Detect wr_start falling edge ==========
        if (wr_start_prev && !wr_start_curr && is_writing) {
            is_writing = false;
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] Detected wr_start falling edge, stop writing" << std::endl;
        }
        
        wr_start_prev = wr_start_curr;
    }
}

// ========== Read Control Driver Process ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::read_control_driver() {
    while (true) {
        wait(); // Wait for clock rising edge
        
        bool rd_start_curr = rd_start_i.read();
        
        // ========== Detect rd_start rising edge ==========
        if (!rd_start_prev && rd_start_curr) {
            is_reading = true;
            start_all_reads();
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] Detected rd_start rising edge, start all reads" << std::endl;
        }
        
        // ========== Detect rd_start falling edge ==========
        if (rd_start_prev && !rd_start_curr && is_reading) {
            is_reading = false;
            stop_all_reads();
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] Detected rd_start falling edge, stop all reads" << std::endl;
        }
        
        rd_start_prev = rd_start_curr;
    }
}

// ========== Buffer Status Monitor Process ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::buffer_status_monitor() {
    if (rst_i.read() == 0) {
        buffer_ready_o.write(false);
        buffer_empty_o.write(true);
        return;
    }
    
    // ========== Count ready FIFOs ==========
    ready_fifo_count = 0;
    for (int i = 0; i < NUM_FIFOS; ++i) {
        if (data_ready_vec[i].read()) {
            ready_fifo_count++;
        }
    }
    
    // ========== Output buffer status ==========
    bool output_ready = (ready_fifo_count == 2*fft_size_real.read());
    bool all_empty = (ready_fifo_count == 0);
    
    buffer_ready_o.write(output_ready);
    buffer_empty_o.write(all_empty);
}

// ========== Start All FIFO Read Operations ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::start_all_reads() {
    for (int i = 0; i < NUM_FIFOS; ++i) {
        internal_rd_start_vec[i].write(true);
    }
}

// ========== Stop All FIFO Read Operations ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::stop_all_reads() {
    for (int i = 0; i < NUM_FIFOS; ++i) {
        internal_rd_start_vec[i].write(false);
    }
}

// ========== Get FIFO Index for Y0 Data ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
int OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::get_y0_fifo_index(int pe_idx, bool is_imag) {
    // Y0[0-3] mapping: 
    // Y[0].re → FIFO[0], Y[0].im → FIFO[4]
    // Y[1].re → FIFO[1], Y[1].im → FIFO[5]
    // Y[2].re → FIFO[2], Y[2].im → FIFO[6]
    // Y[3].re → FIFO[3], Y[3].im → FIFO[7]
    return is_imag ? (pe_idx + NUM_PE) : pe_idx;
}

// ========== Get FIFO Index for Y1 Data ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
int OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::get_y1_fifo_index(int pe_idx, bool is_imag) {
    // Y1[0-3] mapping to Y[4-7]:
    // Y[4].re → FIFO[8],  Y[4].im → FIFO[12]
    // Y[5].re → FIFO[9],  Y[5].im → FIFO[13]
    // Y[6].re → FIFO[10], Y[6].im → FIFO[14]
    // Y[7].re → FIFO[11], Y[7].im → FIFO[15]
    int base_idx = GROUP_SIZE; // Start from FIFO[8]
    return is_imag ? (base_idx + pe_idx + NUM_PE) : (base_idx + pe_idx);
}

#endif // OUT_BUF_VEC_FFT_CPP