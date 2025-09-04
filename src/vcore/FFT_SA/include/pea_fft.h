#ifndef PEA_FFT_H
#define PEA_FFT_H

#include "systemc.h"
#include "in_buf_vec_fft.h"
#include "out_buf_vec_fft.h"
#include "fft_multi_stage.h"
#include "../utils/complex_types.h"

/**
 * @brief Complete FFT Processing Element Array with Input Buffer
 * @tparam T Complex data type, typically ComplexT<scalar_t>
 * @tparam N FFT size, must be power of 2 (default 8-point FFT)
 * @tparam NUM_PE Number of Processing Elements (default 4 for 8-point)
 * @tparam FIFO_DEPTH Depth of each input FIFO buffer (default 8)
 */
template<typename T, unsigned N = 8,  int FIFO_DEPTH = 8>
SC_MODULE(PEA_FFT) {
    // Static assertions for template parameters
    static_assert(N >= 2 && ((N & (N-1)) == 0), "FFT size N must be power of two");
    
    // ========== Template Constants ==========
    static constexpr int NUM_PE = N/2;
    static constexpr int NUM_FIFOS = NUM_PE * 4;  // 16 FIFOs for 8-point FFT
    static constexpr int GROUP_SIZE = NUM_PE * 2; // 8 FIFOs per group
    static constexpr int NUM_STAGES = log2_const(N); // Number of FFT stages
    
    // ========== Control Ports ==========
    sc_in_clk clk_i;
    sc_in<bool> rst_i;
    
    // ========== Data Input Ports (Raw FFT Data) ==========
    sc_vector<sc_in<T>> data_i_vec;      // 16-way parallel FFT data input (T for real/imag separation)
    sc_in<bool> wr_start_i;                  // Write start signal
    sc_vector<sc_in<bool>> wr_en_i;                     // Write enable signal  
    sc_vector<sc_out<bool>> wr_ready_o_vec;  // Write ready status output
    
    // ========== FFT Control Ports ==========
    sc_in<bool> fft_mode_i;                  // FFT mode control
    sc_in<sc_uint<4>> fft_shift_i;           // FFT shift control
    sc_in<bool> fft_conj_en_i;               // FFT conjugate enable
    sc_vector<sc_in<bool>> stage_bypass_en;
    
    // ========== FFT Processing Control ==========
    sc_in<bool> fft_start_i;                 // FFT processing start signal
    sc_out<bool> input_ready_o;              // Input buffer ready status
    sc_out<bool> input_empty_o;              // Input buffer empty status

    
    // ========== Output Buffer Control ==========
    sc_in<bool> rd_start_i;                  // Output buffer read start signal
    sc_out<bool> output_ready_o;             // Output buffer ready status
    sc_out<bool> output_empty_o;             // Output buffer empty status
    
    // ========== Final Output Ports (16-way parallel) ==========
    sc_vector<sc_out<T>> data_o_vec;         // 16-way parallel data output
    sc_vector<sc_out<bool>> rd_valid_o_vec;  // 16-way valid signal output
    sc_vector<sc_out<bool>> wr_ready_out_vec; // Output buffer write ready status
    
    // ========== Twiddle Factor Interface ==========
    sc_in<bool> tw_load_en;                  // Twiddle load enable
    sc_in<sc_uint<8>> tw_stage_idx;          // Twiddle stage index
    sc_in<sc_uint<8>> tw_pe_idx;             // Twiddle PE index
    sc_in<complex<T>> tw_data;                        // Twiddle factor data
    
    // ========== Internal Components ==========
    IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>* input_buffer;  // Input buffer module (T for real/imag separation)
    FftMultiStage<T, N>* fft_core;                            // FFT computation core
    OUT_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>* output_buffer;    // Output buffer module (ComplexT input, T output)
    
    // ========== Internal Interconnection Signals ==========
    // Input buffer separated real/imaginary outputs (T type)
    sc_vector<sc_signal<T>> buf_group0_real;     // Group0 real parts [0-3]
    sc_vector<sc_signal<T>> buf_group0_imag;     // Group0 imag parts [4-7]  
    sc_vector<sc_signal<T>> buf_group1_real;     // Group1 real parts [0-3]
    sc_vector<sc_signal<T>> buf_group1_imag;     // Group1 imag parts [4-7]
    sc_vector<sc_signal<bool>> buf_group0_real_v;    // Group0 real valid signals
    sc_vector<sc_signal<bool>> buf_group0_imag_v;    // Group0 imag valid signals
    sc_vector<sc_signal<bool>> buf_group1_real_v;    // Group1 real valid signals
    sc_vector<sc_signal<bool>> buf_group1_imag_v;    // Group1 imag valid signals
    
    // Reconstructed complex signals for FFT core (ComplexT<T> type)
    sc_vector<sc_signal<complex<T>>> buf_to_fft_a;        // Reconstructed Group0 → FFT in_a ports
    sc_vector<sc_signal<complex<T>>> buf_to_fft_b;        // Reconstructed Group1 → FFT in_b ports  
    sc_vector<sc_signal<bool>> buf_to_fft_a_v;   // Group0 valid → FFT in_a_v
    sc_vector<sc_signal<bool>> buf_to_fft_b_v;   // Group1 valid → FFT in_b_v
    
    // Dummy signals for unused input buffer outputs
    sc_vector<sc_signal<T>> dummy_group0_data;   // Unused Group0 outputs [4-7]
    sc_vector<sc_signal<T>> dummy_group1_data;   // Unused Group1 outputs [4-7]
    sc_vector<sc_signal<bool>> dummy_group0_valid;   // Unused Group0 valid [4-7]
    sc_vector<sc_signal<bool>> dummy_group1_valid;   // Unused Group1 valid [4-7]
    
    // FFT core output signals (ComplexT<T> type)
    sc_vector<sc_signal<complex<T>>> fft_out_y0;          // FFT out_y0 complex output
    sc_vector<sc_signal<complex<T>>> fft_out_y1;          // FFT out_y1 complex output  
    sc_vector<sc_signal<bool>> fft_out_y0_v;     // FFT out_y0_v valid signals
    sc_vector<sc_signal<bool>> fft_out_y1_v;     // FFT out_y1_v valid signals
    
    
    // ========== Constructor Declaration ==========
    SC_CTOR(PEA_FFT);
    
    // ========== Destructor Declaration ==========
    ~PEA_FFT();

private:
    // ========== Internal Control Methods ==========
    void setup_connections();               // Setup all internal connections
    void map_buffer_to_fft_data();         // Map buffer groups to FFT inputs
    void setup_output_buffer_connections(); // Setup output buffer connections
    void complex_reconstruction_process();   // Process to reconstruct complex numbers from real/imag
};

// Include template implementations
#include "../src/pea_fft.cpp"

#endif // PEA_FFT_H