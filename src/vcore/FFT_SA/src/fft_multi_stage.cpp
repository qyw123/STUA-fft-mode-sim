/**
 * @file fft_multi_stage.cpp
 * @brief 多级FFT流水线模块实现 - 简洁高效版
 * 
 * @version 3.0 - 精简版
 * @date 2025-08-29
 */

#include "../include/fft_multi_stage.h"

// ====== FftStageRow实现 ======
template<typename T, unsigned N>
FftStageRow<T,N>::FftStageRow(sc_module_name name) : sc_module(name) {
    // PE阵列连接
    for (unsigned k = 0; k < NUM_PES; ++k) {
        // 时钟和控制连接
        pes[k].clk_i(clk_i);
        pes[k].rst_i(rst_i);
        pes[k].fft_mode_i(fft_mode_i);
        pes[k].fft_shift_i(fft_shift_i);
        pes[k].fft_conj_en_i(fft_conj_en_i);
        pes[k].stage_bypass_en(stage_bypass_en);
        
        // 数据通路连接
        pes[k].mac_i(a_i[k]);
        pes[k].x_i(b_i[k]);
        pes[k].mac_v_i(a_v_i[k]);
        pes[k].x_v_i(b_v_i[k]);
        
        pes[k].mac_o(y0_o[k]);
        pes[k].x_o(y1_o[k]);
        pes[k].mac_v_o(y0_v_o[k]);
        pes[k].x_v_o(y1_v_o[k]);
        
        // Twiddle连接
        pes[k].w_i(twiddle_sig[k]);
        pes[k].wr_en_i(twiddle_en_sig[k]);
    }
    
    // 级控制进程
    SC_THREAD(stage_control_proc);
    sensitive << clk_i.pos();
}

template<typename T, unsigned N>
void FftStageRow<T,N>::stage_control_proc() {
    while (true) {
        // 复位处理
        if (rst_i.read() == false) {
            for (unsigned k = 0; k < NUM_PES; ++k) {
                twiddle_sig[k].write(complex<T>(0,0));
                twiddle_en_sig[k].write(false);
            }
        } else {
            // Twiddle因子分发
            if (tw_load_en.read()) {
                unsigned target_pe = tw_pe_idx.read();
                if (target_pe < NUM_PES) {
                    twiddle_sig[target_pe].write(tw_data.read());
                    twiddle_en_sig[target_pe].write(true);
                } else {
                    for (unsigned k = 0; k < NUM_PES; ++k) {
                        twiddle_en_sig[k].write(false);
                    }
                }
            } else {
                for (unsigned k = 0; k < NUM_PES; ++k) {
                    twiddle_en_sig[k].write(false);
                }
            }
        }
        wait();
    }
}

// ====== FftMultiStage实现 ======
template<typename T, unsigned N>
FftMultiStage<T,N>::FftMultiStage(sc_module_name name) : sc_module(name) {
    // 初始化信号向量
    for (unsigned s = 0; s < NUM_SHUFFLES; ++s) {
        inter_y0[s].init(NUM_PES);
        inter_y1[s].init(NUM_PES);
        inter_y0_v[s].init(NUM_PES);
        inter_y1_v[s].init(NUM_PES);
        inter_a[s].init(NUM_PES);
        inter_b[s].init(NUM_PES);
        inter_a_v[s].init(NUM_PES);
        inter_b_v[s].init(NUM_PES);
        
        shuffle_stage_idx_sig[s].write(static_cast<sc_uint<8>>(s));
        shuffle_fft_mode_sig[s].write(true);
    }
    
    // 建立级连接
    for (unsigned s = 0; s < NUM_STAGES; ++s) {
        stages[s].clk_i(clk_i);
        stages[s].rst_i(rst_i);
        stages[s].fft_mode_i(fft_mode_i);
        stages[s].fft_shift_i(fft_shift_i);
        stages[s].fft_conj_en_i(fft_conj_en_i);
        stages[s].stage_bypass_en(stage_bypass_en[s]);
        
        stages[s].tw_load_en(stage_tw_load_en_sig[s]);
        stages[s].tw_pe_idx(stage_tw_pe_idx_sig[s]);
        stages[s].tw_data(stage_tw_data_sig[s]);
        
        // 数据连接
        if (s == 0) {
            for (unsigned k = 0; k < NUM_PES; ++k) {
                stages[s].a_i[k](in_a[k]);
                stages[s].b_i[k](in_b[k]);
                stages[s].a_v_i[k](in_a_v[k]);
                stages[s].b_v_i[k](in_b_v[k]);
            }
        } else {
            for (unsigned k = 0; k < NUM_PES; ++k) {
                stages[s].a_i[k](inter_a[s-1][k]);
                stages[s].b_i[k](inter_b[s-1][k]);
                stages[s].a_v_i[k](inter_a_v[s-1][k]);
                stages[s].b_v_i[k](inter_b_v[s-1][k]);
            }
        }
        
        if (s == NUM_STAGES - 1) {
            for (unsigned k = 0; k < NUM_PES; ++k) {
                stages[s].y0_o[k](out_y0[k]);
                stages[s].y1_o[k](out_y1[k]);
                stages[s].y0_v_o[k](out_y0_v[k]);
                stages[s].y1_v_o[k](out_y1_v[k]);
            }
        } else {
            for (unsigned k = 0; k < NUM_PES; ++k) {
                stages[s].y0_o[k](inter_y0[s][k]);
                stages[s].y1_o[k](inter_y1[s][k]);
                stages[s].y0_v_o[k](inter_y0_v[s][k]);
                stages[s].y1_v_o[k](inter_y1_v[s][k]);
            }
        }
    }
    
    // 建立混洗连接
    for (unsigned s = 0; s < NUM_SHUFFLES; ++s) {
        shuffles[s].clk_i(clk_i);
        shuffles[s].rst_i(rst_i);
        shuffles[s].fft_mode_i(stage_bypass_en[s]);
        shuffles[s].stage_idx(shuffle_stage_idx_sig[s]);
        
        for (unsigned k = 0; k < NUM_PES; ++k) {
            shuffles[s].in_y0[k](inter_y0[s][k]);
            shuffles[s].in_y1[k](inter_y1[s][k]);
            shuffles[s].in_y0_v[k](inter_y0_v[s][k]);
            shuffles[s].in_y1_v[k](inter_y1_v[s][k]);
            
            shuffles[s].out_a[k](inter_a[s][k]);
            shuffles[s].out_b[k](inter_b[s][k]);
            shuffles[s].out_a_v[k](inter_a_v[s][k]);
            shuffles[s].out_b_v[k](inter_b_v[s][k]);
        }
    }
    
    // 流水线控制进程
    SC_THREAD(pipeline_control_proc);
    sensitive << clk_i.pos();
}

template<typename T, unsigned N>
void FftMultiStage<T,N>::pipeline_control_proc() {
    while (true) {
        // 复位处理 - 不直接写输出信号，避免多驱动冲突
        if (rst_i.read() == false) {
            // 仅处理内部控制逻辑，输出由各级PE负责
            for (unsigned s = 0; s < NUM_STAGES; ++s) {
                stage_tw_load_en_sig[s].write(false);
                stage_tw_pe_idx_sig[s].write(sc_uint<8>(0));
                stage_tw_data_sig[s].write(complex<T>(0,0));
            }
        } else {
            // Twiddle分发处理
            bool load_active = tw_load_en.read();
            unsigned target_stage = tw_stage_idx.read();
            
            for (unsigned s = 0; s < NUM_STAGES; ++s) {
                bool stage_selected = load_active && (s == target_stage);
                stage_tw_load_en_sig[s].write(stage_selected);
                stage_tw_pe_idx_sig[s].write(stage_selected ? tw_pe_idx.read() : sc_uint<8>(0));
                stage_tw_data_sig[s].write(stage_selected ? tw_data.read() : complex<T>(0,0));
            }
        }
        wait();
    }
}

// ====== 模板显式实例化 ======
template class FftStageRow<float, 4>;
template class FftStageRow<float, 8>;
template class FftStageRow<float, 16>;
template class FftStageRow<float, 32>;
template class FftStageRow<float, 64>;

template class FftMultiStage<float, 4>;
template class FftMultiStage<float, 8>;
template class FftMultiStage<float, 16>;
template class FftMultiStage<float, 32>;
template class FftMultiStage<float, 64>;

