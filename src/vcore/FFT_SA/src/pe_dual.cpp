#include "../include/pe_dual.h"
#include <iostream>
#include <map>
#include <systemc.h>

// ====== 构造函数实现 ======

template<typename T>
PE_DUAL<T>::PE_DUAL(sc_module_name name) : sc_module(name) {
    // 权重和数据处理进程 (时钟同步)
    SC_METHOD(weight_and_data_proc);
    sensitive << clk_i.pos();
    
    // MAC计算和有效信号处理进程 (时钟同步)
    SC_METHOD(mac_and_valid_proc);
    sensitive << clk_i.pos();
    
    // 输出多路选择进程
    SC_METHOD(output_mux_proc);
    sensitive << clk_i.pos() << stage_bypass_en << mac_v_i << x_v_i;
    
    // 初始化内部状态
    reset_internal_state();
}

// ====== 内部状态复位方法 ======

template<typename T>
void PE_DUAL<T>::reset_internal_state() {
    w_gemm_r = complex<T>(0, 0);
    w_gemm_valid = false;
    w_fft_re_r = T(0);
    w_fft_im_r = T(0);
    w_fft_valid = false;
    mode_r = false;
    mode_switching = false;
    
    // 延时控制状态初始化
    fft_state = IDLE;
    gemm_state = IDLE;
    fft_delay_counter = 0;
    gemm_delay_counter = 0;
    fft_temp_y0 = complex<T>(0, 0);
    fft_temp_y1 = complex<T>(0, 0);
    gemm_temp_mac = complex<T>(0, 0);
    gemm_temp_x = complex<T>(0, 0);
    
    gemm_mac_r = complex<T>(0, 0);
    gemm_x_r = complex<T>(0, 0);
    gemm_mac_v_r = false;
    gemm_x_v_r = false;
    fft_y0_r = complex<T>(0, 0);
    fft_y1_r = complex<T>(0, 0);
    fft_y0_v_r = false;
    fft_y1_v_r = false;
}

// ====== 权重和数据处理进程 ======

template<typename T>
void PE_DUAL<T>::weight_and_data_proc() {
    // while (true) {
        // 规范reset逻辑
        if (rst_i.read() == false) {
            w_gemm_r = complex<T>(0, 0);
            w_gemm_valid = false;
            w_fft_re_r = T(0);
            w_fft_im_r = T(0); 
            w_fft_valid = false;
            mode_r = false;
            mode_switching = false;
            // 延时控制复位
            fft_state = IDLE;
            gemm_state = IDLE;
            fft_delay_counter = 0;
            gemm_delay_counter = 0;
        } else {
            // 模式控制更新
            bool new_mode = fft_mode_i.read();
            if (new_mode != mode_r) {
                mode_switching = true;
                mode_r = new_mode;
            } else {
                mode_switching = false;
            }
            
            // 权重/Twiddle加载处理
            if (wr_en_i.read()) {
                bool current_mode = fft_mode_i.read();
                cout << "[PE_TWIDDLE] " << this->name() << " 收到Twiddle写使能信号，当前模式: " << (current_mode ? "FFT" : "GEMM") << endl;
                
                // 解决SystemC信号传播延迟：如果是复数数据且不为零，强制按FFT模式处理
                complex<T> W = w_i.read();
                bool has_valid_twiddle = (W.real != 0 || W.imag != 0);
                bool force_fft_mode = has_valid_twiddle; // 有非零Twiddle数据时强制FFT模式
                
                if (current_mode || force_fft_mode) {
                    // FFT模式：twiddle加载
                    w_fft_re_r = W.real;
                    w_fft_im_r = W.imag;
                    w_fft_valid = true;
                    cout << sc_time_stamp() << " " << this->name() << " FFT Twiddle加载: (" 
                         << W.real << "," << W.imag << ")" << (force_fft_mode ? " [强制FFT模式]" : "") << endl;
                } else {
                    // GEMM模式：权重加载 (仅使用实部)
                    w_gemm_r = complex<T>(W.real, 0);
                    w_gemm_valid = true;
                    cout << sc_time_stamp() << " " << this->name() << " GEMM Weight加载: " 
                         << W.real << endl;
                }
            }
        }
    //     wait(); // 等待下一个时钟上升沿
    // }
}

// ====== MAC计算和有效信号处理进程 ======

template<typename T>
void PE_DUAL<T>::mac_and_valid_proc() {
    // while (true) {
        // 规范reset逻辑
        if (rst_i.read() == false) {
            gemm_mac_r = T(0);
            gemm_x_r = T(0);
            gemm_mac_v_r = false;
            gemm_x_v_r = false;
            fft_y0_r = T(0);
            fft_y1_r = T(0);
            fft_y0_v_r = false;
            fft_y1_v_r = false;
            // 延时控制复位
            fft_state = IDLE;
            gemm_state = IDLE;
            fft_delay_counter = 0;
            gemm_delay_counter = 0;
        } else {
            // 延时控制状态机处理
            process_delay_state_machines();
            
            // 分别调用FFT和GEMM计算逻辑
            perform_fft();
            perform_gemm_computation();
        }
    //     wait(); // 等待下一个时钟上升沿
    // }
}

// ====== 延时控制状态机处理 ======
template<typename T>
void PE_DUAL<T>::process_delay_state_machines() {
    // FFT状态机处理
    if (fft_state == COMPUTING) {
        fft_delay_counter++;
        if (fft_delay_counter > FFT_OPERATION_CYCLES) {
            fft_state = READY;
            fft_delay_counter = 0;
            // 延时完成，输出结果
            fft_y0_r = fft_temp_y0;
            fft_y1_r = fft_temp_y1;
            fft_y0_v_r = true;
            fft_y1_v_r = true;
            cout << sc_time_stamp() << " " << this->name() << " FFT延时计算完成: Y0=(" << fft_y0_r.real << "," << fft_y0_r.imag 
                 << "), Y1=(" << fft_y1_r.real << "," << fft_y1_r.imag << ")" << endl;
        }
    } else if (fft_state == READY) {
        // 清除有效信号并回到空闲状态
        fft_y0_v_r = false;
        fft_y1_v_r = false;
        fft_state = IDLE;
    }
    
    // GEMM状态机处理
    if (gemm_state == COMPUTING) {
        gemm_delay_counter++;
        if (gemm_delay_counter > GEMM_OPERATION_CYCLES) {
            gemm_state = READY;
            gemm_delay_counter = 0;
            // 延时完成，输出结果
            gemm_mac_r = gemm_temp_mac;
            gemm_x_r = gemm_temp_x;
            gemm_mac_v_r = true;
            gemm_x_v_r = true;
            cout << sc_time_stamp() << " " << this->name() << " GEMM延时计算完成: MAC=" << gemm_mac_r.real 
                 << ", X=" << gemm_x_r.real << endl;
        }
    } else if (gemm_state == READY) {
        // 清除有效信号并回到空闲状态
        gemm_mac_v_r = false;
        gemm_x_v_r = false;
        gemm_state = IDLE;
    }
}

// ====== FFT计算逻辑实现 (带延时控制的版本) ======
template<typename T>
void PE_DUAL<T>::perform_fft() {
    bool x_valid = x_v_i.read();
    bool mac_valid = mac_v_i.read();
    
    // 临时调试：打印PE状态
    static int debug_pe_counter = 0;
    if (++debug_pe_counter % 100 == 0) {
        cout << "[PE_DEBUG] " << this->name() << " x_v=" << x_valid << " mac_v=" << mac_valid 
             << " w_fft_v=" << w_fft_valid << " mode=" << mode_r << " fft_state=" << fft_state << endl;
    }
    
    // 只有在IDLE状态才能启动新的计算
    if (x_valid && mac_valid && w_fft_valid && !mode_switching && (mode_r == 1) && (fft_state == IDLE)) {
        // 输入数据获取 (X0=mac_i, X1=x_i)
        complex<T> X0 = complex<T>(mac_i.read().real, mac_i.read().imag);
        complex<T> X1 = complex<T>(x_i.read().real, x_i.read().imag);
        
        // Twiddle处理
        bool conj_en = fft_conj_en_i.read();
        complex<T> W = conj_en ? 
            c_conj(complex<T>(w_fft_re_r, w_fft_im_r)) : 
            complex<T>(w_fft_re_r, w_fft_im_r);
        
        int shift = fft_shift_i.read();
        
        // 调试输出：打印使用的Twiddle因子
        // cout << sc_time_stamp() << " " << this->name() << " 使用Twiddle因子: "
        //      << "W=(" << W.real << "," << W.imag << ") shift=" << shift << endl;
        
        // 立即计算FFT结果并存储到临时寄存器
        complex<T> sum = c_add(X0, X1);                    // Y0 = X0 + X1
        complex<T> diff = c_sub(X0, X1);                   // temp = X0 - X1
        complex<T> Y1 = c_mul(diff, W);                    // Y1 = (X0 - X1) * W
        
        // 应用缩放
        if (shift > 0) {
            sum = c_scale(sum, shift);
            Y1 = c_scale(Y1, shift);
        }
        
        // // 调试输出：打印计算结果
        cout << sc_time_stamp() << " " << this->name() << " 计算结果: "
             << "Y0=(" << sum.real << "," << sum.imag << ") "
             << "Y1=(" << Y1.real << "," << Y1.imag << ")" << endl;
        
        // 存储到临时寄存器，等待延时
        fft_temp_y0 = complex<T>(sum.real, sum.imag);
        fft_temp_y1 = complex<T>(Y1.real, Y1.imag);
        
        // 启动延时状态机
        fft_state = COMPUTING;
        fft_delay_counter = 1; // 从第1个周期开始计数
        
        cout << sc_time_stamp() << " " << this->name() << " FFT计算启动，开始" << FFT_OPERATION_CYCLES << "周期延时" << endl;
    } else {
        // 打印详细的FFT启动条件检查
        static int condition_debug_counter = 0;
        if (++condition_debug_counter % 50 == 0 && (x_valid || mac_valid)) {  // 只在有输入数据时打印
            cout << sc_time_stamp() << " " << this->name() << " FFT启动条件检查: "
                 << "x_valid=" << x_valid 
                 << ", mac_valid=" << mac_valid
                 << ", w_fft_valid=" << w_fft_valid
                 << ", mode_switching=" << mode_switching
                 << ", mode_r=" << mode_r
                 << ", fft_state=" << (fft_state == IDLE ? "IDLE" : (fft_state == COMPUTING ? "COMPUTING" : "READY"))
                 << ", 综合条件=" << (x_valid && mac_valid && w_fft_valid && !mode_switching && (mode_r == 1) && (fft_state == IDLE))
                 << endl;
        }
    }
}

// ====== GEMM计算逻辑实现 (带延时控制的版本) ======
template<typename T>
void PE_DUAL<T>::perform_gemm_computation() {
    bool x_valid = x_v_i.read();
    bool mac_valid = mac_v_i.read();
    
    // 只有在IDLE状态才能启动新的计算
    if (x_valid && mac_valid && w_gemm_valid && !mode_switching && (mode_r == 0) && (gemm_state == IDLE)) {
        complex<T> x_in = x_i.read();
        complex<T> mac_in = mac_i.read(); 
        
        // 立即计算GEMM结果并存储到临时寄存器
        T mac_result = x_in.real * w_gemm_r.real + mac_in.real;
        T x_result = x_in.real;
        
        // 存储到临时寄存器，等待延时
        gemm_temp_mac = complex<T>(mac_result, 0);
        gemm_temp_x = complex<T>(x_result, 0);
        
        // 启动延时状态机
        gemm_state = COMPUTING;
        gemm_delay_counter = 1; // 从第1个周期开始计数
        
        cout << sc_time_stamp() << " " << this->name() << " GEMM计算启动，开始" << GEMM_OPERATION_CYCLES << "周期延时" << endl;
        
    } else if (mac_valid && w_gemm_valid && !mode_switching && (mode_r == 0) && (gemm_state == IDLE)) {
        complex<T> mac_in = mac_i.read();
        T mac_result = 0 * w_gemm_r.real + mac_in.real;
        T x_result = 0;
        
        gemm_temp_mac = complex<T>(mac_result, 0);
        gemm_temp_x = complex<T>(x_result, 0);
        
        gemm_state = COMPUTING;
        gemm_delay_counter = 1;
        
        cout << sc_time_stamp() << " " << this->name() << " GEMM计算启动(仅MAC)，开始" << GEMM_OPERATION_CYCLES << "周期延时" << endl;
        
    } else if (x_valid && w_gemm_valid && !mode_switching && (mode_r == 0) && (gemm_state == IDLE)) {
        complex<T> x_in = x_i.read();
        T mac_result = x_in.real * w_gemm_r.real + 0;
        T x_result = x_in.real;
        
        gemm_temp_mac = complex<T>(mac_result, 0);
        gemm_temp_x = complex<T>(x_result, 0);
        
        gemm_state = COMPUTING;
        gemm_delay_counter = 1;
        
        cout << sc_time_stamp() << " " << this->name() << " GEMM计算启动(仅X)，开始" << GEMM_OPERATION_CYCLES << "周期延时" << endl;
    }
}


// ====== 输出多路选择进程 ======

template<typename T>
void PE_DUAL<T>::output_mux_proc() {
    // while (true) {
        // 规范reset逻辑 - Active low reset: false means in reset
        if (rst_i.read() == false) {
            mac_o.write(complex<T>(0, 0));
            x_o.write(complex<T>(0, 0));
            mac_v_o.write(false);
            x_v_o.write(false);
        } else {
    
   bool current_bypass = stage_bypass_en.read();
   if (current_bypass) {
        // Bypass模式：零延迟直通 - 修复：立即传递输入信号
        complex<T> mac_val = mac_i.read();
        complex<T> x_val = x_i.read();
        bool mac_valid = mac_v_i.read();
        bool x_valid = x_v_i.read();
        
        mac_o.write(mac_val);      
        x_o.write(x_val);          
        mac_v_o.write(mac_valid);  
        x_v_o.write(x_valid);
        
    } else if (is_fft_ready()) {
        // FFT模式：选择FFT支路输出
        mac_o.write(fft_y0_r);      
        x_o.write(fft_y1_r);        
        mac_v_o.write(fft_y0_v_r);
        x_v_o.write(fft_y1_v_r);
        
        // cout << sc_time_stamp() << " " << this->name() << " FFT输出: Y0=(" << fft_y0_r.real << "," << fft_y0_r.imag 
        //      << "), Y1=(" << fft_y1_r.real << "," << fft_y1_r.imag << ")" << endl;
        
    } else if (is_gemm_ready()) {
        // GEMM模式：选择GEMM支路输出
        mac_o.write(gemm_mac_r);    
        x_o.write(gemm_x_r);        
        mac_v_o.write(gemm_mac_v_r);
        x_v_o.write(gemm_x_v_r);
        
        cout << sc_time_stamp() << " " << this->name() << " GEMM输出: MAC=" << gemm_mac_r.real 
             << ", X=" << gemm_x_r.real << endl;
        
        } else {
            // 未就绪状态：输出零
            mac_o.write(complex<T>(0, 0));
            x_o.write(complex<T>(0, 0));
            mac_v_o.write(false);
            x_v_o.write(false);
        }
        }
    //     wait(); // 等待下一个事件
    // }
}

// ====== 辅助方法实现 ======

template<typename T>
bool PE_DUAL<T>::is_fft_ready() const {
    bool result = (mode_r == 1) && (fft_y0_v_r || fft_y1_v_r) && !mode_switching;
    return result;
}

template<typename T>
bool PE_DUAL<T>::is_gemm_ready() const {
    return (mode_r == 0) && (gemm_mac_v_r || gemm_x_v_r) && !mode_switching;
}

// ====== 模板实例化 ======
template class PE_DUAL<float>;