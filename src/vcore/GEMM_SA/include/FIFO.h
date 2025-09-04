#ifndef FIFO_H
#define FIFO_H

#include "systemc.h"
#include <iostream>
#include <iomanip>
#include <queue>

/**
 * @brief 输入缓冲区模块类（自动化版）
 */
template<typename T=float, int BUFFER_DEPTH = 16>
SC_MODULE(FIFO) {
    // ========== 端口 ==========
    sc_in_clk clk_i;
    sc_in<bool> rst_i;
    sc_in<T> data_i;
    sc_in<bool> wr_start_i;
    sc_in<bool> wr_en_i;
    sc_out<bool> wr_ready_o;
    sc_out<T> data_o;
    sc_in<bool> rd_start_i;
    sc_out<bool> rd_valid_o;
    sc_out<bool> data_ready_o;  // 实时输出缓冲区数据量

    // ========== 内部状态 ==========
    std::queue<T> buffer;
    int data_count;
    bool is_empty;
    bool is_full;
    std::string module_id;

    // ========== 构造函数 ==========
    SC_CTOR(FIFO);

    // ========== 方法声明 ==========
    void write_input_logic();
    void read_output_logic();
    void data_ready_output_logic();
};

// 包含模板实现
#include "../src/FIFO.cpp"

#endif // FIFO_H
