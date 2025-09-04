#ifndef OUT_BUF_ROW_ARRAY_H
#define OUT_BUF_ROW_ARRAY_H

#include "systemc.h"
#include "FIFO.h"
#include <vector>

/**
 * @brief 输出缓冲行阵列模块（支持向量写入读出）
 */
template<typename T, int ROWS = 4, int DEPTH = 8>
SC_MODULE(OUT_BUF_ROW_ARRAY) {
    sc_in_clk clk_i;
    sc_in<bool> rst_i;

    sc_vector<sc_in<T>> data_i_vec;         // 输入数据向量
    sc_in<bool> wr_start_i;
    sc_vector<sc_in<bool>> wr_en_i_vec;     // 🚀 向量化写使能信号 - 基于PE输出有效性
    sc_vector<sc_in<bool>> rd_start_i_vec;  // 🚀 向量化读启动信号 - 每个FIFO独立控制
    sc_vector<sc_out<bool>> wr_ready_o_vec; // 🔧 修正拼写：写就绪输出信号

    sc_vector<sc_out<T>> data_o_vec;        // 输出数据向量
    sc_vector<sc_out<bool>> rd_valid_vec;   // 有效信号向量
    sc_vector<sc_signal<bool>> data_ready_vec; // 存在数据信号

    sc_vector<FIFO<T, DEPTH>> buf_array;

    // ========== 构造函数声明 ==========
    SC_CTOR(OUT_BUF_ROW_ARRAY);
};

// 包含模板实现
#include "../src/out_buf_vec.cpp"

#endif // OUT_BUF_ROW_ARRAY_H
