/**
 * @file pe.h
 * @brief 脉动阵列处理单元(Processing Element)的SystemC声明
 * 
 * 这个模块定义了一个基本的PE单元，用于构建脉动阵列。
 * PE单元能够执行以下功能：
 * 1. 存储权重数据
 * 2. 接收输入数据并向右传递
 * 3. 执行乘加运算(MAC)并向下传递结果
 * 4. 处理有效信号以控制数据流
 */

#ifndef PE_H
#define PE_H

#include "systemc.h"

template<typename T=float>
SC_MODULE(PE) {
    // ====== 输入端口 ======
    sc_in_clk clk_i;         // 时钟输入，控制PE的同步操作
    sc_in<bool> rst_i;       // 复位信号，用于初始化PE的状态
    
    sc_in<T> x_i{"x_i"};   // 数据输入端口，从左侧PE接收数据
    sc_in<bool> x_v_i{"x_v_i"};       // 数据有效信号，表示输入数据是否有效
    
    sc_in<T> mac_i{"mac_i"};// MAC结果输入，从上方PE接收
    sc_in<bool> mac_v_i{"mac_v_i"};     // MAC有效信号，表示输入的MAC结果是否有效
    
    sc_in<T> w_i{"w_i"};   // 权重输入
    sc_in<bool> wr_en_i{"wr_en_i"};     // 权重写使能，控制权重的加载时机
    
    // ====== 输出端口 ======
    sc_out<T> x_o{"x_o"};  // 数据输出，将输入数据传递给右侧PE
    sc_out<bool> x_v_o{"x_v_o"};      // 数据有效输出，指示输出数据是否有效
    
    sc_out<T> mac_o{"mac_o"};// MAC结果输出，传递给下方PE
    sc_out<bool> mac_v_o{"mac_v_o"};     // MAC有效输出，指示MAC结果是否有效
    
    // ====== 内部寄存器 ======
    T w_r;           // 权重寄存器，存储当前PE的权重值
    T mac_r;         // MAC结果寄存器，避免端口自引用
    bool w_valid;    // 权重有效标志，指示权重是否已正确加载
    
    // ====== 构造函数声明 ======
    SC_CTOR(PE);
    
    // ====== 方法声明 ======
    void weight_and_data_proc();
    void mac_and_valid_proc();
};

// 包含模板实现
#include "../src/pe.cpp"

#endif // PE_H