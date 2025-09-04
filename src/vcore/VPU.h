#ifndef VPU_H
#define VPU_H

#include "../../util/const.h"
#include "../../util/tools.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;


// 定义MAC支持的操作类型
enum OpType {
    MAC_OP,  // 乘加操作 C = A*B + C
    ADD_OP,  // 加法操作 C = A + B
    SUB_OP   // 减法操作 C = A - B
};
// 定义MAC子模块
template<typename T>
class MAC : public sc_module {
public:
    // TLM接口
    tlm_utils::simple_target_socket<MAC> socket;
    
    SC_CTOR(MAC) : socket("socket") {
        socket.register_b_transport(this, &MAC::b_transport);
        result = T();  // 初始化结果
    }
    
    // MAC的计算函数，根据操作类型执行不同运算
    void compute(T* source, OpType op_type) {
        switch(op_type) {
            case MAC_OP:
                // 执行乘加操作 C = A*B + C
                source[2] = source[0] * source[1] + source[2];
                break;
            case ADD_OP:
                // 执行加法操作 C = A + B
                source[2] = source[0] + source[1];
                //cout << "VPU: " <<this->name()<< "执行" << source[2] << " = " << source[0] << " + " << source[1] << endl;
                break;
            case SUB_OP:
                // 执行减法操作 C = A - B
                source[2] = source[0] - source[1];
                break;
            default:
                // 未知操作类型
                SC_REPORT_INFO("MAC", "Unknown operation type");
                break;
        }
    }

    // 处理传入的数据
    void b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            // 使用扩展接口获取操作类型
            T* data = reinterpret_cast<T*>(trans.get_data_ptr());
            
            // 获取操作类型，假设它存储在数据指针之前的一个字节
            unsigned char* full_data = trans.get_data_ptr();
            OpType op_type = static_cast<OpType>(full_data[0]);
            
            // 调整数据指针，跳过操作类型字节
            T* compute_data = reinterpret_cast<T*>(full_data + 1);
            
            // 执行计算
            compute(compute_data, op_type);
            
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            // 如果不需要读取操作，可以返回错误状态
            SC_REPORT_INFO("MAC", "Read command is not supported");
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        }
    }

private:
    T result;
};
//定义VPU子模块，VPU包含所有的MAC单元
template<typename T>
class VPU : public sc_module {
public:
    // // 使用与MAC相同的操作类型枚举
    // typedef typename MAC<T>::OpType OpType;

    tlm_utils::multi_passthrough_target_socket<VPU, 512> spu2vpu_target_socket;
    vector<tlm_utils::simple_initiator_socket<VPU>*> mac_sockets;  // 连接MAC的socket
    vector<MAC<T>*> mac_units;  // MAC单元数组
    
    SC_CTOR(VPU) : spu2vpu_target_socket("spu2vpu_target_socket") {
        spu2vpu_target_socket.register_b_transport(this, &VPU::b_transport);
        // 创建MAC单元
        for(int i = 0; i < MAC_PER_VPU; i++) {
            string mac_name = "mac_" + to_string(i);
            MAC<T>* mac = new MAC<T>(mac_name.c_str());
            mac_units.push_back(mac);
            
            // 创建并绑定socket
            auto* init_socket = new tlm_utils::simple_initiator_socket<VPU>(
                ("init_socket_" + to_string(i)).c_str());
            mac_sockets.push_back(init_socket);
            init_socket->bind(mac->socket);
        }
    }
    
    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay) {
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            // 获取操作类型和数据
            unsigned char* data_ptr = trans.get_data_ptr();
            OpType op_type = static_cast<OpType>(data_ptr[0]);
            
            // 调整数据指针，跳过操作类型字节
            T* input_data = reinterpret_cast<T*>(data_ptr + 1);
            
            // 计算元素总数和组数
            uint64_t total_data_bytes = trans.get_data_length() - 1; // 减去操作类型字节
            uint64_t total_elements = total_data_bytes / sizeof(T);
            uint64_t num_groups = total_elements / 3;  // 每组3个元素(A,B,C)
            
            // 使用的MAC数量不能超过数据组数和可用MAC数量的较小值
            //判断是否超过MAC_PER_VPU，如果超过，打印警告信息
            uint64_t active_macs = num_groups;
            if(active_macs > MAC_PER_VPU){
                cerr << "VPU: " <<this->name()<< " 请求使用的MAC数量超过" << MAC_PER_VPU << "个！！！！！！！！！" << endl;
                exit(1);
            }
            // 准备每个MAC的数据缓冲区，包括操作类型
            vector<unsigned char*> mac_data_buffers;
            for (uint64_t mac_id = 0; mac_id < active_macs; mac_id++) {
                // 为每个MAC创建数据缓冲区，1字节操作类型 + 3个T类型元素
                unsigned char* buffer = new unsigned char[1 + 3 * sizeof(T)];
                
                // 设置操作类型
                buffer[0] = static_cast<unsigned char>(op_type);
                
                // 复制计算数据
                memcpy(buffer + 1, input_data + mac_id * 3, 3 * sizeof(T));
                
                mac_data_buffers.push_back(buffer);
            }
            
            // 并行启动MAC单元
            for (uint64_t mac_id = 0; mac_id < active_macs; mac_id++) {
                // 为每个MAC创建事务
                tlm::tlm_generic_payload mac_trans;
                mac_trans.set_command(tlm::TLM_WRITE_COMMAND);
                
                // 使用准备好的数据缓冲区
                mac_trans.set_data_ptr(mac_data_buffers[mac_id]);
                mac_trans.set_data_length(1 + 3 * sizeof(T));  // 1字节操作类型 + 3个元素
                
                // 发送到MAC进行计算
                (*mac_sockets[mac_id])->b_transport(mac_trans, delay);
            }
            switch(op_type) {
                case MAC_OP:
                    wait(MAC_LATENCY);
                    break;
                case ADD_OP:
                    wait(ADD_LATENCY);
                    break;
                case SUB_OP:
                    wait(SUB_LATENCY);
                    break;
                default:
                    break;
            }
            // 等待所有活跃的MAC完成
            //wait(MAC_LATENCY);
            //delay += MAC_LATENCY;
            
            // 收集MAC计算结果并复制回输入缓冲区
            for (uint64_t mac_id = 0; mac_id < active_macs; mac_id++) {
                // 获取MAC计算后的数据指针
                T* mac_result = reinterpret_cast<T*>(mac_data_buffers[mac_id] + 1);
                // 将计算结果复制回原始缓冲区中对应的位置
                memcpy(input_data + mac_id * 3, mac_result, 3 * sizeof(T));
                
                // // 打印调试信息
                // cout << "收集MAC " << mac_id << " 结果: (" << mac_result[0] << ", " 
                //      << mac_result[1] << ", " << mac_result[2] << ")" << endl;
            }
            
            // 释放为MAC准备的数据缓冲区
            for (auto buffer : mac_data_buffers) {
                delete[] buffer;
            }
            
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            // 读取结果（如果需要）
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        }
    }
    
    ~VPU() {
        for(auto mac : mac_units) {
            delete mac;
        }
        for(auto socket : mac_sockets) {
            delete socket;
        }
    }
};
#endif