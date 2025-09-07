#ifndef BASE_INITIATOR_MODEL_H
#define BASE_INITIATOR_MODEL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/multi_passthrough_initiator_socket.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <vector>
#include <iostream>
#include <string>
#include "../src/vcore/FFT_SA/include/FFT_TLM.h"
#include "../src/vcore/FFT_SA/utils/complex_types.h"
#include "../src/vcore/FFT_SA/utils/fft_test_utils.h"
#include "const.h"
#include "tools.h"
#include "instruction.h"


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
    
    // FFT事件驱动控制事件
    sc_event fft_input_ready_event;     // 输入数据写入完成事件
    sc_event fft_result_ready_event;    // FFT计算完成事件  
    sc_event fft_output_ready_event;    // 输出数据读取完成事件

    int array_width;
    int array_height;
    //note: 下面构造函数进行定义阵列的规模,需要与Vore.h中的pe_array_size一致

    
    // FFT时序控制常量(基于测试代码的验证值)
    static constexpr int FFT_CONFIG_WAIT_CYCLES = 15;       // 配置等待周期(1ns/cycle)
    static constexpr int FFT_TWIDDLE_WAIT_CYCLES = 25;      // 旋转因子加载等待周期
    static constexpr int FFT_INPUT_WAIT_CYCLES = 20;        // 输入数据写入等待周期
    static constexpr int FFT_PROCESSING_WAIT_CYCLES = 100;  // FFT处理等待周期
    static constexpr int FFT_OUTPUT_WAIT_CYCLES = 20;       // 输出数据读取等待周期
    
    // FFT事件通知协议地址定义
    static constexpr uint64_t FFT_EVENT_BASE_ADDR = 0xFFFF0000;     // 事件通知基地址
    static constexpr uint64_t FFT_INPUT_READY_ADDR = 0xFFFF0001;    // 输入数据写入完成事件地址
    static constexpr uint64_t FFT_RESULT_READY_ADDR = 0xFFFF0002;   // FFT计算完成事件地址
    static constexpr uint64_t FFT_OUTPUT_READY_ADDR = 0xFFFF0003;   // 输出数据读取完成事件地址
    
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
            // 收到计算完成通知（保持原有逻辑）
            blocked_computation_done_event.notify();
        }
        // FFT事件通知处理
        else if (addr == FFT_INPUT_READY_ADDR) {
            cout << sc_time_stamp() << " [BaseInitiatorModel] 收到FFT输入数据写入完成事件通知" << endl;
            fft_input_ready_event.notify();
        }
        else if (addr == FFT_RESULT_READY_ADDR) {
            cout << sc_time_stamp() << " [BaseInitiatorModel] 收到FFT计算完成事件通知" << endl;
            fft_result_ready_event.notify();
        }
        else if (addr == FFT_OUTPUT_READY_ADDR) {
            cout << sc_time_stamp() << " [BaseInitiatorModel] 收到FFT输出数据读取完成事件通知" << endl;
            fft_output_ready_event.notify(1,SC_NS);  // 使用SC_ZERO_TIME延迟通知
        }
        
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
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

    //=============关于FFT_TLM的指令封装==============
    
    /**
     * @brief FFT系统复位
     * 发送FFT阵列复位命令
     */
    void send_fft_reset_transaction() {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(0, SC_NS);
        
        FFTExtension ext;
        ext.cmd = FFTCommand::RESET_FFT_ARRAY;
        ext.data_size = 0;
        
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(FFT_BASE_ADDR);
        trans.set_extension(&ext);
        
        uint8_t dummy = 0;
        trans.set_data_ptr(&dummy);
        trans.set_data_length(1);
        socket->b_transport(trans, delay);
        
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            cout << "ERROR: FFT reset transaction failed" << endl;
        }
        
        trans.clear_extension(&ext);
    }
    
    /**
     * @brief FFT配置设置
     * 配置FFT参数(模式、大小等)
     */
    void send_fft_configure_transaction(const FFTConfiguration& config) {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(0, SC_NS);
        
        FFTExtension ext;
        ext.cmd = FFTCommand::CONFIGURE_FFT_MODE;
        
        ext.data_size = sizeof(FFTConfiguration);
        
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(FFT_BASE_ADDR);
        trans.set_extension(&ext);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(const_cast<FFTConfiguration*>(&config)));
        trans.set_data_length(sizeof(FFTConfiguration));
        
        socket->b_transport(trans, delay);
        
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            cout << "ERROR: FFT configure transaction failed" << endl;
        }
        
        trans.clear_extension(&ext);
    }
    
    /**
     * @brief 加载FFT旋转因子
     * 批量加载标准旋转因子到FFT模块
     */
    void send_fft_load_twiddles_transaction() {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(0, SC_NS);
        
        FFTExtension ext;
        ext.cmd = FFTCommand::LOAD_TWIDDLE_FACTORS;
        
        ext.data_size = 0;
        
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(FFT_BASE_ADDR);
        trans.set_extension(&ext);
        
        uint8_t dummy = 0;
        trans.set_data_ptr(&dummy);
        trans.set_data_length(1);
        
        socket->b_transport(trans, delay);
        
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            cout << "ERROR: FFT twiddle loading transaction failed" << endl;
        }
        
        trans.clear_extension(&ext);
    }
    
    /**
     * @brief 写入FFT输入数据
     * 将N点复数输入数据写入FFT模块
     * @param input_data N点复数输入数据
     */
    void send_fft_write_input_transaction(int N, const vector<complex<T>>& input_data) {
        // 转换为16路浮点格式
        vector<T> float_data(2*N, 0.0f);
        FFTTestUtils::map_complex_input_to_T_float(N, input_data, float_data);
        
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(0, SC_NS);
        
        FFTExtension ext;
        ext.cmd = FFTCommand::WRITE_INPUT_DATA;
        ext.data_size = float_data.size() * sizeof(T);
        
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(FFT_BASE_ADDR);
        trans.set_extension(&ext);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(float_data.data()));
        trans.set_data_length(float_data.size() * sizeof(float));
        
        socket->b_transport(trans, delay);
        
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            cout << "ERROR: FFT input data write transaction failed" << endl;
        }
        
        trans.clear_extension(&ext);
    }
    
    /**
     * @brief 启动FFT处理
     * 启动FFT计算
     */
    void send_fft_start_processing_transaction() {
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(0, SC_NS);
        
        FFTExtension ext;
        ext.cmd = FFTCommand::START_FFT_PROCESSING;
        ext.data_size = 0;
        
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(FFT_BASE_ADDR);
        trans.set_extension(&ext);
        
        uint8_t dummy = 0;
        trans.set_data_ptr(&dummy);
        trans.set_data_length(1);
        //cout << "start computing cmd" << endl;
        socket->b_transport(trans, delay);
        
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            cout << "ERROR: FFT processing start transaction failed" << endl;
        }
        
        trans.clear_extension(&ext);
    }
    
    /**
     * @brief 读取FFT输出数据
     * 从FFT模块读取8点复数输出结果
     * @return N点复数输出数据
     */
    vector<complex<T>> send_fft_read_output_transaction(int N) {
        // 准备N路复数浮点输出缓冲区

        vector<T> float_output(2*N, 0.0f);
        
        tlm::tlm_generic_payload trans;
        sc_time delay = sc_time(0, SC_NS);
        
        FFTExtension ext;
        ext.cmd = FFTCommand::READ_OUTPUT_DATA;
        ext.data_size = float_output.size() * sizeof(float);
        
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(FFT_BASE_ADDR);
        trans.set_extension(&ext);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(float_output.data()));
        trans.set_data_length(float_output.size() * sizeof(float));
        socket->b_transport(trans, delay);
        
        
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            cout << "ERROR: FFT output data read transaction failed" << endl;
        }
        
        trans.clear_extension(&ext);

        // cout << "float_output data: ";
        // for (size_t i = 0; i < float_output.size(); ++i) {
        //     cout << float_output[i] << " ";
        // }
        // cout << endl;
        
        // 重构点复数输出
        return FFTTestUtils::reconstruct_complex_from_T_parallel(N, float_output);
    }
    
    /**
     * @brief 一站式FFT计算
     * 完成FFT的完整流程：配置->输入->处理->输出
     * @param input_data 8点复数输入数据
     * @param frame_id 帧ID(默认为0)
     * @param fft_size FFT大小(默认为8)
     * @return 8点复数FFT结果
     */
    vector<complex<T>> perform_fft(const vector<complex<T>>& input_data, 
                                                   size_t fft_size = 8) {
        cout << "\n[FFT_base_init] Starting one-stop " << fft_size << "-point FFT computation\n";
        
        // Check input data size
        if (input_data.size() != fft_size) {
            cout << "ERROR: Input data size (" << input_data.size() << ") does not match FFT size (" << fft_size << ")" << endl;
            return vector<complex<T>>();
        }
        
        // 4. 写入输入数据
        cout << "[FFT_base_init] 4/5 写入输入数据..." << endl;
        send_fft_write_input_transaction(fft_size, input_data);
        wait(fft_input_ready_event);  // 等待输入数据写入完成事件
        
        // 5. 启动FFT处理
        cout << "[FFT_base_init] 5/5 启动FFT处理..." << endl;
        send_fft_start_processing_transaction();
        wait(fft_result_ready_event);  // 等待FFT计算完成事件
        
        // 6. 读取输出结果
        cout << sc_time_stamp<< "[FFT_base_init] 读取输出结果..." << endl;
        auto output_data = send_fft_read_output_transaction(fft_size);
        cout << sc_time_stamp()<< "[FFT_base_init] 等待输出数据读取完成事件..." << endl;
        //wait(fft_output_ready_event);  // 等待输出数据读取完成事件
        cout << sc_time_stamp()<< "[FFT_base_init] 输出数据读取完成事件已收到" << endl;
        cout << sc_time_stamp()<< "[FFT_base_init] FFT计算完成 - " << output_data.size() << "个复数结果\n" << endl;
        wait(1,SC_NS);
        return output_data;
            
    }
    
};

#endif // BASE_INITIATOR_MODEL_H
