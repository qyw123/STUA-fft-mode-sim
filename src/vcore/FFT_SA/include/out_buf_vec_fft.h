#ifndef OUT_BUF_VEC_FFT_H
#define OUT_BUF_VEC_FFT_H

#include "systemc.h"
#include "FIFO.h"
#include "../utils/complex_types.h"
#include <vector>

/**
 * @brief FFT output buffer vector module (supports real/imaginary separation storage)
 * @tparam T data type, usually ComplexT<scalar_t>
 * @tparam NUM_PE number of PEs, default 4 (supports 8-point FFT output)
 * @tparam FIFO_DEPTH depth of each FIFO, default 8
 */
template<typename T, int NUM_PE = 4, int FIFO_DEPTH = 8>
SC_MODULE(OUT_BUF_VEC_FFT) {
    // ========== Template Constants ==========
    static constexpr int NUM_FIFOS = NUM_PE * 4;  // Total 16 FIFOs (for 8 complex outputs)
    static constexpr int GROUP_SIZE = NUM_PE * 2; // 8 FIFOs per group (4 real + 4 imag)
    static constexpr int NUM_GROUPS = 2;          // 2 groups (Group0: Y[0-3], Group1: Y[4-7])

    // ========== Control Ports ==========
    sc_in_clk clk_i;
    sc_in<bool> rst_i;

    // ========== Data Write Ports (from FFT core) ==========
    sc_vector<sc_in<complex<T>>> data_i_y0;           // FFT out_y0[4] input (ComplexT)
    sc_vector<sc_in<complex<T>>> data_i_y1;           // FFT out_y1[4] input (ComplexT)
    sc_vector<sc_in<bool>> data_i_y0_v;      // Y0 valid signals
    sc_vector<sc_in<bool>> data_i_y1_v;      // Y1 valid signals
    sc_in<bool> wr_start_i;                  // Write start signal

    // ========== Data Read Ports (16-way parallel output) ==========
    sc_vector<sc_out<T>> data_o_vec;         // 16-way parallel data output (T)
    sc_vector<sc_out<bool>> rd_valid_o_vec;  // 16-way valid signal output
    sc_in<bool> rd_start_i;                  // Global read start signal

    // ========== Control Status Ports ==========
    sc_out<bool> buffer_ready_o;             // Buffer has data ready to read
    sc_out<bool> buffer_empty_o;             // Buffer is empty
    sc_vector<sc_out<bool>> wr_ready_o_vec;  // Write ready status for each FIFO

    // ========== Internal Components ==========
    sc_vector<FIFO<T, FIFO_DEPTH>> fifo_array; // 16 FIFO array for real/imag storage
    sc_vector<sc_signal<bool>> data_ready_vec;  // Each FIFO data ready status
    
    // ========== Internal Write Signals ==========
    sc_vector<sc_signal<T>> internal_data_i_vec;    // 16-way internal data input (T)
    sc_vector<sc_signal<bool>> internal_wr_en_vec;   // 16-way internal write enable signals
    sc_vector<sc_signal<bool>> internal_rd_start_vec; // 16-way internal read start signals

    // ========== Control State Variables ==========
    bool is_writing;                         // Whether in writing state
    bool is_reading;                         // Whether in reading state
    bool wr_start_prev;                      // Previous state of wr_start_i for edge detection
    bool rd_start_prev;                      // Previous state of rd_start_i for edge detection
    int ready_fifo_count;                    // Count of ready FIFOs

    // ========== Constructor Declaration ==========
    SC_CTOR(OUT_BUF_VEC_FFT);

    // ========== Method Declarations ==========
    void complex_decompose_driver();         // Complex number decomposition process
    void write_control_driver();             // Write control process
    void read_control_driver();              // Read control process
    void buffer_status_monitor();            // Buffer status monitor process
    void reset_internal_state();             // Internal state reset method

private:
    // ========== Internal Helper Methods ==========
    void decompose_complex_to_fifos();       // Decompose ComplexT inputs to real/imag FIFOs
    void start_all_reads();                  // Start all FIFO read operations
    void stop_all_reads();                   // Stop all FIFO read operations
    int get_y0_fifo_index(int pe_idx, bool is_imag); // Get FIFO index for Y0 data
    int get_y1_fifo_index(int pe_idx, bool is_imag); // Get FIFO index for Y1 data
};

// Include template implementations
#include "../src/out_buf_vec_fft.cpp"

#endif // OUT_BUF_VEC_FFT_H