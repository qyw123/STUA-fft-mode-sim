/**
 * @file out_buf_vec.cpp
 * @brief OUT_BUF_ROW_ARRAY模块实现文件
 */

#ifndef OUT_BUF_VEC_CPP
#define OUT_BUF_VEC_CPP

#include "../include/out_buf_vec.h"

// ========== 构造函数实现 ==========
template<typename T, int ROWS, int DEPTH>
OUT_BUF_ROW_ARRAY<T, ROWS, DEPTH>::OUT_BUF_ROW_ARRAY(sc_module_name name) :
    sc_module(name),
    data_i_vec("data_i_vec", ROWS),
    wr_en_i_vec("wr_en_i_vec", ROWS),
    rd_start_i_vec("rd_start_i_vec", ROWS),
    wr_ready_o_vec("wr_ready_o_vec", ROWS),
    data_o_vec("data_o_vec", ROWS),
    rd_valid_vec("rd_valid_vec", ROWS),
    buf_array("buf_array", ROWS),
    data_ready_vec("data_ready_vec", ROWS)
{
    for (int i = 0; i < ROWS; ++i) {
        // 基础信号连接
        buf_array[i].clk_i(clk_i);
        buf_array[i].rst_i(rst_i);
        
        // 🚀 写入接口 - 向量化控制
        buf_array[i].data_i(data_i_vec[i]);
        buf_array[i].wr_start_i(wr_start_i);
        buf_array[i].wr_en_i(wr_en_i_vec[i]);       // 每个FIFO独立写使能
        buf_array[i].wr_ready_o(wr_ready_o_vec[i]); // 写就绪输出

        // 🚀 读出接口 - 向量化控制
        buf_array[i].data_o(data_o_vec[i]);
        buf_array[i].rd_start_i(rd_start_i_vec[i]); // 每个FIFO独立读启动
        buf_array[i].rd_valid_o(rd_valid_vec[i]);
        buf_array[i].data_ready_o(data_ready_vec[i]);
    }
}

#endif // OUT_BUF_VEC_CPP