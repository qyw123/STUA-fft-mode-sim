/**
 * @file test_fft_tlm.cpp
 * @brief FFT TLM2.0模块测试程序 - 精简参数化版本
 * 
 * @version 3.0 - 精简版
 * @date 2025-01-01
 */

#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "../include/fft_tlm.h"
#include "../utils/complex_types.h"
#include "../utils/config.h"
#include "../utils/fft_test_utils.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <algorithm>

using namespace sc_core;
using namespace tlm;
using namespace std;

/**
 * @brief 精简的FFT TLM2.0测试发起端
 */
SC_MODULE(FftTlmTestbench) {
    // TLM发起端socket
    tlm_utils::simple_initiator_socket<FftTlmTestbench> init_socket{"init_socket"};
    
    // 测试配置参数
    unsigned hardware_size;  // 硬件FFT点数
    unsigned test_size;      // 测试FFT点数
    
    SC_CTOR(FftTlmTestbench) : hardware_size(16), test_size(4) {
        SC_THREAD(test_process);
    }
    
    // 设置测试参数
    void set_test_config(unsigned hw_size, unsigned t_size) {
        hardware_size = hw_size;
        test_size = t_size;
        cout << "配置测试参数: " << test_size << "点FFT on " << hardware_size << "点硬件" << endl;
    }
    
private:
    void test_process();
    
    // 核心测试方法
    void test_fft_with_config();
    
    // 自动bypass配置
    uint32_t calculate_bypass_mask(unsigned hw_size, unsigned t_size);
    void load_twiddles_for_config(unsigned hw_size, unsigned t_size, uint32_t bypass_mask);
    
    // 数据映射辅助方法
    vector<complex<float>> map_input_data(const vector<complex<float>>& input, unsigned hw_size, unsigned fft_size);
    vector<complex<float>> extract_output_data(const vector<complex<float>>& pe_y0,
                                              const vector<complex<float>>& pe_y1,
                                              unsigned t_size, unsigned hw_size);
    
    // TLM传输辅助方法
    void write_register(uint32_t addr, uint32_t data);
    uint32_t read_register(uint32_t addr);
    void write_data(uint32_t addr, const vector<complex<float>>& data);
    vector<complex<float>> read_data(uint32_t addr, size_t count);
    
    // 打印辅助方法
    void print_test_header(const string& test_name);
    void print_test_result(const string& test_name, bool passed);
    void print_complex_vector(const string& name, const vector<complex<float>>& data);
    
    // FFT验证
    bool verify_fft_result(const vector<complex<float>>& fft_output, 
                          const vector<complex<float>>& input_data, 
                          unsigned size, float tolerance = 1e-2);
};

void FftTlmTestbench::test_process() {
    cout << "========================================" << endl;
    cout << "FFT TLM2.0 精简测试开始" << endl;
    cout << "测试配置: " << test_size << "点FFT on " << hardware_size << "点硬件" << endl;
    cout << "========================================" << endl;
    
    // 等待初始化
    wait(100, SC_NS);
    
    try {
        test_fft_with_config();
    } catch (const exception& e) {
        cout << "测试过程中发生异常: " << e.what() << endl;
    }
    
    cout << "========================================" << endl;
    cout << "FFT TLM2.0 测试完成" << endl;
    cout << "========================================" << endl;
    
    sc_stop();
}

uint32_t FftTlmTestbench::calculate_bypass_mask(unsigned hw_size, unsigned t_size) {
    // 验证输入参数
    if (t_size > hw_size) {
        cout << "错误: 测试点数(" << t_size << ") 不能大于硬件点数(" << hw_size << ")" << endl;
        return 0;
    }
    
    if ((hw_size & (hw_size - 1)) != 0 || (t_size & (t_size - 1)) != 0) {
        cout << "错误: FFT点数必须是2的幂次" << endl;
        return 0;
    }
    
    unsigned hardware_stages = log2_const(hw_size);
    unsigned test_stages = log2_const(t_size);
    unsigned bypass_stages = hardware_stages - test_stages;
    
    // 生成bypass mask：前bypass_stages位置1
    uint32_t bypass_mask = (1 << bypass_stages) - 1;
    
    cout << "自动计算Bypass配置:" << endl;
    cout << "  硬件规模: " << hw_size << "点 (" << hardware_stages << "级)" << endl;
    cout << "  测试规模: " << t_size << "点 (" << test_stages << "级)" << endl;
    cout << "  Bypass级数: " << bypass_stages << "级" << endl;
    cout << "  Bypass掩码: 0x" << hex << bypass_mask << dec << endl;
    
    return bypass_mask;
}

void FftTlmTestbench::load_twiddles_for_config(unsigned hw_size, unsigned t_size, uint32_t bypass_mask) {
    cout << "加载Twiddle因子..." << endl;
    
    unsigned hardware_stages = log2_const(hw_size);
    unsigned hardware_pes = hw_size / 2;
    unsigned bypass_stages = __builtin_popcount(bypass_mask);
    unsigned active_stages = hardware_stages - bypass_stages;
    
    // 生成适用于当前配置的Twiddle因子
    auto twiddles = FFTTestUtils::generate_fft_twiddles(hw_size, hardware_stages, hardware_pes, bypass_stages);
    
    cout << "  激活" << active_stages << "级，每级" << hardware_pes << "个PE" << endl;
    
    // 设置FFT模式并等待信号传播
    write_register(0x0000, 0x02);  // 设置FFT模式（不启动）
    wait(500, SC_NS);  // 等待模式信号传播到PE
    
    // 加载Twiddle因子到激活的stages
    for (unsigned stage = 0; stage < twiddles.size(); ++stage) {
        unsigned actual_stage = stage + bypass_stages;  // 映射到实际的硬件stage
        
        for (unsigned pe = 0; pe < twiddles[stage].size(); ++pe) {
            // 写入Twiddle控制寄存器
            uint32_t tw_ctrl = (1 << 16) | (actual_stage << 8) | pe;
            write_register(0x0020, tw_ctrl);
            
            // 写入Twiddle实部和虚部
            float tw_real = twiddles[stage][pe].real;
            float tw_imag = twiddles[stage][pe].imag;
            write_register(0x0024, *reinterpret_cast<uint32_t*>(&tw_real));
            write_register(0x0028, *reinterpret_cast<uint32_t*>(&tw_imag));
            
            wait(50, SC_NS);  // 等待加载完成
            
            // 清除load_en
            write_register(0x0020, (actual_stage << 8) | pe);
            
            if (pe < 2) {  // 只显示前几个PE的信息
                cout << "    Stage[" << actual_stage << "] PE[" << pe << "] <- W(" 
                     << tw_real << "," << tw_imag << ")" << endl;
            }
        }
    }
    
    cout << "Twiddle因子加载完成" << endl;
}

vector<complex<float>> FftTlmTestbench::map_input_data(const vector<complex<float>>& input, unsigned hw_size, unsigned fft_size) {
    // 将测试输入数据映射到硬件PE阵列，支持bypass模式
    vector<complex<float>> mapped_input(hw_size, complex<float>(0, 0));
    
    // 计算需要的PE数量
    unsigned required_pes = fft_size / 2;
    unsigned hw_pes = hw_size / 2;
    
    if (fft_size <= hw_size) {
        // bypass模式：DIF (Decimation-In-Frequency) 数据映射
        // 对于N点FFT，需要N/2个PE
        // PE[i]接收 X[i] 和 X[i + N/2]
        for (unsigned i = 0; i < required_pes && i < hw_pes; ++i) {
            if (i < input.size()) {
                mapped_input[i] = input[i];  // A路径
            }
            if ((i + required_pes) < input.size()) {
                mapped_input[i + hw_pes] = input[i + required_pes];  // B路径
            }
        }
    } else {
        // 正常模式：简单顺序映射
        for (unsigned i = 0; i < input.size() && i < hw_size; ++i) {
            mapped_input[i] = input[i];
        }
    }
    
    return mapped_input;
}

vector<complex<float>> FftTlmTestbench::extract_output_data(const vector<complex<float>>& pe_y0,
                                                           const vector<complex<float>>& pe_y1,
                                                           unsigned t_size, unsigned hw_size) {
    vector<complex<float>> extracted_output(t_size);
    
    if (t_size <= hw_size) {
        // bypass模式：需要特殊的输出映射
        // 对于N点FFT，需要N/2个PE的输出
        unsigned required_pes = t_size / 2;
        unsigned hw_pes = hw_size / 2;
        unsigned bypass_stages = log2_const(hw_size) - log2_const(t_size);
        
        cout << "Bypass模式输出提取: 需要" << required_pes << "个PE的结果, bypass_stages=" << bypass_stages << endl;
        
        // 在bypass模式下，PE输出位置按2^bypass_stages的步长分布
        unsigned pe_stride = 1 << bypass_stages;  // 2^bypass_stages
        
        cout << "PE输出步长: " << pe_stride << ", 查找PE位置: ";
        for (unsigned i = 0; i < required_pes; ++i) {
            unsigned pe_idx = i * pe_stride;  // PE位置按步长分布
            cout << pe_idx << " ";
            
            if (pe_idx < pe_y0.size() && pe_idx < pe_y1.size()) {
                extracted_output[i] = pe_y0[pe_idx];                    // Y0 -> 前半部分
                extracted_output[i + required_pes] = pe_y1[pe_idx];     // Y1 -> 后半部分
            }
        }
        cout << endl;
    } else {
        // 正常模式：使用标准映射
        vector<complex<float>> full_output = FFTTestUtils::map_pe_output_to_natural_order(pe_y0, pe_y1, hw_size);
        for (unsigned i = 0; i < t_size; ++i) {
            extracted_output[i] = full_output[i];
        }
    }
    
    return extracted_output;
}

void FftTlmTestbench::test_fft_with_config() {
    string test_name = to_string(test_size) + "点FFT测试 (基于" + to_string(hardware_size) + "点硬件)";
    print_test_header(test_name);
    
    bool test_passed = true;
    
    try {
        // 模块复位
        write_register(0x0000, 0x01);
        wait(20, SC_NS);
        write_register(0x0000, 0x00);
        wait(20, SC_NS);
        
        // 自动计算bypass配置
        uint32_t bypass_mask = calculate_bypass_mask(hardware_size, test_size);
        
        // 配置FFT模式和bypass
        write_register(0x0000, 0x02);  // FFT模式
        write_register(0x0004, 0x00);  // 无移位
        write_register(0x0008, 0x00);  // 无共轭
        write_register(0x000C, bypass_mask);  // 配置bypass
        
        // 加载Twiddle因子
        load_twiddles_for_config(hardware_size, test_size, bypass_mask);
        
        // 生成测试数据
        vector<complex<float>> input_data = FFTTestUtils::generate_test_sequence(test_size, FFTTestUtils::DataGenType::SEQUENTIAL);
        
        cout << test_size << "点FFT输入序列: ";
        print_complex_vector("", input_data);
        
        // 映射输入数据到硬件
        vector<complex<float>> mapped_input = map_input_data(input_data, hardware_size, test_size);
        
        // 分配到A和B路径
        unsigned num_pes = hardware_size / 2;
        vector<complex<float>> input_a(num_pes), input_b(num_pes);
        for (unsigned i = 0; i < num_pes; ++i) {
            input_a[i] = mapped_input[i];
            input_b[i] = mapped_input[i + num_pes];
        }
        
        // 写入输入数据
        write_data(0x1000, input_a);
        write_data(0x2000, input_b);
        
        // 启动FFT
        write_register(0x0000, 0x06);  // fft_mode=1, start=1
        
        // 等待完成
        bool done = false;
        int timeout = 2000;
        while (!done && timeout > 0) {
            wait(10, SC_NS);
            uint32_t status = read_register(0x0010);
            done = (status & 0x02) != 0;
            timeout--;
            if (timeout % 200 == 0) {
                cout << "等待FFT完成，状态: 0x" << hex << status << dec << " (超时: " << timeout << ")" << endl;
            }
        }
        
        if (done) {
            cout << test_size << "点FFT计算完成！" << endl;
        } else {
            cout << test_size << "点FFT计算未在预期时间内完成，继续读取输出..." << endl;
        }
        
        // 读取输出数据
        vector<complex<float>> output_y0 = read_data(0x3000, num_pes);
        vector<complex<float>> output_y1 = read_data(0x4000, num_pes);
        
        cout << "PE输出Y0: ";
        print_complex_vector("", output_y0);
        cout << "PE输出Y1: ";
        print_complex_vector("", output_y1);
        
        // 提取测试结果
        vector<complex<float>> fft_result = extract_output_data(output_y0, output_y1, test_size, hardware_size);
        
        cout << "提取的" << test_size << "点FFT结果: ";
        print_complex_vector("", fft_result);
        
        // 验证结果
        test_passed = verify_fft_result(fft_result, input_data, test_size);
        
    } catch (const exception& e) {
        cout << test_name << "异常: " << e.what() << endl;
        test_passed = false;
    }
    
    print_test_result(test_name, test_passed);
    wait(100, SC_NS);
}

bool FftTlmTestbench::verify_fft_result(const vector<complex<float>>& fft_output,
                                       const vector<complex<float>>& input_data,
                                       unsigned size, float tolerance) {
    cout << "开始FFT结果验证..." << endl;
    
    // 计算参考DFT
    vector<complex<float>> reference_dft = FFTTestUtils::compute_reference_dft(input_data);
    
    cout << "参考DFT结果: ";
    print_complex_vector("", reference_dft);
    
    // 比较结果
    bool passed = FFTTestUtils::compare_complex_sequences(fft_output, reference_dft, tolerance, true);
    
    cout << "FFT验证结果: " << (passed ? "通过" : "失败") << endl;
    return passed;
}

// TLM传输辅助方法实现
void FftTlmTestbench::write_register(uint32_t addr, uint32_t data) {
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(sizeof(uint32_t));
    trans.set_streaming_width(sizeof(uint32_t));
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    init_socket->b_transport(trans, delay);
    wait(delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        cout << "写寄存器失败: 地址=0x" << hex << addr << ", 数据=0x" << data << dec << endl;
    }
}

uint32_t FftTlmTestbench::read_register(uint32_t addr) {
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t data = 0;
    
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(sizeof(uint32_t));
    trans.set_streaming_width(sizeof(uint32_t));
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    init_socket->b_transport(trans, delay);
    wait(delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        cout << "读寄存器失败: 地址=0x" << hex << addr << dec << endl;
        return 0;
    }
    
    return data;
}

void FftTlmTestbench::write_data(uint32_t addr, const vector<complex<float>>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        
        complex<float> temp_data = data[i];
        
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(addr + i * sizeof(complex<float>));
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&temp_data));
        trans.set_data_length(sizeof(complex<float>));
        trans.set_streaming_width(sizeof(complex<float>));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
        
        init_socket->b_transport(trans, delay);
        wait(delay);
    }
}

vector<complex<float>> FftTlmTestbench::read_data(uint32_t addr, size_t count) {
    vector<complex<float>> result(count);
    
    for (size_t i = 0; i < count; ++i) {
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        
        trans.set_command(TLM_READ_COMMAND);
        trans.set_address(addr + i * sizeof(complex<float>));
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&result[i]));
        trans.set_data_length(sizeof(complex<float>));
        trans.set_streaming_width(sizeof(complex<float>));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
        
        init_socket->b_transport(trans, delay);
        wait(delay);
    }
    
    return result;
}

// 辅助打印方法
void FftTlmTestbench::print_test_header(const string& test_name) {
    cout << "\n----------------------------------------" << endl;
    cout << "开始测试: " << test_name << endl;
    cout << "----------------------------------------" << endl;
}

void FftTlmTestbench::print_test_result(const string& test_name, bool passed) {
    cout << "测试结果: " << test_name << " - " 
         << (passed ? "通过" : "失败") << endl;
}

void FftTlmTestbench::print_complex_vector(const string& name, const vector<complex<float>>& data) {
    if (!name.empty()) cout << name << ": ";
    for (const auto& val : data) {
        cout << val << " ";
    }
    cout << endl;
}

/**
 * @brief 参数化的顶层测试模块
 */
template<unsigned HW_SIZE>
SC_MODULE(TestTopFlexible) {
    FftTlm<float, HW_SIZE>* dut;
    FftTlmTestbench* testbench;
    
    SC_CTOR(TestTopFlexible) {
        // 创建被测设备
        string dut_name = "dut_" + to_string(HW_SIZE) + "pt";
        dut = new FftTlm<float, HW_SIZE>(dut_name.c_str());
        
        // 创建测试台
        testbench = new FftTlmTestbench("testbench");
        
        // 连接TLM socket
        testbench->init_socket.bind(dut->tgt_socket);
        
        cout << "创建" << HW_SIZE << "点硬件FFT测试系统" << endl;
    }
    
    void set_test_size(unsigned test_size) {
        testbench->set_test_config(HW_SIZE, test_size);
    }
};

int sc_main(int argc, char* argv[]) {
    cout << "SystemC FFT TLM2.0 精简测试程序启动" << endl;
    cout << "SystemC 版本: " << sc_version() << endl;
    
    // 默认配置
    unsigned hardware_size = 16;
    unsigned test_size = 4;
    
    // 解析命令行参数
    if (argc > 1) {
        hardware_size = atoi(argv[1]);
        if (argc > 2) {
            test_size = atoi(argv[2]);
        }
    }
    
    // 验证参数
    if ((hardware_size & (hardware_size - 1)) != 0 || (test_size & (test_size - 1)) != 0) {
        cout << "错误: FFT点数必须是2的幂次" << endl;
        return -1;
    }
    
    if (test_size > hardware_size) {
        cout << "错误: 测试点数(" << test_size << ") 不能大于硬件点数(" << hardware_size << ")" << endl;
        return -1;
    }
    
    cout << "测试配置: " << test_size << "点FFT on " << hardware_size << "点硬件" << endl;
    
    // 根据硬件配置创建相应的测试模块
    switch (hardware_size) {
    case 4: {
        TestTopFlexible<4> top("top");
        top.set_test_size(test_size);
        sc_start();
        break;
    }
    case 8: {
        TestTopFlexible<8> top("top");
        top.set_test_size(test_size);
        sc_start();
        break;
    }
    case 16: {
        TestTopFlexible<16> top("top");
        top.set_test_size(test_size);
        sc_start();
        break;
    }
    case 32: {
        TestTopFlexible<32> top("top");
        top.set_test_size(test_size);
        sc_start();
        break;
    }
    case 64: {
        TestTopFlexible<64> top("top");
        top.set_test_size(test_size);
        sc_start();
        break;
    }
    default:
        cout << "错误: 不支持的硬件点数 " << hardware_size << endl;
        cout << "支持的硬件点数: 4, 8, 16, 32, 64" << endl;
        return -1;
    }
    
    cout << "\n========================================" << endl;
    cout << "FFT TLM2.0 精简测试程序结束" << endl;
    cout << "测试配置: " << test_size << "点FFT on " << hardware_size << "点硬件" << endl;
    cout << "========================================" << endl;
    
    return 0;
}