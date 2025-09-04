#ifndef IN_BUF_VEC_FFT_H
#define IN_BUF_VEC_FFT_H

#include "systemc.h"
#include "FIFO.h"
#include <vector>

/**
 * @brief FFT input buffer vector module (supports grouped parallel reading)
 * @tparam T data type, usually float
 * @tparam NUM_PE number of PEs, default 4 (supports 8-point FFT)
 * @tparam FIFO_DEPTH depth of each FIFO, default 8
 */
template<typename T, int NUM_PE = 4, int FIFO_DEPTH = 8>
SC_MODULE(IN_BUF_VEC_FFT) {
    // ========== Template Constants ==========
    static constexpr int NUM_FIFOS = NUM_PE * 4;  // Total 16 FIFOs (when NUM_PE=4)
    static constexpr int GROUP_SIZE_MAX = NUM_PE * 2; // 8 FIFOs per group (4 real + 4 imag)
    static constexpr int NUM_GROUPS = 2;          // 2 groups (Group0: X[0-3], Group1: X[4-7])

    // ========== Control Ports ==========
    sc_in_clk clk_i;
    sc_in<bool> rst_i;

    // ========== Data Write Ports ==========
    sc_vector<sc_in<T>> data_i_vec;      // 16-way parallel data input
    sc_in<bool> wr_start_i;              // Write start signal
    sc_vector<sc_in<bool>> wr_en_i;      // Write enable signal
    sc_vector<sc_out<bool>> wr_ready_o_vec; // Write ready status output

    // ========== Grouped Data Read Ports ==========
    // Group0: Complex X[0-3] -> PE array a ports
    sc_vector<sc_out<T>> data_o_group0;     // Group0 data output (8-way: 4 real + 4 imag)
    sc_vector<sc_out<bool>> rd_valid_group0; // Group0 valid signal output
    
    // Group1: Complex X[4-7] -> PE array b ports  
    sc_vector<sc_out<T>> data_o_group1;     // Group1 data output (8-way: 4 real + 4 imag)
    sc_vector<sc_out<bool>> rd_valid_group1; // Group1 valid signal output

    // ========== Read Control Ports ==========
    sc_in<bool> rd_start_i;              // Global read start signal
    sc_out<bool> groups_ready_o;         // Both groups have sufficient data to read
    sc_out<bool> groups_empty_o;         // Both groups are empty

    // ========== Internal Components ==========
    sc_vector<FIFO<T, FIFO_DEPTH>> fifo_array; // 16 FIFO array
    sc_vector<sc_signal<bool>> data_ready_vec;  // Each FIFO data ready status
    
    // ========== Internal Signals ==========
    sc_vector<sc_signal<bool>> rd_start_group0; // Group0 FIFO read start signals
    sc_vector<sc_signal<bool>> rd_start_group1; // Group1 FIFO read start signals

    // ========== Control State Variables ==========
    bool is_reading;                     // Whether in reading state
    bool rd_start_prev;                  // Previous state of rd_start_i for edge detection
    int group0_ready_count;              // Group0 ready FIFO count
    int group1_ready_count;              // Group1 ready FIFO count
    
    // ========== Write Enable Edge Detection ==========
    sc_vector<sc_signal<bool>> wr_en_prev; // Previous state of wr_en_i for edge detection
    bool groups_ready_flag;              // Flag to indicate write completion detected

    // ========== Constructor Declaration ==========
    SC_CTOR(IN_BUF_VEC_FFT);

    // ========== Method Declarations ==========
    void read_group_driver();            // Grouped read driver process
    void group_status_monitor();         // Group status monitor process
    void reset_internal_state();         // Internal state reset method

private:
    // ========== Internal Helper Methods ==========
    bool check_group_ready(int group_idx); // Check if specified group is ready
    void start_group_read(int group_idx);   // Start specified group read
    void stop_all_reads();                  // Stop all read operations
};

// Include template implementations
#include "../src/in_buf_vec_fft.cpp"

#endif // IN_BUF_VEC_FFT_H