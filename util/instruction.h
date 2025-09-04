#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include "const.h"
#include "tools.h"
#include <systemc>
#include <tlm>
#include <tlm_utils/multi_passthrough_initiator_socket.h>

namespace ins{
    // 读取DMI数据的通用函数    
    template<typename T>
    void read_from_dmi(uint64_t addr, std::vector<T>& values, 
                      const tlm::tlm_dmi& dmi, unsigned int data_num,
                      const std::string& module_name = "DMI_Utils") {
        const unsigned int bytes_per_block = DDR_DATA_WIDTH;
        const unsigned int elements_per_block = bytes_per_block / sizeof(T);

        if (addr + data_num * sizeof(T) - 1 <= dmi.get_end_address()) {
            values.resize(data_num);
            unsigned char* dmi_addr = dmi.get_dmi_ptr() + (addr - dmi.get_start_address());
            unsigned int total_blocks = (data_num + elements_per_block - 1) / elements_per_block;

            for (unsigned int block = 0; block < total_blocks; ++block) {
                unsigned int block_start = block * elements_per_block;
                unsigned int block_end = std::min(block_start + elements_per_block, data_num);
                
                for (unsigned int i = block_start; i < block_end; ++i) {
                    memcpy(&values[i], dmi_addr + i * sizeof(T), sizeof(T));
                }
                
                wait(SYSTEM_CLOCK);
            }
            wait(dmi.get_read_latency());
        } else {
            SC_REPORT_ERROR(module_name.c_str(), "DMI read failed: Address out of range");
        }
    }

    // 写入DMI数据的通用函数
    template<typename T>
    void write_to_dmi(uint64_t start_addr, uint64_t& end_addr, 
                     const std::vector<T>& values, const tlm::tlm_dmi& dmi, 
                     unsigned int data_num, const std::string& module_name = "DMI_Utils") {
        const unsigned int bytes_per_block = DDR_DATA_WIDTH;
        const unsigned int elements_per_block = bytes_per_block / sizeof(T);

        if (data_num != values.size()) {
            SC_REPORT_ERROR(module_name.c_str(), "Mismatch between data_num and values size");
            return;
        }

        end_addr = start_addr + data_num * sizeof(T) - 1;

        if (end_addr <= dmi.get_end_address() && start_addr >= dmi.get_start_address()) {
            unsigned char* dmi_addr = dmi.get_dmi_ptr() + (start_addr - dmi.get_start_address());
            unsigned int total_blocks = (data_num + elements_per_block - 1) / elements_per_block;

            for (unsigned int block = 0; block < total_blocks; ++block) {
                unsigned int block_start = block * elements_per_block;
                unsigned int block_end = std::min(block_start + elements_per_block, data_num);

                for (unsigned int i = block_start; i < block_end; ++i) {
                    memcpy(dmi_addr + i * sizeof(T), &values[i], sizeof(T));
                }
                
                wait(SYSTEM_CLOCK);
            }
            wait(dmi.get_write_latency());
        } else {
            SC_REPORT_ERROR(module_name.c_str(), "DMI write failed: Address out of range");
        }
    }

    //AM 16路并行列访问
    template <typename T>
    void am2vpu_16_trans(vector<T>& data_vector, tlm::tlm_dmi& am_dmi, uint64_t source_addr, uint64_t array_byte_index, uint64_t array_element_num, uint64_t array_num) {
        //判断source_addr是否在AM的DMI范围内
        if (source_addr < am_dmi.get_start_address() || source_addr > am_dmi.get_end_address()) {
            SC_REPORT_ERROR("AM2VPU16Trans", "Source address out of range");
            return;
        }
        //检查读取内容是否越界

        //从source_addr开始取数据,取array_num帧，帧大小array_element_num个T类型的数据
        //每个array之间隔array_byte_index(上一帧结束地址与下一帧开始地址)
        //单周期最多读取128B的数据
        //将数据写入data_vector中
        
        // 计算总共需要读取的元素数量
        uint64_t total_elements = array_element_num * array_num;
        // 调整data_vector大小以容纳所有数据
        data_vector.resize(total_elements);
        
        // 获取DMI指针的起始位置
        unsigned char* dmi_ptr = am_dmi.get_dmi_ptr();
        
        // 每个元素的字节大小
        const uint64_t element_size = sizeof(T);
        // 计算单周期能读取的最大元素数量 (256B / sizeof(T))
        const uint64_t max_elements_per_cycle = 128 / element_size;
        
        // 遍历每一帧
        for (uint64_t frame = 0; frame < array_num; ++frame) {
            // 计算当前帧的起始地址
            uint64_t frame_start_addr = source_addr + frame * array_byte_index;
            // 计算当前帧在DMI内存中的偏移量
            uint64_t frame_offset = frame_start_addr - am_dmi.get_start_address();
            // 计算当前帧在data_vector中的起始索引
            uint64_t vec_index = frame * array_element_num;
            
            // 当前帧还需要读取的元素数量
            uint64_t remaining_elements = array_element_num;
            // 当前帧中已处理的元素数量
            uint64_t processed_elements = 0;
            
            // 分批读取当前帧的数据，每批最多读取max_elements_per_cycle个元素
            while (remaining_elements > 0) {
                // 当前批次要读取的元素数量
                uint64_t batch_elements = std::min(remaining_elements, max_elements_per_cycle);
                
                // 读取数据到data_vector
                for (uint64_t i = 0; i < batch_elements; ++i) {
                    // 计算源地址在DMI内存中的位置
                    uint64_t src_offset = frame_offset + (processed_elements + i) * element_size;
                    // 复制数据到data_vector
                    memcpy(&data_vector[vec_index + processed_elements + i], dmi_ptr + src_offset, element_size);
                }
                
                // 更新已处理和剩余元素数量
                processed_elements += batch_elements;
                remaining_elements -= batch_elements;
                

            }
        }
        //这里考虑的是，帧数很多，但是单帧数据量不大（resnet18）,不会占满带宽，因此可以一次性读取所有数据，采用这种方式来模拟延时
        wait(SYSTEM_CLOCK);
        //cout << "将AM中矩阵数据载入到VPU中" << endl;
    }

    //SG传输启动增强版(带帧结构)
    template <typename T>
    void sg_trans_ext_inst(tlm_utils::multi_passthrough_initiator_socket<T,512>& socket, const tlm::tlm_dmi& sm_dmi,
        uint64_t destination_addr, uint64_t destination_array_index, uint32_t destination_elem_byte_num, uint32_t destination_array_num) {
        //设置传输模式
        tlm::tlm_generic_payload trans;
        // 创建数据缓冲区：1字节模式 + 8字节目标地址 + 8字节目标帧索引 + 4字节目标单元字节数 + 4字节目标帧数
        unsigned char* data = new unsigned char[25];
        //设置传输模式
        data[0] = 0x02;  // 0x02表示SG传输模式
        
        // 设置目标地址（第1-8字节）
        for(int i = 0; i < 8; i++) {
            data[1 + i] = (destination_addr >> (i * 8)) & 0xFF;
        }
        
        // 设置目标帧索引（第9-16字节）
        for(int i = 0; i < 8; i++) {
            data[9 + i] = (destination_array_index >> (i * 8)) & 0xFF;
        }
        
        // 设置目标单元字节数（第17-20字节）
        for(int i = 0; i < 4; i++) {
            data[17 + i] = (destination_elem_byte_num >> (i * 8)) & 0xFF;
        }
        
        // 设置目标帧数（第21-24字节）
        for(int i = 0; i < 4; i++) {
            data[21 + i] = (destination_array_num >> (i * 8)) & 0xFF;
        }
        
        //设置TLM传输属性
        trans.set_data_ptr(data);
        trans.set_address(DMA_BASE_ADDR);
        trans.set_data_length(25);  // 总长度25字节
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        //发送传输事务
        sc_time delay = SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        //cout << "SG传输指令已发送,等待完成" << endl;
        //等待事务完成
        wait_for_OK_response(trans);
        //cout << "SG传输指令已完成 " << endl;
        //清理数据指针
        delete[] data;
    }

    //SG传输配置参数写入内存
    template <typename T>
    void sg_trans_param_write_inst(tlm_utils::multi_passthrough_initiator_socket<T,512>& socket, const tlm::tlm_dmi& sm_dmi,
        uint64_t source_addr, vector<uint32_t> Byte_index_list, vector<uint32_t> length_list, uint32_t data_num) {
        // 为SG传输配置参数写入内存,默认写入SM中，从0x010020f00开始,不超过2KB大小
        uint64_t start_addr = 0x010020f00;
        // 检查DMI访问权限
        if (!sm_dmi.is_write_allowed()) {
            SC_REPORT_ERROR("Sg_trans_inst", "DMI write not allowed");
            return;
        }
        // 检查地址范围
        if (start_addr < sm_dmi.get_start_address() || 
            start_addr + (data_num+2) *8  > sm_dmi.get_end_address()) {
            //第一个双字（8B）位置存储配置状态标志，第二个双字位置存储源地址，其余位置配置sg参数
            SC_REPORT_ERROR("Sg_trans_inst", "DMI address out of range");
            return;
        }
        //sg传输控制参数字uint64_t类型，低8位为分散读取的次数即data_num，第9位为sg参数有效位，1代表有效，0代表无效
        const uint64_t SG_PARAM_VALID_BIT = 0x10000;  // 第17位为1，表示有效
        const uint64_t DATA_NUM_MASK = 0xFFFF;        // 低16位掩码
        uint64_t sg_param = (data_num & DATA_NUM_MASK) | SG_PARAM_VALID_BIT;
        // 获取DMI指针并计算偏移
        unsigned char* sm_dmi_ptr = sm_dmi.get_dmi_ptr();
        uint64_t offset = start_addr - sm_dmi.get_start_address();

        // 写入内存
        uint64_t* target = reinterpret_cast<uint64_t*>(sm_dmi_ptr + offset);
        target[0] = sg_param;
        target[1] = source_addr;

        for (unsigned int i = 0; i < data_num; i++) {
            target[i+2] = ((static_cast<uint64_t>(Byte_index_list[i]) << 32) | length_list[i]);
        }

        // cout << "SG配置参数通过DMI写入完成:写入" << dec << (data_num+2)*8 << "字节数据到地址0x" 
        //     << hex << start_addr << dec << endl;
    }


    template <typename T>
    void dma_matrix_transpose_trans(tlm_utils::multi_passthrough_initiator_socket<T,512>& socket, uint64_t source_addr, uint64_t destination_addr, 
        uint32_t row_num, uint32_t column_num, uint32_t element_byte_num, bool is_complex = false) {
        // 设置事务类型为DMA矩阵转置传输
        tlm::tlm_generic_payload trans;
        unsigned char* data = new unsigned char[30];

        // 设置传输模式（第0字节）
        data[0] = 0x01;  // 假设0x01表示矩阵转置传输模式
        
        // 设置源地址（第1-8字节）
        for(int i = 0; i < 8; i++) {
            data[1 + i] = (source_addr >> (i * 8)) & 0xFF;
        }
        // 设置目标地址（第9-16字节）
        for(int i = 0; i < 8; i++) {
            data[9 + i] = (destination_addr >> (i * 8)) & 0xFF;
        }
        // 设置行数（第17-20字节）
        for(int i = 0; i < 4; i++) {
            data[17 + i] = (row_num >> (i * 8)) & 0xFF;
        }
        // 设置列数（第21-24字节）
        for(int i = 0; i < 4; i++) {
            data[21 + i] = (column_num >> (i * 8)) & 0xFF;
        }
        //设置元素字节数（第25-28字节）
        for(int i = 0; i < 4; i++) {
            data[25 + i] = (element_byte_num >> (i * 8)) & 0xFF;
        }
        //设置是否为复数（第29字节）
        data[29] = is_complex;
        // 设置TLM传输属性
        trans.set_data_ptr(data);
        trans.set_address(DMA_BASE_ADDR);
        trans.set_data_length(30);  // 总长度30字节
        trans.set_command(tlm::TLM_WRITE_COMMAND);  // 设置为写命令
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);  // 初始化响应状态

        // 发送传输事务
        sc_time delay = SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        //等待事务完成
        wait_for_OK_response(trans);
        // 清理数据指针
        delete[] data;
        cout << "DMA矩阵转置传输完成,从地址0x" << hex << source_addr << "到地址0x" << destination_addr << "传输了" << dec << row_num << "行" << column_num << "列" << "单数据字节数："<< element_byte_num  << endl;
    }

    //DMA点对点传输
    template <typename T>
    void dma_p2p_trans(tlm_utils::multi_passthrough_initiator_socket<T,512>& socket,
        uint64_t source_addr, uint64_t source_array_index, uint32_t source_elem_byte_num, uint32_t source_array_num,
        uint64_t destination_addr, uint64_t destination_array_index, uint32_t destination_elem_byte_num, uint32_t destination_array_num) {
        
        // 设置事务类型为DMA点对点传输
        tlm::tlm_generic_payload trans;
        
        // 计算数据缓冲区大小：1字节传输模式 + 所有参数字节
        // 1(模式) + 8(源地址) + 8(源帧索引) + 4(源单元字节数) + 4(源帧数)
        // + 8(目标地址) + 8(目标帧索引) + 4(目标单元字节数) + 4(目标帧数)
        // = 总共49字节
        unsigned char* data = new unsigned char[49];
        
        // 设置传输模式（第0字节）
        data[0] = 0x03;  // 0x03表示点对点传输模式
        
        // 源地址参数
        // 设置源起始地址（第1-8字节）
        for(int i = 0; i < 8; i++) {
            data[1 + i] = (source_addr >> (i * 8)) & 0xFF;
        }
        
        // 设置源帧索引（第9-16字节）
        for(int i = 0; i < 8; i++) {
            data[9 + i] = (source_array_index >> (i * 8)) & 0xFF;
        }
        
        // 设置源单元字节数（第17-20字节）
        for(int i = 0; i < 4; i++) {
            data[17 + i] = (source_elem_byte_num >> (i * 8)) & 0xFF;
        }
        
        // 设置源帧数（第21-24字节）
        for(int i = 0; i < 4; i++) {
            data[21 + i] = (source_array_num >> (i * 8)) & 0xFF;
        }
        
        // 目标地址参数
        // 设置目标起始地址（第25-32字节）
        for(int i = 0; i < 8; i++) {
            data[25 + i] = (destination_addr >> (i * 8)) & 0xFF;
        }
        
        // 设置目标帧索引（第33-40字节）
        for(int i = 0; i < 8; i++) {
            data[33 + i] = (destination_array_index >> (i * 8)) & 0xFF;
        }
        
        // 设置目标单元字节数（第41-44字节）
        for(int i = 0; i < 4; i++) {
            data[41 + i] = (destination_elem_byte_num >> (i * 8)) & 0xFF;
        }
        
        // 设置目标帧数（第45-48字节）
        for(int i = 0; i < 4; i++) {
            data[45 + i] = (destination_array_num >> (i * 8)) & 0xFF;
        }
        
        // 设置TLM传输属性
        trans.set_data_ptr(data);
        trans.set_address(DMA_BASE_ADDR);
        trans.set_data_length(49);  // 总长度49字节
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        
        // 发送传输事务
        sc_time delay = SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        
        // 等待事务完成
        wait_for_OK_response(trans);
        
        // 清理数据指针
        delete[] data;
        
        cout << "DMA点对点传输完成，从地址0x" << hex << source_addr << "到地址0x" << destination_addr 
             << "，源帧数:" << dec << source_array_num << "，目标帧数:" << destination_array_num << endl;
    }

   

}
#endif
