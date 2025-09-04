/**
 * @file in_buf_vec.cpp
 * @brief IN_BUF_ROW_ARRAY模块实现文件
 */

#ifndef IN_BUF_VEC_CPP
#define IN_BUF_VEC_CPP

#include "../include/in_buf_vec.h"
#include <iostream>

// ========== 构造函数实现 ==========
template<typename T, int ROWS, int DEPTH>
IN_BUF_ROW_ARRAY<T, ROWS, DEPTH>::IN_BUF_ROW_ARRAY(sc_module_name name) :
    sc_module(name),
    data_i_vec("data_i_vec", ROWS),
    data_o_vec("data_o_vec", ROWS),
    wr_read_o_vec("wr_read_o_vec",ROWS),
    rd_valid_vec("rd_valid_vec", ROWS),
    buf_array("buf_array", ROWS),
    data_ready_vec("data_ready_vec", ROWS)
{
    for (int i = 0; i < ROWS; ++i) {

        buf_array[i].clk_i(clk_i);
        buf_array[i].rst_i(rst_i);
        buf_array[i].data_i(data_i_vec[i]);
        buf_array[i].wr_start_i(wr_start_i);
        buf_array[i].wr_en_i(wr_en_i);
        buf_array[i].wr_ready_o(wr_read_o_vec[i]); // 可留空或扩展为向量输出

        buf_array[i].data_o(data_o_vec[i]);
        buf_array[i].rd_start_i(rd_start_chain[i]);
        buf_array[i].rd_valid_o(rd_valid_vec[i]);
        buf_array[i].data_ready_o(data_ready_vec[i]);
    }

    SC_THREAD(read_staggered_driver);
    sensitive << clk_i.pos();
    dont_initialize();

    SC_THREAD(read_staggered_driver_reset);
    sensitive << clk_i.pos() << rst_i;
}

// ========== 复位驱动方法实现 ==========
template<typename T, int ROWS, int DEPTH>
void IN_BUF_ROW_ARRAY<T, ROWS, DEPTH>::read_staggered_driver_reset(){
    // 状态变量
    is_reading = false;     // 是否正在执行平行四边形读出
    staggered_counter = 0;   // 平行四边形阶段计数器
    rd_start_prev = false;  // 记录rd_start_i的前一状态，用于检测上升沿
}

// ========== 平行四边形读出驱动方法实现 ==========
template<typename T, int ROWS, int DEPTH>
void IN_BUF_ROW_ARRAY<T, ROWS, DEPTH>::read_staggered_driver() {
    if(rst_i.read() == 0){
        // 初始化所有读启动信号为0
        cout << sc_time_stamp() << " 复位清空启动信号" << endl;
        for (int i = 0; i < ROWS; ++i) {
            rd_start_chain[i] = 0;
        }
    }
    while (true) {
        wait(); // 等待时钟上升沿或rd_start_i变化
        bool rd_start_curr = rd_start_i.read();
        
        // 检测rd_start_i的上升沿，启动平行四边形序列
        if (!rd_start_prev && rd_start_curr) {
            is_reading = true;
            staggered_counter = 0;
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] 检测到rd_start上升沿，开始平行四边形读出序列" << std::endl;
        }
        
        // 检测rd_start_i的下降沿，停止平行四边形序列
        if (rd_start_prev && !rd_start_curr && is_reading) {
            is_reading = false;
            for (int i = 0; i < ROWS; ++i) {
                rd_start_chain[i] = 0;
            }
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] 检测到rd_start下降沿，停止平行四边形读出序列" << std::endl;
        }
        
        if (is_reading) {
            // 首先检查是否还有数据需要读取
            bool has_any_data = false;
            for (int i = 0; i < ROWS; ++i) {
                if (data_ready_vec[i].read()) {
                    has_any_data = true;
                    break;
                }
            }
            
            if (!has_any_data) {
                // 如果所有缓冲区都没有数据，停止读取
                is_reading = false;
                for (int i = 0; i < ROWS; ++i) {
                    rd_start_chain[i] = 0;
                }
                std::cout << sc_time_stamp() << ": [" << this->name() 
                          << "] 所有缓冲区无数据，停止平行四边形读出" << std::endl;
            } else {
                // 清除之前的启动信号
                for (int i = 0; i < ROWS; ++i) {
                    rd_start_chain[i] = 0;
                }
                    
                if (staggered_counter < ROWS) {
                    // 启动指定范围内有数据的缓冲区
                    for (int i = 0; i <= staggered_counter; ++i) {
                        if (data_ready_vec[i].read()) {
                            rd_start_chain[i] = 1;
                            // cout << sc_time_stamp() << ": [" << this->name() 
                            //      << "] 启动FIFO[" << i << "]读取, counter=" << staggered_counter << endl;
                        }
                    }
                } 
                else{//全部启动
                    for (int i = 0; i < ROWS; ++i) {
                        if (data_ready_vec[i].read()) {
                            rd_start_chain[i] = 1;
                            // cout << sc_time_stamp() << ": [" << this->name() 
                            //      << "] 全部启动FIFO[" << i << "]读取" << endl;
                        }
                    }
                }
                staggered_counter++;
            }
        }
        
        rd_start_prev = rd_start_curr;
    }
}

#endif // IN_BUF_VEC_CPP