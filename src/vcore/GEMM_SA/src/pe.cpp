/**
 * @file pe.cpp
 * @brief PE模块实现文件
 */

#ifndef PE_CPP
#define PE_CPP

#include "../include/pe.h"
#include <iostream>

// ====== 构造函数实现 ======
template<typename T>
PE<T>::PE(sc_module_name name) : sc_module(name) {
    SC_METHOD(weight_and_data_proc);  // 权重和数据处理
    sensitive << clk_i.pos();
    
    SC_METHOD(mac_and_valid_proc);    // MAC运算和有效信号处理
    sensitive << clk_i.pos();
    
    // 初始化内部状态
    w_r = 0;
    mac_r = 0;
    w_valid = false;
}

/**
 * @brief 权重和数据处理方法（优化1：合并方法减少调度开销）
 */
template<typename T>
void PE<T>::weight_and_data_proc() {
    bool rst = rst_i.read();  // 优化：只读取一次
    
    // 统一复位处理（优化3）
    if (!rst) {
        w_r = 0;
        w_valid = false;
        x_o.write(0);
        return;
    }
    
    // 权重处理
    if (wr_en_i.read()) {
        w_r = w_i.read();
        w_valid = true;
        //cout << sc_time_stamp() <<" [PE-"<< this->name() <<"] Weight loaded: w=" << dec << w_i.read() << endl;
    }
    
    // 数据输出处理（优化2：只在必要时更新）
    bool x_valid = x_v_i.read();
    T new_x_out = x_valid ? x_i.read() : T(0);
    if (x_o.read() != new_x_out) {
        x_o.write(new_x_out);
    }
    
}

/**
 * @brief MAC运算和有效信号处理方法（优化1：合并方法减少调度开销）
 */
template<typename T>
void PE<T>::mac_and_valid_proc() {
    bool rst = rst_i.read();  // 优化：只读取一次
    bool x_valid = x_v_i.read();
    bool mac_valid = mac_v_i.read();
    
    // 统一复位处理（优化3）
    if (!rst) {
        mac_r = 0;
        x_v_o.write(false);
        mac_v_o.write(false);
        mac_o.write(0);
        return;
    }
    
    // MAC计算路径优化（优化4：简化分支逻辑）
    T mac_input = mac_valid ? mac_i.read() : T(0);
    T mult_result = (x_valid && w_valid) ? x_i.read() * w_r : T(0);
    
    
    // 统一MAC计算：要么计算新值，要么传递输入值
    T old_mac = mac_r;
    if (x_valid && w_valid) {
        mac_r = mult_result + mac_input;  // 正常MAC运算
        // cout << sc_time_stamp() << " [PE-" << this->name() << "] MAC Compute: "
        //      << "(" << x_i.read() << " * " << w_r << " + " << mac_input << ") = " << mac_r << endl;
    } else if (mac_valid) {
        mac_r = mac_input;  // 纯传递
        //cout << sc_time_stamp() << " [PE-" << this->name() << "] MAC Pass: " << mac_input << endl;
    }
    // 其他情况保持当前值
    
    // 有效信号处理（优化2：只在必要时更新）
    if (x_v_o.read() != x_valid) {
        x_v_o.write(x_valid);
    }
    
    // 优化的MAC有效信号逻辑
    bool mac_output_valid = mac_valid || (w_valid && x_valid);
    if (mac_v_o.read() != mac_output_valid) {
        mac_v_o.write(mac_output_valid);
        // cout << sc_time_stamp() << " [PE-" << this->name() << "] MAC Valid Updated: " 
        //      << mac_v_o.read() << " -> " << mac_output_valid << endl;
    }
    
    // MAC输出始终更新（避免端口自引用）
    mac_o.write(mac_r);
    
    // // Debug: 输出最终状态
    // if (mac_output_valid) {
    //     cout << sc_time_stamp() << " [PE-" << this->name() << "] Final Output: "
    //          << "mac_o=" << mac_r << ", mac_v_o=" << mac_output_valid << endl;
    // }
}

#endif // PE_CPP