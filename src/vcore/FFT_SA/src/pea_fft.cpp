/**
 * @file pea_fft.cpp
 * @brief Complete FFT Processing Element Array with Input Buffer Implementation
 */

#ifndef PEA_FFT_CPP
#define PEA_FFT_CPP

#include "../include/pea_fft.h"
#include <iostream>

// ========== Constructor Implementation ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
PEA_FFT<T, N,  FIFO_DEPTH>::PEA_FFT(sc_module_name name) :
    sc_module(name),
    data_i_vec("data_i_vec", NUM_FIFOS),
    wr_en_i("wr_en_i", NUM_FIFOS),
    wr_ready_o_vec("wr_ready_o_vec", NUM_FIFOS),
    data_o_vec("data_o_vec", NUM_FIFOS),
    rd_valid_o_vec("rd_valid_o_vec", NUM_FIFOS),
    wr_ready_out_vec("wr_ready_out_vec", NUM_FIFOS),
    buf_group0_real("buf_group0_real", NUM_PE),
    buf_group0_imag("buf_group0_imag", NUM_PE),
    buf_group1_real("buf_group1_real", NUM_PE),
    buf_group1_imag("buf_group1_imag", NUM_PE),
    buf_group0_real_v("buf_group0_real_v", NUM_PE),
    buf_group0_imag_v("buf_group0_imag_v", NUM_PE),
    buf_group1_real_v("buf_group1_real_v", NUM_PE),
    buf_group1_imag_v("buf_group1_imag_v", NUM_PE),
    buf_to_fft_a("buf_to_fft_a", NUM_PE),
    buf_to_fft_b("buf_to_fft_b", NUM_PE),
    buf_to_fft_a_v("buf_to_fft_a_v", NUM_PE),
    buf_to_fft_b_v("buf_to_fft_b_v", NUM_PE),
    dummy_group0_data("dummy_group0_data", NUM_PE),
    dummy_group1_data("dummy_group1_data", NUM_PE),
    dummy_group0_valid("dummy_group0_valid", NUM_PE),
    dummy_group1_valid("dummy_group1_valid", NUM_PE),
    fft_out_y0("fft_out_y0", NUM_PE),
    fft_out_y1("fft_out_y1", NUM_PE),
    fft_out_y0_v("fft_out_y0_v", NUM_PE),
    fft_out_y1_v("fft_out_y1_v", NUM_PE),
    stage_bypass_en("stage_bypass_en", NUM_STAGES)
{
    SC_HAS_PROCESS(PEA_FFT);
    
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Initializing PEA_FFT module (N=" << N 
    //           << ", NUM_PE=" << NUM_PE << ", FIFO_DEPTH=" << FIFO_DEPTH << ")" << std::endl;
    
    // ========== Instantiate Input Buffer Component ==========
    input_buffer = new IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>("input_buffer");
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Created input buffer module" << std::endl;
    
    // ========== Instantiate FFT Core Component ==========
    fft_core = new FftMultiStage<T, N>("fft_core");
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Created FFT core module" << std::endl;
    
    // ========== Instantiate Output Buffer Component ==========
    output_buffer = new OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>("output_buffer");
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Created output buffer module" << std::endl;
    
    // ========== Setup All Internal Connections ==========
    setup_connections();
    
    // ========== Register Complex Reconstruction Process ==========
    SC_METHOD(complex_reconstruction_process);
    for (int i = 0; i < NUM_PE; ++i) {
        sensitive << buf_group0_real[i] << buf_group0_imag[i];
        sensitive << buf_group1_real[i] << buf_group1_imag[i];
        sensitive << buf_group0_real_v[i] << buf_group0_imag_v[i];
        sensitive << buf_group1_real_v[i] << buf_group1_imag_v[i];
    }
    dont_initialize();
    
    
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] PEA_FFT initialization completed" << std::endl;
}

// ========== Destructor Implementation ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
PEA_FFT<T, N,  FIFO_DEPTH>::~PEA_FFT() {
    delete input_buffer;
    delete fft_core;
    delete output_buffer;
    // std::cout << "PEA_FFT destructor: All components cleaned up (input_buffer, fft_core, output_buffer)" << std::endl;
}

// ========== Setup All Internal Connections ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void PEA_FFT<T, N,  FIFO_DEPTH>::setup_connections() {
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Setting up internal connections..." << std::endl;
    
    // ========== Input Buffer Control Connections ==========
    input_buffer->clk_i(clk_i);
    input_buffer->rst_i(rst_i);
    
    // ========== Input Buffer Data Write Connections ==========
    for (int i = 0; i < NUM_FIFOS; ++i) {
        input_buffer->data_i_vec[i](data_i_vec[i]);
        input_buffer->wr_ready_o_vec[i](wr_ready_o_vec[i]);
        input_buffer->wr_en_i[i](wr_en_i[i]);
    }
    input_buffer->wr_start_i(wr_start_i);
    
    
    // ========== Input Buffer Read Control Connections ==========
    input_buffer->rd_start_i(fft_start_i);  // FFT start triggers buffer read
    input_buffer->groups_ready_o(input_ready_o);
    input_buffer->groups_empty_o(input_empty_o);
    
    // ========== FFT Core Control Connections ==========
    fft_core->clk_i(clk_i);
    fft_core->rst_i(rst_i);
    fft_core->fft_mode_i(fft_mode_i);
    fft_core->fft_shift_i(fft_shift_i);
    fft_core->fft_conj_en_i(fft_conj_en_i);
    for(int i = 0; i < NUM_STAGES; i++){
        fft_core->stage_bypass_en[i](stage_bypass_en[i]);
    }
    
    // ========== FFT Core Twiddle Connections ==========
    fft_core->tw_load_en(tw_load_en);
    fft_core->tw_stage_idx(tw_stage_idx);
    fft_core->tw_pe_idx(tw_pe_idx);
    fft_core->tw_data(tw_data);
    
    // ========== FFT Core Output Connections ==========
    for (int i = 0; i < NUM_PE; ++i) {
        fft_core->out_y0[i](fft_out_y0[i]);
        fft_core->out_y1[i](fft_out_y1[i]);
        fft_core->out_y0_v[i](fft_out_y0_v[i]);
        fft_core->out_y1_v[i](fft_out_y1_v[i]);
    }
    
    // ========== Buffer to FFT Data Mapping ==========
    map_buffer_to_fft_data();
    
    // ========== Output Buffer Connections ==========
    setup_output_buffer_connections();
    
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Internal connections setup completed" << std::endl;
}

// ========== Map Buffer Groups to FFT Input Ports ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void PEA_FFT<T, N,  FIFO_DEPTH>::map_buffer_to_fft_data() {
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Mapping buffer groups to FFT input ports..." << std::endl;
    
    // ========== Group0 (Buffer output) -> Separated Real/Imag Signals ==========
    // Group0: data_o_group0[0-3] = real parts, data_o_group0[4-7] = imag parts
    for (int i = 0; i < NUM_PE; ++i) {
        // Real parts: Group0[0-3] -> buf_group0_real[0-3]
        input_buffer->data_o_group0[i](buf_group0_real[i]);
        input_buffer->rd_valid_group0[i](buf_group0_real_v[i]);
        
        // Imaginary parts: Group0[4-7] -> buf_group0_imag[0-3] 
        input_buffer->data_o_group0[i + NUM_PE](buf_group0_imag[i]);
        input_buffer->rd_valid_group0[i + NUM_PE](buf_group0_imag_v[i]);
    }
    
    // ========== Group1 (Buffer output) -> Separated Real/Imag Signals ==========
    // Group1: data_o_group1[0-3] = real parts, data_o_group1[4-7] = imag parts
    for (int i = 0; i < NUM_PE; ++i) {
        // Real parts: Group1[0-3] -> buf_group1_real[0-3]
        input_buffer->data_o_group1[i](buf_group1_real[i]);
        input_buffer->rd_valid_group1[i](buf_group1_real_v[i]);
        
        // Imaginary parts: Group1[4-7] -> buf_group1_imag[0-3]
        input_buffer->data_o_group1[i + NUM_PE](buf_group1_imag[i]);
        input_buffer->rd_valid_group1[i + NUM_PE](buf_group1_imag_v[i]);
    }
    
    // ========== Connect FFT Core Inputs ==========
    // Complex reconstruction will be handled by complex_reconstruction_process()
    for (int i = 0; i < NUM_PE; ++i) {
        fft_core->in_a[i](buf_to_fft_a[i]);
        fft_core->in_a_v[i](buf_to_fft_a_v[i]);
        fft_core->in_b[i](buf_to_fft_b[i]);
        fft_core->in_b_v[i](buf_to_fft_b_v[i]);
    }
    
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Buffer to FFT data mapping completed:" << std::endl;
    // std::cout << "  - Group0 [0-3] -> FFT in_a[0-3], [4-7] -> dummy signals" << std::endl;
    // std::cout << "  - Group1 [0-3] -> FFT in_b[0-3], [4-7] -> dummy signals" << std::endl;
}

// ========== Setup Output Buffer Connections ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void PEA_FFT<T, N,  FIFO_DEPTH>::setup_output_buffer_connections() {
    // std::cout << sc_time_stamp() << ": [" << this->name() 
    //           << "] Setting up output buffer connections..." << std::endl;
    
    // ========== Output Buffer Control Connections ==========
    output_buffer->clk_i(clk_i);
    output_buffer->rst_i(rst_i);
    
    // ========== FFT Core to Output Buffer Data Connections ==========
    // Connect FFT complex outputs directly to output buffer
    for (int i = 0; i < NUM_PE; ++i) {
        output_buffer->data_i_y0[i](fft_out_y0[i]);
        output_buffer->data_i_y1[i](fft_out_y1[i]);
        output_buffer->data_i_y0_v[i](fft_out_y0_v[i]);
        output_buffer->data_i_y1_v[i](fft_out_y1_v[i]);
    }
    
    // ========== Output Buffer Control Signal Connections ==========
    // Write control - triggered by FFT processing completion (could use a specific signal)
    // For now, we'll use a simple approach - write when FFT outputs are valid
    output_buffer->wr_start_i(fft_start_i);  // Use same signal as FFT start for simplicity
    
    // Read control from external
    output_buffer->rd_start_i(rd_start_i);
    
    // ========== Output Buffer Status Connections ==========
    output_buffer->buffer_ready_o(output_ready_o);
    output_buffer->buffer_empty_o(output_empty_o);
    
    // ========== Output Buffer Data Output Connections ==========
    for (int i = 0; i < NUM_FIFOS; ++i) {
        output_buffer->data_o_vec[i](data_o_vec[i]);
        output_buffer->rd_valid_o_vec[i](rd_valid_o_vec[i]);
        output_buffer->wr_ready_o_vec[i](wr_ready_out_vec[i]);
    }
    
//     std::cout << sc_time_stamp() << ": [" << this->name() 
//               << "] Output buffer connections completed:" << std::endl;
//     std::cout << "  - FFT outputs → Output buffer inputs (Y0[0-3], Y1[0-3])" << std::endl;
//     std::cout << "  - Output buffer → 16-way parallel outputs (real/imag separation)" << std::endl;
}

// ========== Complex Number Reconstruction Process ==========
template<typename T, unsigned N,  int FIFO_DEPTH>
void PEA_FFT<T, N,  FIFO_DEPTH>::complex_reconstruction_process() {
    // ========== Reconstruct Group0 Complex Numbers (X[0-3] -> FFT in_a) ==========
    for (int i = 0; i < NUM_PE; ++i) {
        T real_part = buf_group0_real[i].read();
        T imag_part = buf_group0_imag[i].read();
        bool real_valid = buf_group0_real_v[i].read();
        bool imag_valid = buf_group0_imag_v[i].read();
        
        if (real_valid && imag_valid) {
            cout << sc_time_stamp() << ": [" << this->name() << "] Reconstructing Group0 complex data for FFT input a[" << i << "]" << endl;
        }
        // Reconstruct complex number
        complex<T> complex_value = complex<T>(real_part, imag_part);
        buf_to_fft_a[i].write(complex_value);
        buf_to_fft_a_v[i].write(real_valid && imag_valid);
    }
    
    // ========== Reconstruct Group1 Complex Numbers (X[4-7] -> FFT in_b) ==========
    for (int i = 0; i < NUM_PE; ++i) {
        T real_part = buf_group1_real[i].read();
        T imag_part = buf_group1_imag[i].read();
        bool real_valid = buf_group1_real_v[i].read();
        bool imag_valid = buf_group1_imag_v[i].read();
        
        if (real_valid && imag_valid) {
            cout << sc_time_stamp() << ": [" << this->name() << "] Reconstructing Group1 complex data for FFT input b[" << i << "]" << endl;
        }
        // Reconstruct complex number
        complex<T> complex_value = complex<T>(real_part, imag_part);
        buf_to_fft_b[i].write(complex_value);
        buf_to_fft_b_v[i].write(real_valid && imag_valid);
    }
}



#endif // PEA_FFT_CPP