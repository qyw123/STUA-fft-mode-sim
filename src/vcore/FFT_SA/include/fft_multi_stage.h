/**
 * @file fft_multi_stage.h
 * @brief 多级FFT流水线模块 - 简洁高效版
 * 
 * @version 3.0 - 精简版
 * @date 2025-08-29
 */

#ifndef FFT_MULTI_STAGE_H
#define FFT_MULTI_STAGE_H

#include "systemc.h"
#include "../utils/complex_types.h"
#include "pe_dual.h"
#include "fft_shuffle_dyn.h"
#include <array>

// ====== 编译时工具函数 ======
constexpr unsigned log2_const(unsigned n) {
    return (n <= 1) ? 0 : 1 + log2_const(n >> 1);
}

/**
 * @brief FFT级行模块
 * @tparam T 复数的实/虚数据类型 ()  
 * @tparam N FFT大小，必须为2的幂次 (N=2^S)
 */
template<typename T, unsigned N>
SC_MODULE(FftStageRow) {
    static_assert(N >= 2 && ((N & (N-1)) == 0), "N must be power of two");
    static constexpr unsigned NUM_PES = N/2;
    
    // ====== 控制端口 ======
    sc_in_clk clk_i{"clk_i"};
    sc_in<bool> rst_i{"rst_i"};
    sc_in<bool> fft_mode_i{"fft_mode_i"};
    sc_in<sc_uint<4>> fft_shift_i{"fft_shift_i"};
    sc_in<bool> fft_conj_en_i{"fft_conj_en_i"};
    sc_in<bool> stage_bypass_en{"stage_bypass_en"};
    
    // ====== 数据端口 ======
    sc_vector<sc_in<complex<T>>>    a_i{"a_i", NUM_PES};
    sc_vector<sc_in<complex<T>>>    b_i{"b_i", NUM_PES};
    sc_vector<sc_in<bool>> a_v_i{"a_v_i", NUM_PES};
    sc_vector<sc_in<bool>> b_v_i{"b_v_i", NUM_PES};
    
    sc_vector<sc_out<complex<T>>>    y0_o{"y0_o", NUM_PES};
    sc_vector<sc_out<complex<T>>>    y1_o{"y1_o", NUM_PES};
    sc_vector<sc_out<bool>> y0_v_o{"y0_v_o", NUM_PES};
    sc_vector<sc_out<bool>> y1_v_o{"y1_v_o", NUM_PES};
    
    // ====== Twiddle接口 ======
    sc_in<bool>       tw_load_en{"tw_load_en"};
    sc_in<sc_uint<8>> tw_pe_idx{"tw_pe_idx"};
    sc_in<complex<T>>          tw_data{"tw_data"};
    
    // ====== PE阵列 ======
    sc_vector<PE_DUAL<T>> pes{"pes", NUM_PES};

private:
    // ====== 内部信号 ======
    sc_vector<sc_signal<complex<T>>> twiddle_sig{"twiddle_sig", NUM_PES};
    sc_vector<sc_signal<bool>> twiddle_en_sig{"twiddle_en_sig", NUM_PES};
    
public:
    SC_CTOR(FftStageRow);
    
private:
    void stage_control_proc();
};

/**
 * @brief 多级FFT流水线模块
 * @tparam T 复数数据类型 (ComplexT<scalar_t>)
 * @tparam N FFT大小，必须为2的幂次 (N=2^S)
 */
template<typename T, unsigned N>
SC_MODULE(FftMultiStage) {
    static_assert(N >= 2 && ((N & (N-1)) == 0), "N must be power of two");
    static constexpr unsigned NUM_PES = N/2;
    static constexpr unsigned NUM_STAGES = log2_const(N);
    static constexpr unsigned NUM_SHUFFLES = (NUM_STAGES > 1) ? (NUM_STAGES - 1) : 0;
    
    // ====== 控制端口 ======
    sc_in_clk clk_i{"clk_i"};
    sc_in<bool> rst_i{"rst_i"};
    sc_in<bool> fft_mode_i{"fft_mode_i"};
    sc_in<sc_uint<4>> fft_shift_i{"fft_shift_i"};
    sc_in<bool> fft_conj_en_i{"fft_conj_en_i"};
    sc_vector<sc_in<bool>> stage_bypass_en{"stage_bypass_en", NUM_STAGES};
    
    // ====== 数据端口 ======
    sc_vector<sc_in<complex<T>>>    in_a{"in_a", NUM_PES};
    sc_vector<sc_in<complex<T>>>    in_b{"in_b", NUM_PES};
    sc_vector<sc_in<bool>> in_a_v{"in_a_v", NUM_PES};
    sc_vector<sc_in<bool>> in_b_v{"in_b_v", NUM_PES};
    
    sc_vector<sc_out<complex<T>>>    out_y0{"out_y0", NUM_PES};
    sc_vector<sc_out<complex<T>>>    out_y1{"out_y1", NUM_PES};
    sc_vector<sc_out<bool>> out_y0_v{"out_y0_v", NUM_PES};
    sc_vector<sc_out<bool>> out_y1_v{"out_y1_v", NUM_PES};
    
    // ====== Twiddle接口 ======
    sc_in<bool>         tw_load_en{"tw_load_en"};
    sc_in<sc_uint<8>>   tw_stage_idx{"tw_stage_idx"};
    sc_in<sc_uint<8>>   tw_pe_idx{"tw_pe_idx"};
    sc_in<complex<T>>            tw_data{"tw_data"};
    
    // ====== 核心组件 ======
    sc_vector<FftStageRow<T,N>> stages{"stages", NUM_STAGES};
    sc_vector<FftShuffleDyn<T,N>> shuffles{"shuffles", NUM_SHUFFLES};
    
private:
    // ====== 级间互连信号 ======
    sc_vector<sc_vector<sc_signal<complex<T>>>> inter_y0{"inter_y0", NUM_SHUFFLES};
    sc_vector<sc_vector<sc_signal<complex<T>>>> inter_y1{"inter_y1", NUM_SHUFFLES};
    sc_vector<sc_vector<sc_signal<bool>>> inter_y0_v{"inter_y0_v", NUM_SHUFFLES};
    sc_vector<sc_vector<sc_signal<bool>>> inter_y1_v{"inter_y1_v", NUM_SHUFFLES};
    
    sc_vector<sc_vector<sc_signal<complex<T>>>> inter_a{"inter_a", NUM_SHUFFLES};
    sc_vector<sc_vector<sc_signal<complex<T>>>> inter_b{"inter_b", NUM_SHUFFLES};
    sc_vector<sc_vector<sc_signal<bool>>> inter_a_v{"inter_a_v", NUM_SHUFFLES};
    sc_vector<sc_vector<sc_signal<bool>>> inter_b_v{"inter_b_v", NUM_SHUFFLES};
    
    // ====== 控制信号 ======
    sc_vector<sc_signal<sc_uint<8>>> shuffle_stage_idx_sig{"shuffle_stage_idx_sig", NUM_SHUFFLES};
    sc_vector<sc_signal<bool>> shuffle_fft_mode_sig{"shuffle_fft_mode_sig", NUM_SHUFFLES};
    
    sc_vector<sc_signal<bool>> stage_tw_load_en_sig{"stage_tw_load_en_sig", NUM_STAGES};
    sc_vector<sc_signal<sc_uint<8>>> stage_tw_pe_idx_sig{"stage_tw_pe_idx_sig", NUM_STAGES};
    sc_vector<sc_signal<complex<T>>> stage_tw_data_sig{"stage_tw_data_sig", NUM_STAGES};
    
public:
    SC_CTOR(FftMultiStage);
    
private:
    void pipeline_control_proc();
};

#endif // FFT_MULTI_STAGE_H