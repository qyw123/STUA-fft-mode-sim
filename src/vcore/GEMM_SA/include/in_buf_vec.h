#ifndef IN_BUF_ROW_ARRAY_H
#define IN_BUF_ROW_ARRAY_H

#include "systemc.h"
#include "FIFO.h"
#include <vector>

/**
 * @brief 输入缓冲行阵列模块（支持向量写入与平行四边形读取）
 */
template<typename T, int ROWS = 4, int DEPTH = 8>
SC_MODULE(IN_BUF_ROW_ARRAY) {
    sc_in_clk clk_i;
    sc_in<bool> rst_i;

    sc_vector<sc_in<T>> data_i_vec;         // 输入数据向量
    sc_in<bool> wr_start_i;
    sc_in<bool> wr_en_i;
    sc_in<bool> rd_start_i;                 //输出启动信号，如果为高，则逐拍启动各个buf,形成"平行四边形"数据流
    
    sc_vector<sc_out<bool>> wr_read_o_vec;
    sc_vector<sc_out<T>> data_o_vec;        // 输出数据向量
    sc_vector<sc_out<bool>> rd_valid_vec;   // 有效信号向量


    sc_vector<sc_signal<bool>> data_ready_vec; // 存在数据信号
    sc_vector<FIFO<T, DEPTH>> buf_array;
    sc_signal<bool> rd_start_chain[ROWS];   // 每行专属的读启动信号


    bool is_reading = false;     // 是否正在执行平行四边形读出
    int staggered_counter = 0;   // 平行四边形阶段计数器
    bool rd_start_prev = false;  // 记录rd_start_i的前一状态，用于检测上升沿

    // ========== 构造函数声明 ==========
    SC_CTOR(IN_BUF_ROW_ARRAY);

    // ========== 方法声明 ==========
    void read_staggered_driver_reset();
    void read_staggered_driver();
};

// 包含模板实现
#include "../src/in_buf_vec.cpp"

#endif // IN_BUF_ROW_ARRAY_H