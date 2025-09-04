#ifndef FIFO_H
#define FIFO_H

#include "systemc.h"
#include <iostream>
#include <iomanip>
#include <queue>

/**
 * @brief fifo模块类
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
    
    // 修复E：添加valid信号持续控制
    bool rd_valid_hold;  // 保持rd_valid_o信号的状态
    bool rd_start_prev;  // 记录rd_start_i的前一状态

    // ========== 构造 ==========
    SC_CTOR(FIFO) {
        data_count = 0;
        is_empty = true;
        is_full = false;
        module_id = name();
        rd_valid_hold = false;  // 修复E：初始化valid保持状态
        rd_start_prev = false;  // 修复E：初始化rd_start前一状态

        SC_METHOD(write_input_logic);
        sensitive << clk_i.pos();
        dont_initialize();

        SC_METHOD(read_output_logic);
        sensitive << clk_i.pos();
        dont_initialize();

        SC_METHOD(data_ready_output_logic);
        sensitive << clk_i.pos();
        dont_initialize();

        //std::cout << module_id << ": Initialized with depth = " << BUFFER_DEPTH << std::endl;
    }

    // ========== 写入逻辑 ==========
    void write_input_logic() {
        if (rst_i.read() == 0) {
            data_count = 0;
            is_empty = true;
            is_full = false;
            std::queue<T> empty;
            std::swap(buffer, empty);
            wr_ready_o.write(true);
        } else {
            if (data_count < BUFFER_DEPTH && wr_en_i.read()) {
                T val = data_i.read();
                buffer.push(val);
                data_count++;
                is_empty = false;
                is_full = (data_count == BUFFER_DEPTH);
                wr_ready_o.write(!is_full);

                std::cout << sc_time_stamp() << ": [" << module_id << "] Write data=" << val
                          << ", count=" << data_count << std::endl;

                if (data_count == BUFFER_DEPTH) {
                    std::cout << sc_time_stamp() << ": [" << module_id << "] Buffer is FULL\n";
                }
            } else {
                wr_ready_o.write(!is_full);
            }
        }
    }

    // ========== 读取逻辑 ==========
    void read_output_logic() {
        if (rst_i.read() == 0) {
            data_o.write(T(-1));
            rd_valid_o.write(false);
            rd_valid_hold = false;  // 修复E：重置valid保持状态
            rd_start_prev = false;
        } else {
            bool rd_start_curr = rd_start_i.read();
            
            // 修复I：在rd_start高电平期间持续读取（支持多点FFT）
            if (rd_start_curr && !is_empty) {
                // rd_start高电平且有数据：持续读取
                bool should_read = false;
                
                if (!rd_start_prev && rd_start_curr) {
                    // rd_start上升沿：第一次读取
                    should_read = true;
                } else if (rd_start_prev && rd_start_curr && rd_valid_hold) {
                    // rd_start保持高且之前已读取：检查是否需要读取下一个数据
                    // 简单策略：每个时钟周期读取一个，直到空或rd_start下降
                    should_read = true;
                }
                
                if (should_read) {
                    T val = buffer.front();
                    buffer.pop();
                    data_count--;
                    is_empty = (data_count == 0);
                    is_full = false;

                    data_o.write(val);
                    rd_valid_hold = true;  // 保持valid信号

                    std::cout << sc_time_stamp() << ": [" << module_id << "] Read data=" << val
                              << ", count=" << data_count << std::endl;
                }
            }
            
            // 修复E：检测rd_start下降沿时停止保持valid
            if (rd_start_prev && !rd_start_curr) {
                rd_valid_hold = false;  // rd_start下降沿：停止保持valid
            }
            
            // 修复E：输出valid信号：在rd_start为高期间保持valid
            rd_valid_o.write(rd_valid_hold && rd_start_curr);
            
            rd_start_prev = rd_start_curr;  // 更新前一状态
        }
    }

    // ========== 数据计数输出逻辑 ==========
    void data_ready_output_logic() {
        if (rst_i.read() == 0) {
            data_ready_o.write(0);
        } else {
            data_ready_o.write(data_count!=0);
        }
    }
};

#endif // FIFO_H
