#ifndef BASE_INITIATOR_MODEL_H
#define BASE_INITIATOR_MODEL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/multi_passthrough_initiator_socket.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <vector>
#include <iostream>
#include <string>
#include "../src/vcore/PEA/systolic_array_top_tlm.h"
#include "../src/vcore/FFT_SA/include/FFT_TLM.h"
#include "../src/vcore/FFT_SA/utils/complex_types.h"
#include "../src/vcore/FFT_SA/utils/fft_test_utils.h"
#include "const.h"
#include "tools.h"
#include "instruction.h"
#include "instruction_pea.h"

using namespace std;
using namespace sc_core;

/**
 * @brief 基础Initiator模型类，提供DMI访问的基本功能
 * 
 * 该类封装了通用的DMI操作，包括设置DMI、读写数据等功能，
 * 可作为各种具体Initiator模型的基类
 */
template <typename T>
SC_MODULE(BaseInitiatorModel) {
public:
    tlm_utils::multi_passthrough_initiator_socket<BaseInitiatorModel,512> socket;
    tlm_utils::multi_passthrough_target_socket<BaseInitiatorModel,512> soc2ext_target_socket;
    //setup AM和SM的DMI
    tlm::tlm_dmi sm_dmi;
    tlm::tlm_dmi am_dmi;
    tlm::tlm_dmi ddr_dmi;   
    tlm::tlm_dmi gsm_dmi;
    sc_event blocked_computation_done_event;

    int array_width;
    int array_height;
    //note: 下面构造函数进行定义阵列的规模,需要与Vore.h中的pe_array_size一致
    SC_HAS_PROCESS(BaseInitiatorModel);
    BaseInitiatorModel(sc_module_name name) : sc_module(name), 
        socket("socket"),soc2ext_target_socket("soc2ext_target_socket"),
        array_width(16),array_height(16) {
        socket.register_invalidate_direct_mem_ptr(this, &BaseInitiatorModel::invalidate_direct_mem_ptr);
        soc2ext_target_socket.register_b_transport(this, &BaseInitiatorModel::b_transport);
    }

    void invalidate_direct_mem_ptr(int id, sc_dt::uint64 start_range, sc_dt::uint64 end_range) {
        cout << "DMI invalidated. Range: " << hex << start_range << " - " << end_range << endl;
    }
    
    void b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        uint8_t* data_ptr = trans.get_data_ptr();
        uint64_t addr = trans.get_address();
        
        if (addr == 0xFFFFFFFF && *data_ptr == 1) {
            // 收到计算完成通知
            blocked_computation_done_event.notify();
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);

        //补充接收GEMM结果就绪的trans
    }

    /**
     * @brief 设置DMI访问
     * 
     * 为指定基址建立DMI访问权限
     * 
     * @param base_addr 基址
     * @param dmi DMI引用，用于存储DMI信息
     * @param module_name 模块名称，用于调试输出
     */
    void setup_dmi(uint64_t base_addr, tlm::tlm_dmi& dmi, const std::string& module_name = "BaseInitiator") {
        tlm::tlm_generic_payload trans;
        trans.set_address(base_addr);
        // 获取dmi访问权限
        if (socket->get_direct_mem_ptr(trans, dmi)) {
            cout << module_name << " DMI setup successful for range: 0x" << hex
                 << dmi.get_start_address() << " - 0x" << dmi.get_end_address() << endl;
        } else {
            SC_REPORT_ERROR(module_name.c_str(), "DMI setup failed");
        }
    }
    
    /**
     * @brief 无延迟DMI读取数据
     * 
     * 从DMI区域直接读取数据，不考虑时序延迟
     * 
     * @param start_addr 起始地址
     * @param values 存储读取数据的向量
     * @param data_num 要读取的数据项数量
     * @param dmi DMI引用
     */
    void read_data_dmi_no_latency(uint64_t start_addr, vector<T>& values, unsigned int data_num, const tlm::tlm_dmi& dmi) {
        // 检查DMI读取权限
        if (!dmi.is_read_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI read not allowed");
            return;
        }
        // 检查地址范围
        if (start_addr < dmi.get_start_address() || 
            start_addr + data_num * sizeof(T) > dmi.get_end_address()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
            return;
        }
        // 调整vector大小以容纳数据
        values.resize(data_num);

        // 获取DMI指针并计算偏移
        unsigned char* dmi_ptr = dmi.get_dmi_ptr();
        uint64_t offset = start_addr - dmi.get_start_address();

        // 直接从内存读取
        T* source = reinterpret_cast<T*>(dmi_ptr + offset);
        for (unsigned int i = 0; i < data_num; i++) {
            values[i] = static_cast<T>(source[i]);  // 添加类型转换
        }
    }
    void read_complex_data_dmi_no_latency(uint64_t start_addr, vector<complex<T>>& values, unsigned int data_num, const tlm::tlm_dmi& dmi) {
        // 检查DMI读取权限
        if (!dmi.is_read_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI read not allowed");
            return;
        }
        // 检查地址范围
        if (start_addr < dmi.get_start_address() || 
            start_addr + data_num * sizeof(complex<T>) > dmi.get_end_address()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
            return;
        }
        // 调整vector大小以容纳数据
        values.resize(data_num);

        // 获取DMI指针并计算偏移
        unsigned char* dmi_ptr = dmi.get_dmi_ptr();
        uint64_t offset = start_addr - dmi.get_start_address();

        // 直接从内存读取
        complex<T>* source = reinterpret_cast<complex<T>*>(dmi_ptr + offset);
        for (unsigned int i = 0; i < data_num; i++) {
            values[i] = static_cast<complex<T>>(source[i]);  // 添加类型转换
        }

    }
    
    /**
     * @brief 无延迟DMI写入数据
     * 
     * 直接写入数据到DMI区域，不考虑时序延迟
     * 
     * @param start_addr 起始地址
     * @param values 要写入的数据向量
     * @param data_num 要写入的数据项数量
     * @param dmi DMI引用
     */
    void write_data_dmi_no_latency(uint64_t start_addr, const vector<T>& values, unsigned int data_num, const tlm::tlm_dmi& dmi) {
        // 检查DMI访问权限
        if (!dmi.is_write_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI write not allowed");
            return;
        }

        // 检查地址范围
        if (start_addr < dmi.get_start_address() || 
            start_addr + data_num * sizeof(T) > dmi.get_end_address()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
            return;
        }

        // 检查数据大小
        if (data_num > values.size()) {
            SC_REPORT_ERROR("BaseInitiator", "Data size mismatch");
            return;
        }

        // 获取DMI指针并计算偏移
        unsigned char* dmi_ptr = dmi.get_dmi_ptr();
        uint64_t offset = start_addr - dmi.get_start_address();

        // 直接写入内存
        T* target = reinterpret_cast<T*>(dmi_ptr + offset);
        for (unsigned int i = 0; i < data_num; i++) {
            target[i] = values[i];
        }

        cout << "DMI写入完成:写入" << dec << data_num*sizeof(T) << "字节数据到地址0x" 
            << hex << start_addr << dec << endl;
    }
    
    // 添加专门针对复数类型的写入函数
    void write_complex_data_dmi_no_latency(uint64_t start_addr, const vector<complex<T>>& values, unsigned int data_num, const tlm::tlm_dmi& dmi) {
        // 检查DMI访问权限
        if (!dmi.is_write_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI write not allowed");
            return;
        }

        // 检查地址范围
        if (start_addr < dmi.get_start_address() || 
            start_addr + data_num * sizeof(complex<T>) > dmi.get_end_address()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
            return;
        }

        // 检查数据大小
        if (data_num > values.size()) {
            SC_REPORT_ERROR("BaseInitiator", "Data size mismatch");
            return;
        }

        // 获取DMI指针并计算偏移
        unsigned char* dmi_ptr = dmi.get_dmi_ptr();
        uint64_t offset = start_addr - dmi.get_start_address();

        // 直接写入内存
        complex<T>* target = reinterpret_cast<complex<T>*>(dmi_ptr + offset);
        for (unsigned int i = 0; i < data_num; i++) {
            target[i] = values[i];
        }
    }
    
    /**
     * @brief 按索引写入DMI数据
     * 
     * 根据提供的索引列表，写入数据到DMI区域的指定位置
     * 
     * @param start_addr 基址
     * @param values 要写入的数据向量
     * @param data_num 要写入的数据项数量
     * @param index 索引向量，指定每个数据项的相对偏移
     * @param dmi DMI引用
     */
    void write_data_dmi_index(uint64_t start_addr, const vector<T>& values, unsigned int data_num, const vector<int>& index, const tlm::tlm_dmi& dmi) {
        // 检查DMI访问权限
        if (!dmi.is_write_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI write not allowed");
            return;
        }

        // 检查数据大小
        if (data_num > values.size() || data_num > index.size()) {
            SC_REPORT_ERROR("BaseInitiator", "Data size mismatch");
            return;
        }

        // 获取DMI指针
        unsigned char* dmi_ptr = dmi.get_dmi_ptr();
        uint64_t dmi_start = dmi.get_start_address();
        uint64_t dmi_end = dmi.get_end_address();

        // 根据索引写入数据
        for (unsigned int i = 0; i < data_num; i++) {
            // 计算目标地址
            uint64_t target_addr = start_addr + index[i] * sizeof(T);
            
            // 检查地址是否在DMI范围内
            if (target_addr < dmi_start || target_addr + sizeof(T) > dmi_end) {
                SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
                return;
            }
            
            // 计算偏移量
            uint64_t offset = target_addr - dmi_start;
            
            // 写入数据
            T* target = reinterpret_cast<T*>(dmi_ptr + offset);
            *target = values[i];
        }

        // cout << "DMI索引写入完成: 写入" << dec << data_num << "个数据项到基地址0x" 
        //     << hex << start_addr << dec << "，使用索引列表" << endl;
    }
    
    // 添加专门针对复数类型的按索引写入函数（不需要索引参数的简化版本）
    void write_complex_data_dmi_index(uint64_t start_addr, const vector<complex<T>>& values, unsigned int data_num, const tlm::tlm_dmi& dmi) {
        // 检查DMI写入权限
        if (!dmi.is_write_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI write not allowed");
            return;
        }
        // 检查地址范围
        if (start_addr < dmi.get_start_address() || 
            start_addr + data_num * sizeof(complex<T>) > dmi.get_end_address()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
            return;
        }

        // 获取DMI指针并转换为复数类型
        complex<T>* mem_ptr = reinterpret_cast<complex<T>*>(dmi.get_dmi_ptr() + (start_addr - dmi.get_start_address()));

        // 顺序写入数据
        for (unsigned int i = 0; i < data_num && i < values.size(); ++i) {
            mem_ptr[i] = values[i];
        }
    }

    /**
     * @brief 使用DMI按索引写入复数数据
     * 
     * 将复数数据按照指定索引写入DMI区域，不考虑时序延迟
     * 
     * @param start_addr 起始地址
     * @param values 要写入的复数数据向量
     * @param data_num 要写入的数据项数量
     * @param index 索引向量，指定每个数据项的写入位置
     * @param dmi DMI引用
     */
    void write_complex_data_dmi_index(uint64_t start_addr, const vector<complex<T>>& values, unsigned int data_num, const vector<int>& index, const tlm::tlm_dmi& dmi) {
        // 检查DMI访问权限
        if (!dmi.is_write_allowed()) {
            SC_REPORT_ERROR("BaseInitiator", "DMI write not allowed");
            return;
        }

        // 检查数据大小
        if (data_num > values.size() || data_num > index.size()) {
            SC_REPORT_ERROR("BaseInitiator", "Data size mismatch");
            return;
        }

        // 获取DMI指针
        unsigned char* dmi_ptr = dmi.get_dmi_ptr();
        uint64_t dmi_start = dmi.get_start_address();
        uint64_t dmi_end = dmi.get_end_address();

        // 根据索引写入数据
        for (unsigned int i = 0; i < data_num; i++) {
            // 计算目标地址
            uint64_t target_addr = start_addr + index[i] * sizeof(complex<T>);
            
            // 检查地址是否在DMI范围内
            if (target_addr < dmi_start || target_addr + sizeof(complex<T>) > dmi_end) {
                SC_REPORT_ERROR("BaseInitiator", "DMI address out of range");
                return;
            }
            
            // 计算偏移量
            uint64_t offset = target_addr - dmi_start;
            
            // 写入数据
            complex<T>* target = reinterpret_cast<complex<T>*>(dmi_ptr + offset);
            *target = values[i];
        }
    }
    
 

    //=============关于GEMM_TLM的指令封装==============
 
    
};

#endif // BASE_INITIATOR_MODEL_H
