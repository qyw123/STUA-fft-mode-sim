/**
 * @file fft_multi_stage.cpp
 * @brief 多级FFT流水线模块实现 - 简洁高效版
 * 
 * @version 3.0 - 精简版
 * @date 2025-08-29
 */

#include "../include/fft_multi_stage.h"
#include <iostream>
using namespace std;

// ====== FftStageRow实现 ======
template<typename T, unsigned N>
FftStageRow<T,N>::FftStageRow(sc_module_name name) : sc_module(name), stage_gen_counter(0) {
    cout << "[STAGE_INIT] " << this->name() << " FftStageRow构造函数开始" << endl;
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
    SC_METHOD(stage_control_proc);
    sensitive << clk_i.pos();
    
    // 数据监控进程
    SC_METHOD(data_monitor_proc);
    sensitive << clk_i.pos();
    
    // 总体监控进程
    SC_METHOD(stage_general_monitor_proc);
    sensitive << clk_i.pos();
}

template<typename T, unsigned N>
void FftStageRow<T,N>::stage_control_proc() {
    // while (true) {
    //     // 复位处理
        if (rst_i.read() == false) {
            for (unsigned k = 0; k < NUM_PES; ++k) {
                twiddle_sig[k].write(complex<T>(0,0));
                twiddle_en_sig[k].write(false);
            }
        } else {
            // Twiddle因子分发
            if (tw_load_en.read()) {
                unsigned target_pe = tw_pe_idx.read();
                cout << "[STAGE_DEBUG] " << this->name() << " 收到Twiddle加载信号 target_pe=" << target_pe << endl;
                if (target_pe < NUM_PES) {
                    twiddle_sig[target_pe].write(tw_data.read());
                    twiddle_en_sig[target_pe].write(true);
                    cout << "[STAGE_DEBUG] " << this->name() << " 向PE[" << target_pe << "]发送Twiddle数据" << endl;
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
    //     wait();
    // }
}

template<typename T, unsigned N>
void FftStageRow<T,N>::data_monitor_proc() {
    static int stage_monitor_counter = 0;
    if (++stage_monitor_counter % 50 == 0) {
        bool has_valid_input = false;
        for (unsigned k = 0; k < NUM_PES; ++k) {
            if (a_v_i[k].read() || b_v_i[k].read()) {
                has_valid_input = true;
                break;
            }
        }
        
        if (has_valid_input) {
            cout << "[STAGE_DATA] " << this->name() << " 检测到有效输入数据" << endl;
            for (unsigned k = 0; k < NUM_PES && k < 4; ++k) {
                cout << "[STAGE_DATA] PE[" << k << "] a_v=" << a_v_i[k].read() 
                     << " b_v=" << b_v_i[k].read() << endl;
            }
        }
    }
}

template<typename T, unsigned N>
void FftStageRow<T,N>::stage_general_monitor_proc() {
    stage_gen_counter++;
    
    // 高频检查以捕获数据传播 - 简化版本，只关注非零数据
    for (unsigned k = 0; k < NUM_PES; ++k) {
        if (a_v_i[k].read() || b_v_i[k].read()) {
            complex<T> data_a = a_i[k].read();
            complex<T> data_b = b_i[k].read();
            
            // 只显示非零数据，减少噪音
            if (data_a.real != 0 || data_a.imag != 0 || data_b.real != 0 || data_b.imag != 0) {
                cout << "[STAGE_NONZERO] " << this->name() << " PE[" << k << "] 非零数据! A=" << data_a
                     << " B=" << data_b << " a_v=" << a_v_i[k].read() 
                     << " b_v=" << b_v_i[k].read() << endl;
            }
        }
    }
    
    bool has_data = false;
    for (unsigned k = 0; k < NUM_PES; ++k) {
        if (a_v_i[k].read() || b_v_i[k].read()) {
            has_data = true;
            break;
        }
    }
    
    // 定期状态报告
    if (stage_gen_counter <= 5 || stage_gen_counter % 200 == 0) {
        cout << "[STAGE_GEN] " << this->name() << " (调用#" << stage_gen_counter 
             << ") 运行状态 - rst=" << rst_i.read() 
             << " fft_mode=" << fft_mode_i.read() << " bypass=" << stage_bypass_en.read() << endl;
        
        if (!has_data) {
            cout << "[STAGE_GEN] " << this->name() << " 当前无有效数据输入" << endl;
        }
        
        if (tw_load_en.read()) {
            cout << "[STAGE_GEN] " << this->name() << " Twiddle加载使能激活" << endl;
        }
    }
}

// ====== FftMultiStage实现 ======
template<typename T, unsigned N>
FftMultiStage<T,N>::FftMultiStage(sc_module_name name) : sc_module(name) {
    cout << "[MULTI_INIT] " << this->name() << " N=" << N << " NUM_STAGES=" << NUM_STAGES 
         << " NUM_SHUFFLES=" << NUM_SHUFFLES << " NUM_PES=" << NUM_PES << endl;
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
    cout << "[MULTI_INIT] 建立 " << NUM_STAGES << " 个stage的连接..." << endl;
    for (unsigned s = 0; s < NUM_STAGES; ++s) {
        cout << "[MULTI_INIT] 配置stage[" << s << "]..." << endl;
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
            cout << "[MULTI_CONN] 配置stage[0]输入端口连接..." << endl;
            for (unsigned k = 0; k < NUM_PES; ++k) {
                stages[s].a_i[k](in_a[k]);
                stages[s].b_i[k](in_b[k]);
                stages[s].a_v_i[k](in_a_v[k]);
                stages[s].b_v_i[k](in_b_v[k]);
                cout << "[MULTI_CONN] stage[0] PE[" << k << "] 连接完成" << endl;
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
    SC_METHOD(pipeline_control_proc);
    sensitive << clk_i.pos() << tw_load_en << tw_stage_idx << tw_pe_idx << tw_data;
    
    // 多级流水线数据监控进程
    SC_METHOD(input_data_monitor_proc);
    sensitive << clk_i.pos();
    
    // 信号测试进程
    SC_METHOD(signal_test_proc);
    sensitive << clk_i.pos();
    
    // Twiddle信号连接测试进程
    SC_METHOD(twiddle_test_proc);
    sensitive << tw_load_en << tw_stage_idx;
}

template<typename T, unsigned N>
void FftMultiStage<T,N>::pipeline_control_proc() {
    // 调试：确保进程被调用
    static int pipeline_debug_counter = 0;
    if (++pipeline_debug_counter % 1000 == 0 || tw_load_en.read()) {
        cout << "[PIPELINE_PROC] " << this->name() << " pipeline_control_proc调用 #" << pipeline_debug_counter 
             << " tw_load_en=" << tw_load_en.read() << endl;
    }
    
    // 复位处理 - 不直接写输出信号，避免多驱动冲突
    if (rst_i.read() == false) {
        if (tw_load_en.read()) {
            cout << "[PIPELINE_PROC] " << this->name() << " 在复位状态中，忽略Twiddle加载" << endl;
        }
        // 仅处理内部控制逻辑，输出由各级PE负责
        for (unsigned s = 0; s < NUM_STAGES; ++s) {
            stage_tw_load_en_sig[s].write(false);
            stage_tw_pe_idx_sig[s].write(sc_uint<8>(0));
            stage_tw_data_sig[s].write(complex<T>(0,0));
        }
    } else {
        // Twiddle分发处理 - 无论时钟还是信号变化都处理
        bool load_active = tw_load_en.read();
        unsigned target_stage = tw_stage_idx.read();
        
        if (load_active) {
            cout << "[MULTI_TWIDDLE] " << this->name() << " 收到Twiddle加载请求: target_stage=" << target_stage 
                 << ", pe_idx=" << tw_pe_idx.read() << ", data=" << tw_data.read() << endl;
        }
        
        for (unsigned s = 0; s < NUM_STAGES; ++s) {
            bool stage_selected = load_active && (s == target_stage);
            stage_tw_load_en_sig[s].write(stage_selected);
            stage_tw_pe_idx_sig[s].write(stage_selected ? tw_pe_idx.read() : sc_uint<8>(0));
            stage_tw_data_sig[s].write(stage_selected ? tw_data.read() : complex<T>(0,0));
            
            if (stage_selected) {
                cout << "[MULTI_TWIDDLE] " << this->name() << " 向stage[" << s << "]转发Twiddle加载信号" << endl;
            }
        }
    }
}


template<typename T, unsigned N>
void FftMultiStage<T,N>::input_data_monitor_proc() {
    static int multi_monitor_counter = 0;
    if (++multi_monitor_counter % 50 == 0) {
        bool has_valid_input = false;
        for (unsigned k = 0; k < NUM_PES; ++k) {
            if (in_a_v[k].read() || in_b_v[k].read()) {
                has_valid_input = true;
                break;
            }
        }
        
        if (has_valid_input) {
            cout << "[MULTI_DATA] " << this->name() << " 检测到有效输入数据" << endl;
            for (unsigned k = 0; k < NUM_PES && k < 4; ++k) {
                if (in_a_v[k].read() || in_b_v[k].read()) {
                    cout << "[MULTI_DATA] MultiStage输入 PE[" << k << "] A=" << in_a[k].read() 
                         << " B=" << in_b[k].read() << " a_v=" << in_a_v[k].read() 
                         << " b_v=" << in_b_v[k].read() << endl;
                }
            }
        }
    }
}

template<typename T, unsigned N>
void FftMultiStage<T,N>::signal_test_proc() {
    static int test_counter = 0;
    test_counter++;
    
    // 只在前几次和有数据时测试
    if (test_counter <= 5 || (test_counter % 100 == 0)) {
        bool has_input_data = false;
        for (unsigned k = 0; k < NUM_PES; ++k) {
            if (in_a_v[k].read() || in_b_v[k].read()) {
                has_input_data = true;
                break;
            }
        }
        
        if (has_input_data) {
            cout << "[SIGNAL_TEST] " << this->name() << " 信号测试 #" << test_counter << endl;
            for (unsigned k = 0; k < NUM_PES && k < 2; ++k) {
                cout << "[SIGNAL_TEST] MultiStage PE[" << k << "] 输入: A=" << in_a[k].read() 
                     << " B=" << in_b[k].read() << " a_v=" << in_a_v[k].read() << endl;
                
                // 尝试读取stage[0]的输入端口 - 这需要通过不同的方法
                cout << "[SIGNAL_TEST] 等待下个时钟查看stage[0]是否接收..." << endl;
            }
        }
    }
}

template<typename T, unsigned N>
void FftMultiStage<T,N>::twiddle_test_proc() {
    bool load_en = tw_load_en.read();
    unsigned stage_idx = tw_stage_idx.read();
    
    if (load_en) {
        cout << "[MULTI_TWIDDLE_TEST] " << this->name() << " 检测到Twiddle加载信号! stage_idx=" << stage_idx 
             << ", pe_idx=" << tw_pe_idx.read() << endl;
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

