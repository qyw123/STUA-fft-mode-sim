/**
 * @file FIFO.cpp
 * @brief FIFO模块实现文件
 */

#ifndef FIFO_CPP
#define FIFO_CPP

#include "../include/FIFO.h"
#include <iostream>

// ========== 构造函数实现 ==========
template<typename T, int BUFFER_DEPTH>
FIFO<T, BUFFER_DEPTH>::FIFO(sc_module_name name) : sc_module(name) {
    data_count = 0;
    is_empty = true;
    is_full = false;
    module_id = this->name();

    SC_METHOD(write_input_logic);
    sensitive << clk_i.pos();
    dont_initialize();

    SC_METHOD(read_output_logic);
    sensitive << clk_i.pos();
    dont_initialize();

    SC_METHOD(data_ready_output_logic);
    sensitive << clk_i.pos();
    dont_initialize();

    // std::cout << module_id << ": Initialized with depth = " << BUFFER_DEPTH << std::endl;
}

// ========== 写入逻辑实现 ==========
template<typename T, int BUFFER_DEPTH>
void FIFO<T, BUFFER_DEPTH>::write_input_logic() {
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

            // std::cout << sc_time_stamp() << ": [" << module_id << "] Write data=" << val
            //           << ", count=" << data_count << std::endl;

            if (data_count == BUFFER_DEPTH) {
                std::cout << sc_time_stamp() << ": [" << module_id << "] Buffer is FULL\n";
            }
        } else {
            wr_ready_o.write(!is_full);
        }
    }
}

// ========== 读取逻辑实现 ==========
template<typename T, int BUFFER_DEPTH>
void FIFO<T, BUFFER_DEPTH>::read_output_logic() {
    if (rst_i.read() == 0) {
        data_o.write(T(-1));
        rd_valid_o.write(false);
    } else {
        if (rd_start_i.read() == 1 && !is_empty) {
            T val = buffer.front();
            buffer.pop();
            data_count--;
            is_empty = (data_count == 0);
            is_full = false;

            data_o.write(val);
            rd_valid_o.write(true);

            // std::cout << sc_time_stamp() << ": [" << module_id << "] Read data=" << val
            //           << ", count=" << data_count << std::endl;
        } else {
            rd_valid_o.write(false);
        }
    }
}

// ========== 数据计数输出逻辑实现 ==========
template<typename T, int BUFFER_DEPTH>
void FIFO<T, BUFFER_DEPTH>::data_ready_output_logic() {
    if (rst_i.read() == 0) {
        data_ready_o.write(0);
    } else {
        data_ready_o.write(data_count!=0);
    }
}

#endif // FIFO_CPP