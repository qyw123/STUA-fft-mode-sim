/**
 * @file fft_shuffle_dyn.h
 * @brief FFT蝶型运算级间数据混洗模块 - 简洁高效版
 * 
 * 特性：
 * - 双阶段流水线：Collect→Shuffle (2周期延时)
 * - 动态stride计算：stride = N >> (s+2)
 * - 硬件友好的SystemC建模
 * - PE分区策略：Y0→[0..N/4-1], Y1→[N/4..N/2-1]
 * 
 * @version 3.0 - THINK ULTRA简化版
 * @date 2025-08-29
 */

#ifndef FFT_SHUFFLE_DYN_H
#define FFT_SHUFFLE_DYN_H

#include "systemc.h"
#include "../utils/complex_types.h"
#include "../utils/config.h"
#include <array>

/**
 * @brief FFT动态完美混洗模块
 * @tparam T 复数的实/虚数据类型 
 * @tparam N FFT大小，必须为2的幂次 (N=2^S)
 */
template<typename T, unsigned N>
SC_MODULE(FftShuffleDyn) {
    static_assert(N >= 2 && ((N & (N-1)) == 0), "N must be power of two");
    static constexpr unsigned NUM_PES = N/2;
    
    // ====== 基础控制端口 ======
    sc_in_clk clk_i{"clk_i"};
    sc_in<bool> rst_i{"rst_i"};
    sc_in<bool> fft_mode_i{"fft_mode_i"};     // 0=FFT(混洗), 1=GEMM(直通)
    sc_in<sc_uint<8>> stage_idx{"stage_idx"}; // 级间混洗索引 (0..S-2)

    // ====== 数据通路端口 ======
    sc_vector<sc_in<complex<T>>>    in_y0{"in_y0", NUM_PES};      // Y0输入 (来自PE mac_o)
    sc_vector<sc_in<complex<T>>>    in_y1{"in_y1", NUM_PES};      // Y1输入 (来自PE x_o)
    sc_vector<sc_in<bool>> in_y0_v{"in_y0_v", NUM_PES};  // Y0有效信号
    sc_vector<sc_in<bool>> in_y1_v{"in_y1_v", NUM_PES};  // Y1有效信号

    sc_vector<sc_out<complex<T>>>    out_a{"out_a", NUM_PES};     // A输出 (到PE mac_i)
    sc_vector<sc_out<complex<T>>>    out_b{"out_b", NUM_PES};     // B输出 (到PE x_i)
    sc_vector<sc_out<bool>> out_a_v{"out_a_v", NUM_PES}; // A有效信号
    sc_vector<sc_out<bool>> out_b_v{"out_b_v", NUM_PES}; // B有效信号

    // ====== THINK ULTRA延时配置 ======


private:
    // ====== 延时控制寄存器 ======
    enum ShuffleState { IDLE, COMPUTING, READY };
    ShuffleState shuffle_state;         // 混洗计算状态
    int shuffle_delay_counter;          // 混洗延时计数器
    
    // ====== 计算结果临时存储 ======
    std::array<complex<T>, N> temp_out_a;       // A输出临时存储
    std::array<complex<T>, N> temp_out_b;       // B输出临时存储
    std::array<bool, N> temp_out_a_v;  // A有效临时存储
    std::array<bool, N> temp_out_b_v;  // B有效临时存储
    
    // ====== 控制信号寄存器 ======
    bool current_fft_mode;              // 当前FFT模式
    sc_uint<8> current_stage_idx;       // 当前级索引

public:
    // ====== 构造函数 ======
    SC_CTOR(FftShuffleDyn);
    
private:
    // ====== 进程方法 ======
    void shuffle_compute_and_output_proc();  // 混洗计算和输出进程
    void process_delay_state_machine();      // 处理延时状态机
    void perform_shuffle_computation();      // 执行混洗计算
    void reset_internal_state();             // 内部状态复位
};

#endif // FFT_SHUFFLE_DYN_H