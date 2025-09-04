/**
 * @file fft_shuffle_dyn.cpp
 * @brief FFT蝶型运算级间数据混洗模块实现 - 简洁高效版
 * 
 * @version 3.0 - THINK ULTRA简化版
 * @date 2025-08-29
 */

#include "../include/fft_shuffle_dyn.h"

template<typename T, unsigned N>
FftShuffleDyn<T,N>::FftShuffleDyn(sc_module_name name) : sc_module(name) {
    // 混洗计算和输出进程 (时钟同步)
    SC_THREAD(shuffle_compute_and_output_proc);
    sensitive << clk_i.pos();
    
    // 初始化内部状态
    reset_internal_state();
}

// ====== 内部状态复位方法 ======
template<typename T, unsigned N>
void FftShuffleDyn<T,N>::reset_internal_state() {
    shuffle_state = IDLE;
    shuffle_delay_counter = 0;
    current_fft_mode = false;
    current_stage_idx = 0;
    
    // 清空临时存储
    temp_out_a.fill(T{});
    temp_out_b.fill(T{});
    temp_out_a_v.fill(false);
    temp_out_b_v.fill(false);
}

// ====== 混洗计算和输出进程 ======
template<typename T, unsigned N>
void FftShuffleDyn<T,N>::shuffle_compute_and_output_proc() {
    while (true) {
        // 规范reset逻辑
        if (rst_i.read() == false) {
            shuffle_state = IDLE;
            shuffle_delay_counter = 0;
            current_fft_mode = false;
            current_stage_idx = 0;
            
            // 清除所有输出
            for (unsigned k = 0; k < NUM_PES; k++) {
                out_a[k].write(T{});
                out_b[k].write(T{});
                out_a_v[k].write(false);
                out_b_v[k].write(false);
            }
        } else {
            // 延时控制状态机处理
            process_delay_state_machine();
            
            // 混洗计算处理
            perform_shuffle_computation();
        }
        wait(); // 等待下一个时钟上升沿
    }
}

// ====== 延时控制状态机处理 ======
template<typename T, unsigned N>
void FftShuffleDyn<T,N>::process_delay_state_machine() {
    if (shuffle_state == COMPUTING) {
        shuffle_delay_counter++;
        if (shuffle_delay_counter > SHUFFLE_OPERATION_CYCLES) {
            shuffle_state = READY;
            shuffle_delay_counter = 0;
            
            // 延时完成，输出结果
            for (unsigned k = 0; k < NUM_PES; k++) {
                out_a[k].write(temp_out_a[k]);
                out_b[k].write(temp_out_b[k]);
                out_a_v[k].write(temp_out_a_v[k]);
                out_b_v[k].write(temp_out_b_v[k]);
            }
            
            cout << sc_time_stamp() << " " << this->name() 
                 << " 混洗延时计算完成" << endl;
        }
    } else if (shuffle_state == READY) {
        // 清除有效信号并回到空闲状态
        for (unsigned k = 0; k < NUM_PES; k++) {
            out_a_v[k].write(false);
            out_b_v[k].write(false);
        }
        shuffle_state = IDLE;
    }
}

// ====== 混洗计算逻辑实现 ======
template<typename T, unsigned N>
void FftShuffleDyn<T,N>::perform_shuffle_computation() {
    // 检查有效输入
    bool any_valid_input = false;
    for (unsigned k = 0; k < NUM_PES; ++k) {
        if (in_y0_v[k].read() || in_y1_v[k].read()) {
            any_valid_input = true;
            break;
        }
    }
    
    // 只有在IDLE状态且有有效输入才能启动新的计算
    if (any_valid_input && (shuffle_state == IDLE)) {
        cout << sc_time_stamp() << " " << this->name() 
             << " 混洗计算启动，开始" << SHUFFLE_OPERATION_CYCLES << "周期延时" << endl;
        
        // 读取控制信号
        current_fft_mode = fft_mode_i.read();
        current_stage_idx = stage_idx.read();
        //cout << "stage_idx: " << current_stage_idx << endl;
        
        // 构建输入数据数组 [y0_0,y1_0,y0_1,y1_1,...]
        std::array<complex<T>, N> input_data;
        std::array<bool, N> input_valid;
        for (unsigned k = 0; k < NUM_PES; ++k) {
            input_data[2*k]     = in_y0[k].read();      // y0_k → lane[2k]
            input_data[2*k + 1] = in_y1[k].read();      // y1_k → lane[2k+1]
            input_valid[2*k]    = in_y0_v[k].read();
            input_valid[2*k + 1] = in_y1_v[k].read();
        }
        
        // 立即进行混洗计算并存储到临时寄存器
        if (current_fft_mode) {
            // GEMM模式：直接旁路传递
            for (unsigned k = 0; k < NUM_PES; ++k) {
                temp_out_a[k] = input_data[2*k];          // lane[2k] → a_k
                temp_out_b[k] = input_data[2*k + 1];      // lane[2k+1] → b_k
                temp_out_a_v[k] = input_valid[2*k];
                temp_out_b_v[k] = input_valid[2*k + 1];
            }
        } else {
            // FFT模式：动态stride混洗算法
            const unsigned s = static_cast<unsigned>(current_stage_idx);
            const unsigned stride = (N >> (s + 2));
            const unsigned half_pes = NUM_PES / 2;
            
            auto y0_lane = [](unsigned u) { return 2 * u; };
            auto y1_lane = [](unsigned u) { return 2 * u + 1; };
            
            for (unsigned p = 0; p < half_pes; ++p) {
                unsigned i = (p / stride) * (2 * stride) + (p % stride);
                
                unsigned k0 = p;
                unsigned a0 = y0_lane(i);
                unsigned b0 = y0_lane(i + stride);
                temp_out_a[k0] = input_data[a0];
                temp_out_b[k0] = input_data[b0];
                temp_out_a_v[k0] = input_valid[a0];
                temp_out_b_v[k0] = input_valid[b0];
                
                unsigned k1 = p + half_pes;
                unsigned a1 = y1_lane(i);
                unsigned b1 = y1_lane(i + stride);
                temp_out_a[k1] = input_data[a1];
                temp_out_b[k1] = input_data[b1];
                temp_out_a_v[k1] = input_valid[a1];
                temp_out_b_v[k1] = input_valid[b1];
            }
        }
        
        // 启动延时状态机
        shuffle_state = COMPUTING;
        shuffle_delay_counter = 1; // 从第1个周期开始计数
    }
}



// ====== 模板显式实例化 ======
template class FftShuffleDyn<float, 4>;
template class FftShuffleDyn<float, 8>;
template class FftShuffleDyn<float, 16>;
template class FftShuffleDyn<float, 32>;
template class FftShuffleDyn<float, 64>;