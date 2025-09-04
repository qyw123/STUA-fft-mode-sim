/**
 * @file pe_dual.h  
 * @brief  支持FFT蝶型算子和GEMM基础算子的双功能计算单元
 * 
 *   特性：
 * - FFT DIF蝶型算子：三级流水线实现（ 结构）
 * - GEMM基础MAC算子：脉动阵列PE标准c=a*b+c运算
 * - Stage Bypass功能：零延迟直通模式（集成式实现）
 * - 模式动态切换：FFT/GEMM运行时切换
 * - SystemC 架构：消除冗余进程和状态，优化内存使用
 * 
 * @version 3.0 (  版本)
 * @date 2025-08-28
 */

#ifndef PE_DUAL_H
#define PE_DUAL_H

#include "systemc.h"
#include "../utils/complex_types.h"
#include "../utils/config.h"


/**
 * @brief PE_DUAL双功能计算单元模板类
 * @tparam T 数据类型，默认为float
 */
template<typename T = float>
SC_MODULE(PE_DUAL) {

    
    // ====== 基础控制端口 ======
    sc_in_clk clk_i;              // 主时钟
    sc_in<bool> rst_i;            // 复位信号 (低有效)
    
    // ====== 数据通路端口 ======  
    sc_in<complex<T>> x_i{"x_i"};          // 水平数据输入
    sc_in<bool> x_v_i{"x_v_i"};   // 水平数据有效
    sc_in<complex<T>> mac_i{"mac_i"};      // 垂直MAC输入
    sc_in<bool> mac_v_i{"mac_v_i"}; // 垂直MAC有效
    
    sc_out<complex<T>> x_o{"x_o"};         // 水平数据输出
    sc_out<bool> x_v_o{"x_v_o"};  // 水平数据有效输出
    sc_out<complex<T>> mac_o{"mac_o"};     // MAC结果输出
    sc_out<bool> mac_v_o{"mac_v_o"}; // MAC有效输出
    
    // ====== 权重/Twiddle端口 ======
    sc_in<complex<T>> w_i{"w_i"};          // 权重/Twiddle输入
    sc_in<bool> wr_en_i{"wr_en_i"}; // 权重写使能
    
    // ====== FFT模式控制端口 ======
    sc_in<bool> fft_mode_i{"fft_mode_i"};           // FFT/GEMM模式选择
    sc_in<sc_uint<4>> fft_shift_i{"fft_shift_i"};   // FFT缩放因子
    sc_in<bool> fft_conj_en_i{"fft_conj_en_i"};     // FFT共轭使能
    
    // ====== Bypass控制端口 ======
    sc_in<bool> stage_bypass_en{"stage_bypass_en"}; // Stage级bypass使能
    
    
private:
    // ====== 内部权重寄存器 ======
    complex<T> w_gemm_r;           // GEMM权重寄存器
    bool w_gemm_valid;    // GEMM权重有效标志
    
    T w_fft_re_r;       // FFT twiddle实部
    T w_fft_im_r;       // FFT twiddle虚部
    bool w_fft_valid;     // FFT twiddle有效标志
    
    // ====== 模式控制寄存器 ======
    bool mode_r;          // 当前运行模式 (0=GEMM, 1=FFT)
    bool mode_switching;  // 模式切换状态标志
    
    // ====== 延时控制寄存器 ======
    enum ComputeState { IDLE, COMPUTING, READY };
    ComputeState fft_state;      // FFT计算状态
    ComputeState gemm_state;     // GEMM计算状态
    int fft_delay_counter;       // FFT延时计数器
    int gemm_delay_counter;      // GEMM延时计数器
    
    // 计算过程中的中间数据存储
    complex<T> fft_temp_y0, fft_temp_y1;   // FFT计算结果临时存储
    complex<T> gemm_temp_mac, gemm_temp_x; // GEMM计算结果临时存储
    
    // ====== 计算支路输出寄存器 ======
    complex<T> gemm_mac_r;         // GEMM MAC结果
    complex<T> gemm_x_r;           // GEMM X传递结果
    bool gemm_mac_v_r;    // GEMM MAC有效标志
    bool gemm_x_v_r;      // GEMM X有效标志
    
    complex<T> fft_y0_r;           // FFT输出Y0
    complex<T> fft_y1_r;           // FFT输出Y1  
    bool fft_y0_v_r;      // FFT Y0有效标志
    bool fft_y1_v_r;      // FFT Y1有效标志
    
    
public:
    // ====== 构造函数 ======
    SC_CTOR(PE_DUAL);
    
    // ====== 进程方法声明 (  版) ======
    void weight_and_data_proc();    // 权重和数据处理 (时钟同步)
    void mac_and_valid_proc();      // MAC计算和有效信号处理 (时钟同步) 
    void output_mux_proc();         // 输出多路选择 (时钟同步，包含bypass逻辑)
    
private:
    // ====== 内部辅助方法 ======
    bool is_fft_ready() const;      // FFT就绪状态检测
    bool is_gemm_ready() const;     // GEMM就绪状态检测
    void reset_internal_state();   // 内部状态复位
    
    // ====== 延时控制状态机 ======
    void process_delay_state_machines(); // 处理FFT和GEMM延时状态机
    
    // ====== 计算方法 (带延时控制版本) ======
    void perform_fft(); // FFT计算逻辑 (带延时控制)
    void perform_gemm_computation();// GEMM计算逻辑 (带延时控制)
};


#endif // PE_DUAL_H