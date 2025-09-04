/**
 * @file in_buf_vec_fft.cpp
 * @brief FFT input buffer vector module implementation file
 */

#ifndef IN_BUF_VEC_FFT_CPP
#define IN_BUF_VEC_FFT_CPP

#include "../include/in_buf_vec_fft.h"
#include <iostream>

// ========== Constructor Implementation ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::IN_BUF_VEC_FFT(sc_module_name name) :
    sc_module(name),
    data_i_vec("data_i_vec", NUM_FIFOS),
    wr_en_i("wr_en_i", NUM_FIFOS),
    wr_ready_o_vec("wr_ready_o_vec", NUM_FIFOS),
    data_o_group0("data_o_group0", GROUP_SIZE_MAX),
    rd_valid_group0("rd_valid_group0", GROUP_SIZE_MAX),
    data_o_group1("data_o_group1", GROUP_SIZE_MAX),
    rd_valid_group1("rd_valid_group1", GROUP_SIZE_MAX),
    fifo_array("fifo_array", NUM_FIFOS),
    data_ready_vec("data_ready_vec", NUM_FIFOS),
    rd_start_group0("rd_start_group0", GROUP_SIZE_MAX),
    rd_start_group1("rd_start_group1", GROUP_SIZE_MAX),
    wr_en_prev("wr_en_prev", NUM_FIFOS)
{
    // ========== FIFO Array Connections ==========
    for (int i = 0; i < NUM_FIFOS; ++i) {
        // Basic control signal connections
        fifo_array[i].clk_i(clk_i);
        fifo_array[i].rst_i(rst_i);
        
        // Write port connections
        fifo_array[i].data_i(data_i_vec[i]);
        fifo_array[i].wr_start_i(wr_start_i);
        fifo_array[i].wr_en_i(wr_en_i[i]);
        fifo_array[i].wr_ready_o(wr_ready_o_vec[i]);
        
        // Data ready status connections
        fifo_array[i].data_ready_o(data_ready_vec[i]);
    }
    
    // ========== Group0 FIFO Read Port Connections ==========
    // Group0: FIFO[0-N] -> data_o_group0[0-N], rd_valid_group0[0-N]
    for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
        fifo_array[i].data_o(data_o_group0[i]);
        fifo_array[i].rd_start_i(rd_start_group0[i]);
        fifo_array[i].rd_valid_o(rd_valid_group0[i]);
    }
    
    // ========== Group1 FIFO Read Port Connections ==========
    // Group1: FIFO[N+1-2N-1] -> data_o_group1[0-N], rd_valid_group1[0-N]
    for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
        int fifo_idx = GROUP_SIZE_MAX + i;  // FIFO[N+1-2N-1]
        fifo_array[fifo_idx].data_o(data_o_group1[i]);
        fifo_array[fifo_idx].rd_start_i(rd_start_group1[i]);
        fifo_array[fifo_idx].rd_valid_o(rd_valid_group1[i]);
    }
    
    // ========== Process Registration ==========
    SC_THREAD(read_group_driver);
    sensitive << clk_i.pos();
    dont_initialize();
    
    SC_METHOD(group_status_monitor);
    sensitive << clk_i.pos();
    for (int i = 0; i < NUM_FIFOS; ++i) {
        sensitive << wr_en_i[i];
    }
    dont_initialize();
    
    // ========== Initial State ==========
    reset_internal_state();
}

// ========== Internal State Reset Method ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::reset_internal_state() {
    is_reading = false;
    rd_start_prev = false;
    group0_ready_count = 0;
    group1_ready_count = 0;
    groups_ready_flag = false;
    
    // Reset all wr_en_prev signals
    for (int i = 0; i < NUM_FIFOS; ++i) {
        wr_en_prev[i].write(false);
    }
}

// ========== Grouped Read Driver Process ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::read_group_driver() {
    if (rst_i.read() == 0) {
        reset_internal_state();
        stop_all_reads();
        std::cout << sc_time_stamp() << ": [" << this->name() 
                  << "] Reset: clear all read start signals" << std::endl;
    }
    
    while (true) {
        wait(); // Wait for clock rising edge
        
        bool rd_start_curr = rd_start_i.read();
        
        // ========== Detect rd_start rising edge ==========
        if (!rd_start_prev && rd_start_curr) {
            is_reading = true;
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] Detected rd_start rising edge, start grouped parallel read" << std::endl;
        }
        
        // ========== Detect rd_start falling edge ==========
        if (rd_start_prev && !rd_start_curr && is_reading) {
            is_reading = false;
            stop_all_reads();
            std::cout << sc_time_stamp() << ": [" << this->name() 
                      << "] Detected rd_start falling edge, stop grouped read" << std::endl;
        }
        
        // ========== Execute grouped parallel read ==========
        if (is_reading) {
            // // Check if both groups have sufficient data
            // bool group0_ready = groups_ready_flag;//check_group_ready(0);
            // bool group1_ready = groups_ready_flag;//check_group_ready(1);
            
            // if (group0_ready && group1_ready) {
                // Both groups ready: start synchronous parallel read
                start_group_read(0);
                start_group_read(1);
                
        //         std::cout << sc_time_stamp() << ": [" << this->name() 
        //                   << "] Both groups ready, start parallel read" << std::endl;
        //     } else {
        //         // Insufficient data: stop reading
        //         stop_all_reads();
        //         if (!group0_ready || !group1_ready) {
        //             is_reading = false;
        //             std::cout << sc_time_stamp() << ": [" << this->name() 
        //                       << "] Insufficient data: Group0=" << (group0_ready ? "Ready" : "NotReady")
        //                       << ", Group1=" << (group1_ready ? "Ready" : "NotReady") 
        //                       << ", stop reading" << std::endl;
        //         }
        //     }
         }
        
        rd_start_prev = rd_start_curr;
    }
}

// ========== Group Status Monitor Process ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::group_status_monitor() {
    if (rst_i.read() == 0) {
        groups_ready_o.write(false);
        groups_empty_o.write(true);
        // Reset previous wr_en states
        for (int i = 0; i < NUM_FIFOS; ++i) {
            wr_en_prev[i].write(false);
        }
        return;
    }
    
    // ========== Detect wr_en_i falling edge ==========
    bool write_completed = false;
    for (int i = 0; i < NUM_FIFOS; ++i) {
        bool curr_wr_en = wr_en_i[i].read();
        bool prev_wr_en = wr_en_prev[i].read();
        
        // Detect falling edge (from true to false)
        if (prev_wr_en && !curr_wr_en) {
            write_completed = true;
            // std::cout << sc_time_stamp() << ": [" << this->name() 
            //           << "] Detected wr_en_i[" << i << "] falling edge, write completed" << std::endl;
        }
        
        // Update previous state
        wr_en_prev[i].write(curr_wr_en);
    }
    
    // ========== Set groups_ready_flag on write completion ==========
    if (write_completed) {
        groups_ready_flag = true;
        std::cout << sc_time_stamp() << ": [" << this->name() 
                  << "] Write completion detected, setting groups_ready_o = true" << std::endl;
    }
    
    // ========== Reset groups_ready_flag when reading starts ==========
    if (rd_start_i.read() && groups_ready_flag) {
        groups_ready_flag = false;
        std::cout << sc_time_stamp() << ": [" << this->name() 
                  << "] Read started, clearing groups_ready_flag" << std::endl;
    }
    
    // ========== Count ready FIFOs for groups_empty_o ==========
    group0_ready_count = 0;
    group1_ready_count = 0;
    
    // Count Group0 ready number (FIFO[0-N])
    for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
        if (data_ready_vec[i].read()) {
            group0_ready_count++;
        }
    }
    
    // Count Group1 ready number (FIFO[N+1-2N-1])
    for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
        int fifo_idx = GROUP_SIZE_MAX + i;
        if (data_ready_vec[fifo_idx].read()) {
            group1_ready_count++;
        }
    }
    
    // ========== Output group status ==========
    bool both_groups_empty = (group0_ready_count == 0) && (group1_ready_count == 0);
    
    groups_ready_o.write(groups_ready_flag);
    groups_empty_o.write(both_groups_empty);
}

// ========== Check if specified group is ready ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
bool IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::check_group_ready(int group_idx) {
    if (group_idx == 0) {
        // Check Group0 (FIFO[0-N])
        for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
            if (!data_ready_vec[i].read()) {
                return false;
            }
        }
        return true;
    } else if (group_idx == 1) {
        // Check Group1 (FIFO[N+1-2N-1])
        for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
            int fifo_idx = GROUP_SIZE_MAX + i;
            if (!data_ready_vec[fifo_idx].read()) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// ========== Start specified group read ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::start_group_read(int group_idx) {
    if (group_idx == 0) {
        // Start Group0 all FIFO reads
        for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
            rd_start_group0[i].write(true);
        }
    } else if (group_idx == 1) {
        // Start Group1 all FIFO reads
        for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
            rd_start_group1[i].write(true);
        }
    }
}

// ========== Stop all read operations ==========
template<typename T, int NUM_PE, int FIFO_DEPTH>
void IN_BUF_VEC_FFT<T, NUM_PE, FIFO_DEPTH>::stop_all_reads() {
    // Stop Group0 reads
    for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
        rd_start_group0[i].write(false);
    }
    
    // Stop Group1 reads
    for (int i = 0; i < GROUP_SIZE_MAX; ++i) {
        rd_start_group1[i].write(false);
    }
}

#endif // IN_BUF_VEC_FFT_CPP