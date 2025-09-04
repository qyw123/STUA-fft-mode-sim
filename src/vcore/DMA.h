#ifndef DMA_H
#define DMA_H

#include "../../util/const.h"
#include "../../util/tools.h"

//简单连续传输参数结构体
struct Simple_Continuous_Trans_Param{
    uint8_t trans_mode;           //传输模式,0:简单连续传输，
    uint64_t Source_addr;         //源开始地址
    uint64_t Destination_addr;    //目的开始地址
    uint32_t Transfer_length;     //传输长度,单位字节，最大为1GB(0x4000 0000)，最小为1字节
};
//点对点传输参数结构体
struct Point2Point_Trans_Param{
    uint8_t trans_mode;           //传输模式,3:点对点传输
    uint64_t Source_addr;         //源开始地址
    uint64_t Source_array_index;          //源帧索引
    uint32_t Source_elem_Byte_num;      //源单元计数
    uint32_t Source_array_num;          //源帧计数
    uint64_t Destination_addr;          //目的开始地址
    uint64_t Destination_array_index;          //目的帧索引
    uint32_t Destination_elem_Byte_num;      //目的单元计数
    uint32_t Destination_array_num;          //目的帧计数

};
//矩阵转置传输参数结构体
struct Matrix_Transpose_Trans_Param{
    uint8_t trans_mode;           //传输模式,1:矩阵转置传输，
    uint64_t Source_addr;         //源开始地址
    uint64_t Destination_addr;    //目的开始地址
    uint32_t Row_num;             //行数
    uint32_t Column_num;          //列数
    uint32_t element_byte_num;    //元素字节数
    bool is_complex;                //是否为复数，默认不为复数
};
//SG传输参数结构体
struct SG_Trans_Param{
    uint8_t trans_mode;           //传输模式,2:SG传输
    uint64_t Destination_addr;    //目的开始地址
    uint64_t Destination_array_index;          //目的帧索引
    uint32_t Destination_elem_Byte_num;      //目的单元计数
    uint32_t Destination_array_num;          //目的帧计数
};

union Trans_Param{
    Simple_Continuous_Trans_Param sctp;
    Matrix_Transpose_Trans_Param mttp;
    SG_Trans_Param sgtp;
    Point2Point_Trans_Param p2pt;
};

template<typename T>
class DMA : public sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<DMA, 512> spu2dma_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<DMA, 512> dma2sm_init_socket;
    tlm_utils::multi_passthrough_initiator_socket<DMA, 512> dma2am_init_socket;
    tlm_utils::multi_passthrough_initiator_socket<DMA, 512> dma2vcore_init_socket;
    SC_CTOR(DMA) : spu2dma_target_socket("spu2dma_target_socket"),
                dma2sm_init_socket("dma2sm_init_socket"), 
                dma2am_init_socket("dma2am_init_socket"),
                dma2vcore_init_socket("dma2vcore_init_socket")
    {
        spu2dma_target_socket.register_b_transport(this, &DMA::b_transport);
        spu2dma_target_socket.register_get_direct_mem_ptr(this, &DMA::get_direct_mem_ptr);

        dma2sm_init_socket.register_invalidate_direct_mem_ptr(this, &DMA::invalidate_direct_mem_ptr);
        dma2am_init_socket.register_invalidate_direct_mem_ptr(this, &DMA::invalidate_direct_mem_ptr);
        dma2vcore_init_socket.register_invalidate_direct_mem_ptr(this, &DMA::invalidate_direct_mem_ptr);
        SC_THREAD(init_process);
        SC_THREAD(simple_continuous_trans_process);
        SC_THREAD(matrix_transpose_transfer);
        SC_THREAD(sg_transfer_process);
        SC_THREAD(point2point_transfer);
        //三种传输模式实现
        // //1、点对点传输
        // SC_THREAD(point_to_point_transfer);
        // //2、SG传输
        // SC_THREAD(sg_transfer);
        // //3、矩阵转置传输
        // SC_THREAD(matrix_transpose_transfer);
    }

    //初始化过程
    void init_process(){
        while(true){
            wait(init_process_start_event);
            switch(trans_mode_flag){
                case 0x00:
                    simple_continuous_trans_event.notify();
                    break;
                case 0x01:
                    matrix_transpose_transfer_event.notify();
                    break;
                case 0x02:
                    sg_transfer_event.notify();
                    break;
                case 0x03:
                    point2point_transfer_event.notify();
                    break;
                default:
                    SC_REPORT_ERROR("DMA", "Unsupported transfer mode");
                    sc_stop();
                    break;
            }
        }
    }   
    //简单连续传输过程
    void simple_continuous_trans_process(){
        while(true) {
            wait(simple_continuous_trans_event);
            Simple_Continuous_Trans_Param sctp_param = dma_param.sctp;
            
            vector<unsigned char> buffer;

            //启动读数据流程，从source_addr开始，读取Transfer_length个字节,使用dmi的方法
            dma_read_trans.set_address(sctp_param.Source_addr);
            dma_read_trans.set_read();
            
            // 获取源地址的DMI访问权限
            if (!get_dmi_access(dma_read_trans, dmi_data_read, sctp_param.Source_addr, "source")) {
                dma_state = ERROR;
                return;
            }

            // 检查DMI读取权限
            if (!dmi_data_read.is_read_allowed()) {
                SC_REPORT_ERROR("DMA", "DMI read not allowed");
                dma_state = ERROR;
                return;
            }

            // 读取数据到缓冲区
            buffer.resize(sctp_param.Transfer_length);
            unsigned char* src_ptr = dmi_data_read.get_dmi_ptr() + 
                                   (sctp_param.Source_addr - dmi_data_read.get_start_address());
            memcpy(buffer.data(), src_ptr, sctp_param.Transfer_length);
            dma_delay = SYSTEM_CLOCK * calculate_clock_cycles(sctp_param.Transfer_length, SM_AM_DATA_WIDTH);
            wait(dma_delay);
            //启动写数据流程，向destination_addr开始，写入Transfer_length个字节，使用dmi的方法
            dma_write_trans.set_address(sctp_param.Destination_addr);
            dma_write_trans.set_write();

            // 获取目标地址的DMI访问权限
            if (!get_dmi_access(dma_write_trans, dmi_data_write, sctp_param.Destination_addr, "destination")) {
                dma_state = ERROR;
                return;
            }
            // 检查DMI写入权限
            if (!dmi_data_write.is_write_allowed()) {
                SC_REPORT_ERROR("DMA", "DMI write not allowed");
                dma_state = ERROR;
                return;
            }

            // 写入数据
            unsigned char* dst_ptr = dmi_data_write.get_dmi_ptr() + 
                                   (sctp_param.Destination_addr - dmi_data_write.get_start_address());
            memcpy(dst_ptr, buffer.data(), sctp_param.Transfer_length);
            dma_delay = SYSTEM_CLOCK * calculate_clock_cycles(sctp_param.Transfer_length, SM_AM_DATA_WIDTH);
            wait(dma_delay);

            //通知传输事务完成，dma_state设置为IDLE
            dma_payload->set_response_status(tlm::TLM_OK_RESPONSE);
            dma_state = IDLE;
            simple_continuous_trans_done_event.notify();
            dma_delay = sc_time(0, SC_NS);
        }
    }
    //新建点对点传输模式
    void point2point_transfer(){
        while(true) {
            wait(point2point_transfer_event);
            Point2Point_Trans_Param p2pt_param = dma_param.p2pt;
            
            // 获取源和目标的参数
            uint64_t source_addr = p2pt_param.Source_addr;
            uint64_t source_array_index = p2pt_param.Source_array_index;
            uint32_t source_elem_byte_num = p2pt_param.Source_elem_Byte_num;
            uint32_t source_array_num = p2pt_param.Source_array_num;
            
            uint64_t destination_addr = p2pt_param.Destination_addr;
            uint64_t destination_array_index = p2pt_param.Destination_array_index;
            uint32_t destination_elem_byte_num = p2pt_param.Destination_elem_Byte_num;
            uint32_t destination_array_num = p2pt_param.Destination_array_num;
            
            // 获取源地址的DMI访问权限
            dma_read_trans.set_address(source_addr);
            dma_read_trans.set_read();
            if (!get_dmi_access(dma_read_trans, dmi_data_read, source_addr, "source")) {
                dma_state = ERROR;
                return;
            }
            
            // 获取目标地址的DMI访问权限
            dma_write_trans.set_address(destination_addr);
            dma_write_trans.set_write();
            if (!get_dmi_access(dma_write_trans, dmi_data_write, destination_addr, "destination")) {
                dma_state = ERROR;
                return;
            }
            
            // 计算单周期最大传输字节数
            const uint32_t max_bytes_per_cycle = 64;//512bits
            
            // 创建中间缓冲区，用于存储从源读取的所有数据
            vector<unsigned char> buffer;
            uint64_t total_source_bytes = source_elem_byte_num * source_array_num;
            buffer.resize(total_source_bytes);
            
            // 从源地址按帧读取数据
            unsigned char* src_dmi_ptr = dmi_data_read.get_dmi_ptr();
            unsigned char* dst_dmi_ptr = dmi_data_write.get_dmi_ptr();
            
            // 处理每一帧源数据
            for (uint32_t src_frame = 0; src_frame < source_array_num; ++src_frame) {
                // 计算当前源帧起始地址
                uint64_t src_frame_addr = source_addr + src_frame * source_array_index;
                uint64_t src_frame_offset = src_frame_addr - dmi_data_read.get_start_address();
                
                // 计算缓冲区中当前帧的起始位置
                uint64_t buffer_offset = src_frame * source_elem_byte_num;
                
                // 当前帧还剩需要传输的字节数
                uint32_t remaining_bytes = source_elem_byte_num;
                // 当前帧中已处理的字节数
                uint32_t processed_bytes = 0;
                
                // 分批读取当前帧数据，每批最多读取max_bytes_per_cycle字节
                while (remaining_bytes > 0) {
                    // 当前批次要读取的字节数
                    uint32_t batch_bytes = std::min(remaining_bytes, max_bytes_per_cycle);
                    
                    // 计算源数据指针
                    unsigned char* src_ptr = src_dmi_ptr + src_frame_offset + processed_bytes;
                    
                    // 复制数据到buffer
                    memcpy(buffer.data() + buffer_offset + processed_bytes, src_ptr, batch_bytes);
                    
                    // 更新已处理和剩余字节数
                    processed_bytes += batch_bytes;
                    remaining_bytes -= batch_bytes;
                    
                    // 每批次读取后等待一个时钟周期
                    //wait(SYSTEM_CLOCK);
                }
            }
            
            // 将数据从buffer写入到目标地址，按目标帧结构组织
            for (uint32_t dst_frame = 0; dst_frame < destination_array_num; ++dst_frame) {
                // 计算目标帧地址
                uint64_t dst_frame_addr = destination_addr + dst_frame * destination_array_index;
                uint64_t dst_frame_offset = dst_frame_addr - dmi_data_write.get_start_address();
                
                // 确定要写入的数据量(不超过源数据总量且不超过目标帧大小)
                uint32_t bytes_to_write = std::min(
                    destination_elem_byte_num,  // 目标帧大小
                    static_cast<uint32_t>(total_source_bytes - dst_frame * destination_elem_byte_num)  // 剩余源数据量
                );
                
                if (bytes_to_write <= 0) break; // 没有更多数据可写
                
                // 当前帧还剩需要传输的字节数
                uint32_t remaining_bytes = bytes_to_write;
                // 当前帧中已处理的字节数
                uint32_t processed_bytes = 0;
                // 缓冲区偏移量
                uint64_t buffer_read_offset = dst_frame * destination_elem_byte_num;
                
                if (buffer_read_offset >= total_source_bytes) break; // 防止缓冲区越界
                
                // 分批写入当前帧数据，每批最多写入max_bytes_per_cycle字节
                while (remaining_bytes > 0) {
                    // 当前批次要写入的字节数
                    uint32_t batch_bytes = std::min(remaining_bytes, max_bytes_per_cycle);
                    
                    // 计算目标数据指针
                    unsigned char* dst_ptr = dst_dmi_ptr + dst_frame_offset + processed_bytes;
                    
                    // 复制数据到目标位置
                    memcpy(dst_ptr, buffer.data() + buffer_read_offset + processed_bytes, batch_bytes);
                    
                    // 更新已处理和剩余字节数
                    processed_bytes += batch_bytes;
                    remaining_bytes -= batch_bytes;
                    
                    // 每批次写入后等待一个时钟周期
                    //wait(SYSTEM_CLOCK);
                }
            }
            
            // 计算总传输数据量
            uint64_t total_bytes = std::min(
                static_cast<uint64_t>(source_elem_byte_num) * source_array_num,
                static_cast<uint64_t>(destination_elem_byte_num) * destination_array_num
            );
            dma_delay = SYSTEM_CLOCK * calculate_clock_cycles(total_bytes, SM_AM_DATA_WIDTH);
            wait(dma_delay);
            
            // 通知传输事务完成
            dma_payload->set_response_status(tlm::TLM_OK_RESPONSE);
            dma_state = IDLE;
            point2point_transfer_done_event.notify();
            dma_delay = sc_time(0, SC_NS);
            
            // cout << "点对点传输完成: 源(" << source_array_num << "帧, 每帧" << dec << source_elem_byte_num 
            //      << "字节) -> 目标(" << destination_array_num << "帧, 每帧" << destination_elem_byte_num << endl;
        }
    }
    //矩阵转置传输过程,完成了分块矩阵转置传输
    void matrix_transpose_transfer(){
        while(true){
            wait(matrix_transpose_transfer_event);
            Matrix_Transpose_Trans_Param mttp_param = dma_param.mttp;
            bool is_complex = mttp_param.is_complex;

            // 获取源地址的DMI访问权限
            dma_read_trans.set_address(mttp_param.Source_addr);
            dma_read_trans.set_read();
            if (!get_dmi_access(dma_read_trans, dmi_data_read, mttp_param.Source_addr, "source")) {
                dma_state = ERROR;
                return;
            }
            // 获取目标地址的DMI访问权限
            dma_write_trans.set_address(mttp_param.Destination_addr);
            dma_write_trans.set_write();      
            if (!get_dmi_access(dma_write_trans, dmi_data_write, mttp_param.Destination_addr, "destination")) {
                dma_state = ERROR;
                return;
            }
            //计算基础块矩阵的行数和列数（元素数）,基础矩阵块是方阵
            uint32_t basic_row_num = 64/mttp_param.element_byte_num;
            uint32_t basic_col_num = basic_row_num;
            // cout << "basic_row_num: " << basic_row_num << endl;
            // cout << "basic_col_num: " << basic_col_num << endl;
            //此处是分块逻辑
            //首先计算行能分成多少块(块行个数)
            uint32_t row_num_block = (mttp_param.Row_num + basic_row_num - 1)/basic_row_num;
            //其次计算列能分成多少块(块列个数)
            uint32_t col_num_block = (mttp_param.Column_num + basic_col_num - 1)/basic_col_num;
            // cout << "row_num_block: " << row_num_block << endl;
            // cout << "col_num_block: " << col_num_block << endl;
            //先行后列，逐个遍历矩阵块
            for(int r = 0; r < row_num_block; r++){
                for(int c = 0; c < col_num_block; c++){
                    //计算本块的起始地址和目标块起始地址
                    uint64_t current_block_start_addr = mttp_param.Source_addr + r * mttp_param.Column_num * basic_row_num * mttp_param.element_byte_num + c * basic_col_num * mttp_param.element_byte_num;
                    uint64_t current_block_target_addr = mttp_param.Destination_addr + c * mttp_param.Row_num * basic_col_num * mttp_param.element_byte_num + r * basic_row_num * mttp_param.element_byte_num;
                    //计算此块真实的行数和列数  
                    uint32_t current_block_row_num = (mttp_param.Row_num - r * basic_row_num) > basic_row_num ? basic_row_num : (mttp_param.Row_num - r * basic_row_num);
                    uint32_t current_block_col_num = (mttp_param.Column_num - c * basic_col_num) > basic_col_num ? basic_col_num : (mttp_param.Column_num - c * basic_col_num);
                    // cout << "current_block_row_num: " << current_block_row_num << endl;
                    // cout << "current_block_col_num: " << current_block_col_num << endl;
                    if (is_complex) {
                        // 复数类型处理
                        vector<complex<T>> buffer_mt_before(current_block_row_num * current_block_col_num);
                        vector<complex<T>> buffer_mt_after(current_block_row_num * current_block_col_num);
                        complex<T>* src_ptr;
                        complex<T>* dst_ptr;
                        
                        // 初始化二维寄存器阵列
                        vector<vector<complex<T>>> basic_martix_array(current_block_row_num, vector<complex<T>>(current_block_col_num));
                        
                        // 读取数据
                        for (int i = 0; i < current_block_row_num; i++) {
                            src_ptr = reinterpret_cast<complex<T>*>(dmi_data_read.get_dmi_ptr() + 
                                (current_block_start_addr - dmi_data_read.get_start_address()) + i * mttp_param.Column_num * mttp_param.element_byte_num);
                            memcpy(buffer_mt_before.data() + i * current_block_col_num, src_ptr, current_block_col_num * mttp_param.element_byte_num);
                        }
                        
                        // 写入寄存器阵列
                        for(int i = 0; i < current_block_row_num; i++){
                            for(int j = 0; j < current_block_col_num; j++){
                                basic_martix_array[i][j] = buffer_mt_before[i*current_block_col_num+j];
                            }
                            wait(SYSTEM_CLOCK);
                        }
                        
                        // 转置读出
                        for (int i = 0; i < current_block_col_num; i++) {
                            for (int j = 0; j < current_block_row_num; j++) {
                                buffer_mt_after[i * current_block_row_num + j] = basic_martix_array[j][i];
                            }
                        }
                        
                        // 写回目标
                        for(int i = 0; i < current_block_col_num; i++){
                            dst_ptr = reinterpret_cast<complex<T>*>(dmi_data_write.get_dmi_ptr() + 
                                (current_block_target_addr-dmi_data_write.get_start_address())+i*mttp_param.Row_num*mttp_param.element_byte_num);
                            memcpy(dst_ptr, buffer_mt_after.data()+i*current_block_row_num, current_block_row_num*mttp_param.element_byte_num);
                        }
                    } else {
                        // 原始普通类型处理
                        vector<T> buffer_mt_before(current_block_row_num * current_block_col_num);
                        vector<T> buffer_mt_after(current_block_row_num * current_block_col_num);
                        T* src_ptr;
                        T* dst_ptr;
                        
                        // 初始化二维寄存器阵列
                        vector<vector<T>> basic_martix_array(current_block_row_num, vector<T>(current_block_col_num, 0));
                        
                        // 读取数据
                        for (int i = 0; i < current_block_row_num; i++) {
                            src_ptr = reinterpret_cast<T*>(dmi_data_read.get_dmi_ptr() + 
                                (current_block_start_addr - dmi_data_read.get_start_address()) + i * mttp_param.Column_num * mttp_param.element_byte_num);
                            memcpy(buffer_mt_before.data() + i * current_block_col_num, src_ptr, current_block_col_num * mttp_param.element_byte_num);
                        }
                        
                        // 写入寄存器阵列
                        for(int i = 0; i < current_block_row_num; i++){
                            for(int j = 0; j < current_block_col_num; j++){
                                basic_martix_array[i][j] = buffer_mt_before[i*current_block_col_num+j];
                            }
                            wait(SYSTEM_CLOCK);
                        }
                        
                        // 转置读出
                        for (int i = 0; i < current_block_col_num; i++) {
                            for (int j = 0; j < current_block_row_num; j++) {
                                buffer_mt_after[i * current_block_row_num + j] = basic_martix_array[j][i];
                            }
                        }
                        
                        // 写回目标
                        for(int i = 0; i < current_block_col_num; i++){
                            dst_ptr = reinterpret_cast<T*>(dmi_data_write.get_dmi_ptr() + 
                                (current_block_target_addr-dmi_data_write.get_start_address())+i*mttp_param.Row_num*mttp_param.element_byte_num);
                            memcpy(dst_ptr, buffer_mt_after.data()+i*current_block_row_num, current_block_row_num*mttp_param.element_byte_num);
                        }
                    }
                }
            }
            //通知传输事务完成，dma_state设置为IDLE
            dma_payload->set_response_status(tlm::TLM_OK_RESPONSE);
            dma_state = IDLE;
            matrix_transpose_transfer_done_event.notify();
            dma_delay = sc_time(0, SC_NS);
        }
    }
    
    //SG传输过程，完成分散读取集合写入
    void sg_transfer_process() {
        while(true) {
            wait(sg_transfer_event);
            SG_Trans_Param sgtp_param = dma_param.sgtp;
            
            // 从SM共享内存中读取SG传输配置参数
            uint64_t sg_config_addr = 0x010020f00; // SG配置参数在SM中的地址
            
            // 获取SM的DMI访问权限
            dma_read_trans.set_address(sg_config_addr);
            dma_read_trans.set_read();
            if (!get_dmi_access(dma_read_trans, dmi_data_read, sg_config_addr, "SM config")) {
                dma_state = ERROR;
                return;
            }
            
            // 读取SG参数
            unsigned char* sm_ptr = dmi_data_read.get_dmi_ptr() + 
                                 (sg_config_addr - dmi_data_read.get_start_address());
            uint64_t* sg_config = reinterpret_cast<uint64_t*>(sm_ptr);
            
            // 检查SG参数是否有效
            uint64_t sg_param = sg_config[0];
            bool sg_valid = (sg_param & 0x10000) != 0; // 第17位为有效位
            if (!sg_valid) {
                SC_REPORT_ERROR("DMA", "SG parameters not valid");
                dma_state = ERROR;
                return;
            }
            
            // 获取数据块数量
            // cout << "DMA: sg_param: "  << sg_param << endl;
            uint32_t data_num = sg_param & 0xFFFF; // 低16位为数据块数量
            // cout << "DMA: data_num: " << data_num << endl;
            
            // 获取源地址
            uint64_t source_base_addr = sg_config[1];
            // cout << "DMA: source_base_addr: "<< hex << source_base_addr << dec << endl;
            // 目标地址
            uint64_t destination_addr = sgtp_param.Destination_addr;
            
            // 获取分散数据块的索引和长度
            vector<uint32_t> byte_index_list(data_num);
            vector<uint32_t> length_list(data_num);
            
            for (unsigned int i = 0; i < data_num; i++) {
                uint64_t sg_entry = sg_config[i+2];
                byte_index_list[i] = static_cast<uint32_t>(sg_entry >> 32);
                length_list[i] = static_cast<uint32_t>(sg_entry & 0xFFFFFFFF);
            }
            
            // 获取目标地址的DMI访问权限
            dma_write_trans.set_address(destination_addr);
            dma_write_trans.set_write();
            if (!get_dmi_access(dma_write_trans, dmi_data_write, destination_addr, "destination")) {
                dma_state = ERROR;
                return;
            }
            
            // 分散读取
            vector<unsigned char> buffer;
            uint64_t total_bytes = 0;
            
            // 计算总数据量
            for (unsigned int i = 0; i < data_num; i++) {
                total_bytes += length_list[i];
            }
            
            // 调整缓冲区大小
            buffer.resize(total_bytes);
            uint64_t buffer_offset = 0;
            
            // 分散读取数据
            for (unsigned int i = 0; i < data_num; i++) {
                uint64_t src_addr = source_base_addr + byte_index_list[i];
                uint32_t length = length_list[i];
                
                // 获取源地址的DMI访问权限
                dma_read_trans.set_address(src_addr);
                dma_read_trans.set_read();
                if (!get_dmi_access(dma_read_trans, dmi_data_read, src_addr, "source")) {
                    dma_state = ERROR;
                    return;
                }
                
                // 读取数据到缓冲区
                unsigned char* src_ptr = dmi_data_read.get_dmi_ptr() + 
                                      (src_addr - dmi_data_read.get_start_address());
                memcpy(buffer.data() + buffer_offset, src_ptr, length);
                buffer_offset += length;
                
                // 模拟读取延迟
                dma_delay = SYSTEM_CLOCK * calculate_clock_cycles(length, SM_AM_DATA_WIDTH);
                //wait(dma_delay);
            }
            
            // 按照与点对点传输相同的模式数据帧模式写入数据
            unsigned char* dst_dmi_ptr = dmi_data_write.get_dmi_ptr();
            
            // 获取目标参数
            // uint64_t destination_addr = sgtp_param.Destination_addr;
            uint64_t destination_array_index = sgtp_param.Destination_array_index;
            uint32_t destination_elem_byte_num = sgtp_param.Destination_elem_Byte_num;
            uint32_t destination_array_num = sgtp_param.Destination_array_num;
            
            // 计算单周期最大传输字节数
            const uint32_t max_bytes_per_cycle = 64; // 512bits
            // cout << "DMA:sg_tranfer:准备将数据写入目标地址" << endl;
            // cout << "DMA:sg_tranfer:destination_array_num: " << destination_array_num << endl;
            // cout << "DMA:sg_tranfer:destination_array_index: " << destination_array_index << endl;
            // cout << "DMA:sg_tranfer:destination_elem_byte_num: " << destination_elem_byte_num << endl;
            // cout << "DMA:sg_tranfer:total_bytes: " << total_bytes << endl;
            // 将数据从buffer写入到目标地址，按目标帧结构组织
            for (uint32_t dst_frame = 0; dst_frame < destination_array_num; ++dst_frame) {
                // 计算目标帧地址
                uint64_t dst_frame_addr = destination_addr + dst_frame * destination_array_index;
                uint64_t dst_frame_offset = dst_frame_addr - dmi_data_write.get_start_address();
                
                // 确定要写入的数据量(不超过源数据总量且不超过目标帧大小)
                uint32_t bytes_to_write = std::min(
                    destination_elem_byte_num, // 目标帧大小
                    static_cast<uint32_t>(total_bytes - dst_frame * destination_elem_byte_num) // 剩余源数据量
                );
                
                if (bytes_to_write <= 0) break; // 没有更多数据可写
                
                // 当前帧还剩需要传输的字节数
                uint32_t remaining_bytes = bytes_to_write;
                // 当前帧中已处理的字节数
                uint32_t processed_bytes = 0;
                // 缓冲区偏移量
                uint64_t buffer_read_offset = dst_frame * destination_elem_byte_num;
                
                if (buffer_read_offset >= total_bytes) break; // 防止缓冲区越界
                
                // 分批写入当前帧数据，每批最多写入max_bytes_per_cycle字节
                while (remaining_bytes > 0) {
                    // 当前批次要写入的字节数
                    uint32_t batch_bytes = std::min(remaining_bytes, max_bytes_per_cycle);
                    
                    // 计算目标数据指针
                    unsigned char* dst_ptr = dst_dmi_ptr + dst_frame_offset + processed_bytes;
                    
                    // 复制数据到目标位置
                    memcpy(dst_ptr, buffer.data() + buffer_read_offset + processed_bytes, batch_bytes);
                    
                    // 更新已处理和剩余字节数
                    processed_bytes += batch_bytes;
                    remaining_bytes -= batch_bytes;
                    
                    // 每批次写入后等待一个时钟周期
                    //wait(SYSTEM_CLOCK);
                }
            }
            
            //cout << "DMA:sg_tranfer:数据写入目标地址完成" << endl;
            // 模拟写入延迟
            dma_delay = SYSTEM_CLOCK * calculate_clock_cycles(total_bytes, SM_AM_DATA_WIDTH);
            wait(dma_delay);
            
            // 通知传输事务完成
            dma_payload->set_response_status(tlm::TLM_OK_RESPONSE);
            dma_state = IDLE;
            sg_transfer_done_event.notify();
            dma_delay = sc_time(0, SC_NS);
            
            // cout << "DMA : SG传输完成: 从" << data_num << "个分散块读取总计" << total_bytes 
            //      << "字节数据，写入到地址0x" << hex << destination_addr << dec << endl;
        }
    }
    //阻塞传输方法，逻辑控制器
    virtual void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay )
    {
        unsigned char* data = trans.get_data_ptr();
        dma_payload = &trans;
        if(dma_state == IDLE){
            // 传输模式，获取第1个字节
            trans_mode_flag = data[0];
            switch(trans_mode_flag){
                case 0x00:
                    dma_param.sctp.trans_mode = trans_mode_flag;
                    // 源地址，获取第2-9个字节（8字节地址）
                    dma_param.sctp.Source_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.sctp.Source_addr |= ((uint64_t)data[1 + i] << (i * 8));
                    }
                    // 目的地址，获取第10-17个字节（8字节地址）
                    dma_param.sctp.Destination_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.sctp.Destination_addr |= ((uint64_t)data[9 + i] << (i * 8));
                    }
                    // 传输长度，获取第18-21个字节（4字节长度）
                    dma_param.sctp.Transfer_length = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.sctp.Transfer_length |= ((uint32_t)data[17 + i] << (i * 8));
                    }
                    
                    break;
                case 0x01:
                    dma_param.mttp.trans_mode = trans_mode_flag;
                    // 源地址，获取第2-9个字节（8字节地址）
                    dma_param.mttp.Source_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.mttp.Source_addr |= ((uint64_t)data[1 + i] << (i * 8));
                    }
                    // 目的地址，获取第10-17个字节（8字节地址）
                    dma_param.mttp.Destination_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.mttp.Destination_addr |= ((uint64_t)data[9 + i] << (i * 8));
                    }
                    // 行数，获取第18-21个字节（4字节行数）
                    dma_param.mttp.Row_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.mttp.Row_num |= ((uint32_t)data[17 + i] << (i * 8));
                    }
                    // 列数，获取第22-25个字节（4字节列数）
                    dma_param.mttp.Column_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.mttp.Column_num |= ((uint32_t)data[21 + i] << (i * 8));
                    }
                    // 元素字节数，获取第26-29个字节（4字节元素字节数）
                    dma_param.mttp.element_byte_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.mttp.element_byte_num |= ((uint32_t)data[25 + i] << (i * 8));
                    }
                    // 是否为复数，获取第30个字节（1字节，0表示实数，1表示复数）
                    dma_param.mttp.is_complex = data[29];
                    break;
                case 0x02:
                    dma_param.sgtp.trans_mode = trans_mode_flag;
                    // 目的地址，获取第2-9个字节（8字节地址）
                    dma_param.sgtp.Destination_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.sgtp.Destination_addr |= ((uint64_t)data[1 + i] << (i * 8));
                    }
                    // 目标帧索引，获取第10-17个字节（8字节索引）
                    dma_param.sgtp.Destination_array_index = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.sgtp.Destination_array_index |= ((uint64_t)data[9 + i] << (i * 8));
                    }
                    
                    // 目标单元字节数，获取第18-21个字节（4字节计数）
                    dma_param.sgtp.Destination_elem_Byte_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.sgtp.Destination_elem_Byte_num |= ((uint32_t)data[17 + i] << (i * 8));
                    }
                    
                    // 目标帧数，获取第22-25个字节（4字节计数）
                    dma_param.sgtp.Destination_array_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.sgtp.Destination_array_num |= ((uint32_t)data[21 + i] << (i * 8));
                    }
                    
                    break;
                case 0x03:
                    dma_param.p2pt.trans_mode = trans_mode_flag;
                    
                    // 源地址，获取第2-9个字节（8字节地址）
                    dma_param.p2pt.Source_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.p2pt.Source_addr |= ((uint64_t)data[1 + i] << (i * 8));
                    }
                    
                    // 源帧索引，获取第10-17个字节（8字节索引）
                    dma_param.p2pt.Source_array_index = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.p2pt.Source_array_index |= ((uint64_t)data[9 + i] << (i * 8));
                    }
                    
                    // 源单元字节数，获取第18-21个字节（4字节计数）
                    dma_param.p2pt.Source_elem_Byte_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.p2pt.Source_elem_Byte_num |= ((uint32_t)data[17 + i] << (i * 8));
                    }
                    
                    // 源帧数，获取第22-25个字节（4字节计数）
                    dma_param.p2pt.Source_array_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.p2pt.Source_array_num |= ((uint32_t)data[21 + i] << (i * 8));
                    }
                    
                    // 目标地址，获取第26-33个字节（8字节地址）
                    dma_param.p2pt.Destination_addr = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.p2pt.Destination_addr |= ((uint64_t)data[25 + i] << (i * 8));
                    }
                    
                    // 目标帧索引，获取第34-41个字节（8字节索引）
                    dma_param.p2pt.Destination_array_index = 0;
                    for(int i = 0; i < 8; i++) {
                        dma_param.p2pt.Destination_array_index |= ((uint64_t)data[33 + i] << (i * 8));
                    }
                    
                    // 目标单元字节数，获取第42-45个字节（4字节计数）
                    dma_param.p2pt.Destination_elem_Byte_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.p2pt.Destination_elem_Byte_num |= ((uint32_t)data[41 + i] << (i * 8));
                    }
                    
                    // 目标帧数，获取第46-49个字节（4字节计数）
                    dma_param.p2pt.Destination_array_num = 0;
                    for(int i = 0; i < 4; i++) {
                        dma_param.p2pt.Destination_array_num |= ((uint32_t)data[45 + i] << (i * 8));
                    }
                    break;
                default:
                    SC_REPORT_ERROR("DMA", "Unsupported transfer mode");
                    sc_stop();
                    break;
            }
            // 更新状态
            dma_state = BUSY;
            init_process_start_event.notify();
        }
        else{
            SC_REPORT_ERROR("DMA", "DMA is busy");
            sc_stop();
        }
        // sc_dt::uint64 address = trans.get_address();
        // if(address >= SM_BASE_ADDR && address < SM_BASE_ADDR + SM_SIZE){
        //     dma2sm_init_socket->b_transport(trans, delay);
        // }else if(address >= AM_BASE_ADDR && address < AM_BASE_ADDR + AM_SIZE){
        //     dma2am_init_socket->b_transport(trans, delay);
        // }else{
        //     SC_REPORT_ERROR("DMA", "Address out of range");
        // }
    }
    //DMI请求方法
    virtual bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        sc_dt::uint64 addr = trans.get_address();
        if(addr >= SM_BASE_ADDR && addr < SM_BASE_ADDR + SM_SIZE){
            return dma2sm_init_socket->get_direct_mem_ptr(trans, dmi_data);
        }else if(addr >= AM_BASE_ADDR && addr < AM_BASE_ADDR + AM_SIZE){
            return dma2am_init_socket->get_direct_mem_ptr(trans, dmi_data);
        }else{
            SC_REPORT_ERROR("DMA", "Address out of range");
        }
    }

    virtual void invalidate_direct_mem_ptr(int id, sc_dt::uint64 start_range, 
                                         sc_dt::uint64 end_range) 
    {
        // 收到 DMI 失效通知时，清除对应的 DMI 指针
        dmi_ptr_valid = false;
        // 如果需要，可以记录日志
        SC_REPORT_INFO("DMA", "DMI access invalidated");
    }

private:
    bool dmi_ptr_valid;  // 标记 DMI 指针是否有效
    //dma状态变量
    enum DMA_STATE{
        IDLE,
        BUSY,
        SG_READY,
        ERROR
    };

    DMA_STATE dma_state = IDLE;
    sc_time dma_delay = sc_time(0, SC_NS);
    uint8_t trans_mode_flag;
    //定义内部变量
    tlm::tlm_generic_payload* dma_payload;
    Trans_Param dma_param;
        // 声明DMI对象
    tlm::tlm_dmi dmi_data_read;
    tlm::tlm_dmi dmi_data_write;
    tlm::tlm_generic_payload dma_read_trans;
    tlm::tlm_generic_payload dma_write_trans;
    //事件通知变量
    sc_event init_process_start_event;
    sc_event simple_continuous_trans_event;
    sc_event simple_continuous_trans_done_event;
    sc_event matrix_transpose_transfer_event;
    sc_event matrix_transpose_transfer_done_event;
    sc_event sg_transfer_event;
    sc_event sg_transfer_done_event;
    sc_event point2point_transfer_event;
    sc_event point2point_transfer_done_event;

    // Helper function to get DMI access
    bool get_dmi_access(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data, uint64_t addr, const char* mem_name) {
        if (addr >= SM_BASE_ADDR && addr < SM_BASE_ADDR + SM_SIZE) {
            return dma2sm_init_socket->get_direct_mem_ptr(trans, dmi_data);
        } else if (addr >= AM_BASE_ADDR && addr < AM_BASE_ADDR + AM_SIZE) {
            return dma2am_init_socket->get_direct_mem_ptr(trans, dmi_data);
        } else if (addr >= DDR_BASE_ADDR && addr < DDR_BASE_ADDR + DDR_SIZE) {
            return dma2vcore_init_socket->get_direct_mem_ptr(trans, dmi_data);
        } else if (addr >= GSM_BASE_ADDR && addr < GSM_BASE_ADDR + GSM_SIZE) {
            return dma2vcore_init_socket->get_direct_mem_ptr(trans, dmi_data);
        } else {
            SC_REPORT_ERROR("DMA", (std::string("Address out of range for ") + mem_name).c_str());
            return false;
        }
    }
};





#endif
