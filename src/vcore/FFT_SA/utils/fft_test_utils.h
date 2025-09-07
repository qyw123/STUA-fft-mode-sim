/**
 * @file fft_test_utils.h
 * @brief FFT测试工具函数库 - 通用化数据生成和验证工具集
 * 
 * 提供FFT测试中常用的功能：
 * - 通用N点FFT Twiddle因子生成和加载
 * - 可配置的测试数据生成（随机、固定、脉冲等）
 * - FFT结果验证（标准DFT参考）
 * - 输出采样和统计
 * - 波形跟踪设置
 * - 支持任意规模的FFT测试，取消硬编码限制
 * 
 * @version 2.0 - 通用化重构版
 * @date 2025-08-30
 */

#ifndef FFT_TEST_UTILS_H
#define FFT_TEST_UTILS_H

#include "systemc.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>
#include "complex_types.h"


// ====== 常量定义 ======
static constexpr double PI = 3.14159265358979323846;

// ====== 通用FFT测试数据生成器 ======
namespace FFTTestUtils {

// 数据生成器类型枚举
enum class DataGenType {
    SEQUENTIAL,    // 顺序数据: (1,1), (2,2), ...
    RANDOM,        // 随机数据: 1-10范围内的浮点数
    IMPULSE,       // 脉冲数据: δ[n]
    ONES,          // 全1数据: (1,1), (1,1), ...
    ZEROS          // 全0数据: (0,0), (0,0), ...
};

/**
 * @brief 通用N点FFT测试序列生成器
 * @param N 序列长度
 * @param gen_type 生成类型
 * @param start_value 起始值（用于顺序生成）
 * @param impulse_pos 脉冲位置（用于脉冲生成）
 * @param seed 随机种子（用于随机生成，0表示使用时间种子）
 * @return 返回N点测试序列
 */
inline vector<complex<float>> generate_test_sequence(unsigned N, 
                                                     DataGenType gen_type = DataGenType::SEQUENTIAL,
                                                     int start_value = 1,
                                                     unsigned impulse_pos = 0,
                                                     unsigned seed = 0) {
    vector<complex<float>> sequence(N);
    
    switch (gen_type) {
        case DataGenType::SEQUENTIAL:
            for (unsigned i = 0; i < N; i++) {
                sequence[i] = complex<float>(start_value + i, start_value + i);
            }
            break;
            
        case DataGenType::RANDOM: {
            // 设置随机数生成器
            mt19937 gen(seed == 0 ? chrono::high_resolution_clock::now().time_since_epoch().count() : seed);
            uniform_real_distribution<float> dist(1.0f, 10.0f);
            
            for (unsigned i = 0; i < N; i++) {
                sequence[i] = complex<float>(dist(gen), dist(gen));
            }
            break;
        }
        
        case DataGenType::IMPULSE:
            for (unsigned i = 0; i < N; i++) {
                sequence[i] = complex<float>(0, 0);
            }
            if (impulse_pos < N) {
                sequence[impulse_pos] = complex<float>(1, 0);
            }
            break;
            
        case DataGenType::ONES:
            for (unsigned i = 0; i < N; i++) {
                sequence[i] = complex<float>(1, 1);
            }
            break;
            
        case DataGenType::ZEROS:
            for (unsigned i = 0; i < N; i++) {
                sequence[i] = complex<float>(0, 0);
            }
            break;
    }
    
    return sequence;
}





// ====== 通用Twiddle因子生成和加载 ======

/**
 * @brief 计算标准FFT的Twiddle因子
 * @param N FFT大小
 * @param k Twiddle索引
 * @return W_N^k = e^(-j*2π*k/N)
 */
inline complex<float> compute_twiddle_factor(unsigned N, unsigned k) {
    double angle = -2.0 * PI * k / N;
    return complex<float>(cos(angle), sin(angle));
}

/**
 * @brief 通用N点FFT多级Twiddle因子生成器
 * @param N FFT大小（必须是2的幂）
 * @param num_stages 级数 (log2(N))
 * @param num_pes PE数量 (N/2)
 * @param bypass_stages 需要bypass的级数（从stage 0开始计数）
 * @return 返回各级的Twiddle因子配置
 */
inline vector<vector<complex<float>>> generate_fft_twiddles(unsigned N, 
                                                           unsigned num_stages = 0,
                                                           unsigned num_pes = 0,
                                                           unsigned bypass_stages = 0) {
    // 自动计算参数
    if (num_stages == 0) {
        num_stages = static_cast<unsigned>(log2(N));
    }
    if (num_pes == 0) {
        num_pes = N / 2;
    }
    
    // 有效级数（减去bypass的级数）
    unsigned effective_stages = num_stages - bypass_stages;
    vector<vector<complex<float>>> twiddles(effective_stages);
    
    // 为每个有效stage生成twiddle因子
    for (unsigned stage = 0; stage < effective_stages; stage++) {
        unsigned actual_stage = stage + bypass_stages;  // 实际的stage编号
        twiddles[stage].resize(num_pes);
        
        // 计算当前stage的twiddle因子步长
        unsigned step = 1 << actual_stage;  // 2^stage
        unsigned group_size = N / step;     // 每组的大小
        
        for (unsigned pe = 0; pe < num_pes; pe++) {
            // 计算当前PE对应的twiddle索引
            unsigned twiddle_idx;
            if (actual_stage == 0) {
                // Stage 0: 每个PE有不同的twiddle
                twiddle_idx = pe;
            } else {
                // 后续stage: 按group分配twiddle
                unsigned group = pe / (group_size / 2);
                twiddle_idx = (pe % (group_size / 2)) * step;
            }
            
            // 使用模运算确保索引在有效范围内
            twiddle_idx %= N;
            
            twiddles[stage][pe] = compute_twiddle_factor(N, twiddle_idx);
        }
    }
    
    return twiddles;
}

// ====== 通用DFT参考计算 ======

/**
 * @brief 计算N点标准DFT作为参考
 * @param input 输入复数序列
 * @return DFT输出序列
 */
inline vector<complex<float>> compute_reference_dft(const vector<complex<float>>& input) {
    unsigned N = input.size();
    vector<complex<float>> output(N);
    
    for (unsigned k = 0; k < N; k++) {
        output[k] = complex<float>(0, 0);
        for (unsigned n = 0; n < N; n++) {
            float angle = -2.0 * PI * k * n / N;
            complex<float> twiddle(cos(angle), sin(angle));
            output[k] += input[n] * twiddle;
        }
    }
    
    return output;
}

/**
 * @brief 通用N点FFT参考计算函数
 * @param input 输入序列（支持ComplexT和complex<float>）
 * @param N 期望的FFT大小（用于验证）
 * @return DFT输出序列
 */
template<typename InputType>
inline vector<complex<float>> compute_reference_fft(const vector<InputType>& input, unsigned N = 0) {
    if (N > 0 && input.size() != N) {
        cout << "Error: compute_reference_fft expected " << N << " points, got " << input.size() << "\n";
        return vector<complex<float>>(N > 0 ? N : input.size(), complex<float>(0, 0));
    }
    return compute_reference_dft(input);
}



// ====== 结果验证工具 ======

/**
 * @brief 比较两个复数序列是否在容差范围内相等
 * @param actual 实际结果
 * @param expected 期望结果
 * @param tolerance 容差
 * @param verbose 是否显示详细比较信息
 * @return 验证是否通过
 */
inline bool compare_complex_sequences(const vector<complex<float>>& actual,
                                     const vector<complex<float>>& expected,
                                     float tolerance = 1e-3f,
                                     bool verbose = false) {
    if (actual.size() != expected.size()) {
        if (verbose) {
            cout << "Error: Sequence size mismatch: " << actual.size() 
                 << " vs " << expected.size() << "\n";
        }
        return false;
    }
    
    bool all_match = true;
    
    if (verbose) {
        cout << " Comparison Results:\n";
    }
    
    for (size_t i = 0; i < actual.size(); i++) {
        float diff_real = abs(actual[i].real - expected[i].real);
        float diff_imag = abs(actual[i].imag - expected[i].imag);
        bool match = (diff_real < tolerance) && (diff_imag < tolerance);
        all_match &= match;
        
        if (verbose) {
            cout << "  [" << i << "] Actual=(" << fixed << setprecision(3) 
                 << actual[i].real << "," << actual[i].imag << ") "
                 << "Expected=(" << expected[i].real << "," << expected[i].imag << ") "
                 << "Diff=(" << diff_real << "," << diff_imag << ") "
                 << (match ? "PASS" : "FAIL") << "\n";
        }
    }
    
    return all_match;
}

/**
 * @brief 通用PE输出映射到自然顺序的工具函数
 * @param pe_y0 PE输出Y0
 * @param pe_y1 PE输出Y1
 * @param N FFT大小
 * @return 自然顺序的FFT输出
 */
inline vector<complex<float>> map_pe_output_to_natural_order(
    const vector<complex<float>>& pe_y0,
    const vector<complex<float>>& pe_y1,
    unsigned N) {
    
    unsigned expected_pes = N / 2;
    if (pe_y0.size() != expected_pes || pe_y1.size() != expected_pes) {
        cout << "Error: " << N << "-point FFT requires " << expected_pes << " PEs, got " 
             << pe_y0.size() << " and " << pe_y1.size() << "\n";
        return vector<complex<float>>(N, complex<float>(0, 0));
    }
    
    vector<complex<float>> output(N);
    
    // DIF FFT的标准输出映射：Y0输出在前半部，Y1输出在后半部
    for (unsigned pe = 0; pe < expected_pes; pe++) {
        output[pe] = complex<float>(pe_y0[pe].real, pe_y0[pe].imag);                    // PE_Y0
        output[pe + expected_pes] = complex<float>(pe_y1[pe].real, pe_y1[pe].imag);     // PE_Y1
    }
    
    return output;
}



// ====== 测试统计和输出工具 ======
/**
 * @brief 计算有效输出数据的数量
 * @param valid_flags 有效标志位数组
 * @return 有效数据的数量
 */
inline int count_valid_outputs(const vector<bool>& valid_flags) {
    return count(valid_flags.begin(), valid_flags.end(), true);
}

/**
 * @brief 打印测试结果统计
 * @param test_name 测试名称
 * @param passed 通过的测试数
 * @param total 总测试数
 */
inline void print_test_results(const string& test_name, int passed, int total) {
    cout << "\n" << test_name << " Test Results\n";
    cout << string(test_name.length() + 13, '=') << "\n";
    cout << "Total Tests: " << total << "\n";
    cout << "Passed: " << passed << "\n";
    cout << "Failed: " << (total - passed) << "\n";
    cout << "Success Rate: " << fixed << setprecision(1) 
         << (100.0 * passed / total) << "%\n";
    
    if (passed == total) {
        cout << "\n✅ ALL TESTS PASSED! " << test_name << " Verified!\n";
    } else {
        cout << "\n❌ Some tests failed. Please review the results above.\n";
    }
}

/**
 * @brief 显示复数序列（用于调试）
 * @param sequence 复数序列
 * @param label 标签
 * @param precision 小数精度
 */
inline void display_complex_sequence(const vector<complex<float>>& sequence, 
                                   const string& label = "Sequence",
                                   int precision = 3) {
    cout << "  " << label << ": ";
    for (const auto& val : sequence) {
        cout << "(" << fixed << setprecision(precision) 
             << val.real << "," << val.imag << ") ";
    }
    cout << "\n";
}

/**
 * @brief 显示标准复数序列（用于调试）
 * @param sequence 标准复数序列
 * @param label 标签
 * @param precision 小数精度
 */
inline void display_std_complex_sequence(const vector<complex<float>>& sequence, 
                                        const string& label = "Sequence",
                                        int precision = 3) {
    cout << "  " << label << ": ";
    for (const auto& val : sequence) {
        cout << "(" << fixed << setprecision(precision) 
             << val.real << "," << val.imag << ") ";
    }
    cout << "\n";
}

// ====== 波形跟踪工具 ======

/**
 * @brief 设置基本的FFT测试波形跟踪
 * @param tf SystemC跟踪文件指针
 * @param clk 时钟信号
 * @param rst 复位信号
 * @param fft_mode FFT模式信号
 * @param bypass_en bypass使能信号
 */
template<typename ClockType, typename ResetType, typename ModeType, typename BypassType>
inline void setup_basic_fft_trace(sc_trace_file* tf,
                                 ClockType& clk,
                                 ResetType& rst,
                                 ModeType& fft_mode,
                                 BypassType& bypass_en) {
    sc_trace(tf, clk, "clk");
    sc_trace(tf, rst, "rst_i");
    sc_trace(tf, fft_mode, "fft_mode_i");
    sc_trace(tf, bypass_en, "stage_bypass_en");
}

/**
 * @brief 设置Twiddle接口的波形跟踪
 */
template<typename LoadEnType, typename StageIdxType, typename PeIdxType, typename TwDataType>
inline void setup_twiddle_trace(sc_trace_file* tf,
                               LoadEnType& tw_load_en,
                               StageIdxType& tw_stage_idx,
                               PeIdxType& tw_pe_idx,
                               TwDataType& tw_data) {
    sc_trace(tf, tw_load_en, "tw_load_en");
    sc_trace(tf, tw_stage_idx, "tw_stage_idx");
    sc_trace(tf, tw_pe_idx, "tw_pe_idx");
    sc_trace(tf, tw_data, "tw_data");
}

/**
 * @brief 设置数据通路的波形跟踪
 */
template<typename DataVectorType, typename ValidVectorType>
inline void setup_data_path_trace(sc_trace_file* tf,
                                 DataVectorType& in_a, DataVectorType& in_b,
                                 DataVectorType& out_y0, DataVectorType& out_y1,
                                 ValidVectorType& in_a_v, ValidVectorType& in_b_v,
                                 ValidVectorType& out_y0_v, ValidVectorType& out_y1_v,
                                 unsigned num_pes) {
    // 输入数据信号
    for (unsigned k = 0; k < num_pes; k++) {
        sc_trace(tf, in_a[k], ("in_a_" + to_string(k)).c_str());
        sc_trace(tf, in_b[k], ("in_b_" + to_string(k)).c_str());
        sc_trace(tf, in_a_v[k], ("in_a_v_" + to_string(k)).c_str());
        sc_trace(tf, in_b_v[k], ("in_b_v_" + to_string(k)).c_str());
    }
    
    // 输出数据信号
    for (unsigned k = 0; k < num_pes; k++) {
        sc_trace(tf, out_y0[k], ("out_y0_" + to_string(k)).c_str());
        sc_trace(tf, out_y1[k], ("out_y1_" + to_string(k)).c_str());
        sc_trace(tf, out_y0_v[k], ("out_y0_v_" + to_string(k)).c_str());
        sc_trace(tf, out_y1_v[k], ("out_y1_v_" + to_string(k)).c_str());
    }
}

// ====== PEA_FFT特定的缓冲区工具函数 ======

/**
 * @brief 将8点FFT输入映射到16路浮点数输入（PEA_FFT缓冲区格式）
 * @param input_data 8点复数输入
 * @param float_array 16路浮点数输出数组
 */
inline void map_complex_input_to_T_float(int N, const vector<complex<float>>& input_data,
                                        vector<float>& float_array) {
    if (input_data.size() != N || float_array.size() != 2*N) {
        cout << "Error: map_complex_input_to_T_float size mismatch\n";
        cout << "input_data.size()=" << input_data.size() << endl;
        cout << "float_array.size()=" << float_array.size() << endl;
        return;
    }
    
    // Group0: X[0-3] -> FIFO[0-3] (real), FIFO[4-7] (imag)  
    for (unsigned i = 0; i < N/2; ++i) {
        float_array[i] = input_data[i].real;      // Real part: X[0-3] -> FIFO[0-3]
        float_array[i + N/2] = input_data[i].imag;  // Imag part: X[0-3] -> FIFO[4-7]
    }
    
    // Group1: X[4-7] -> FIFO[8-11] (real), FIFO[12-15] (imag)
    for (unsigned i = N/2; i < N; ++i) {
        float_array[i + N/2] = input_data[i].real;  // Real part: X[4-7] -> FIFO[8-11]
        float_array[i + N] = input_data[i].imag;  // Imag part: X[4-7] -> FIFO[12-15]
    }
}

/**
 * @brief 从16路并行浮点输出重构8点复数输出
 * @param parallel_output 16路并行浮点数输出
 * @return 8点复数输出
 */
inline vector<complex<float>> reconstruct_complex_from_T_parallel(int N, const vector<float>& parallel_output) {
    // if (parallel_output.size() != 2*N) {
    //     cout << "Error: reconstruct_complex_from_T_parallel requires 2*N("<< N << ") float inputs\n";
    //     return vector<complex<float>>(8, complex<float>(0, 0));
    // }
    // vector<complex<float>> result(N);
    // for (unsigned i = 0; i < N; ++i) {
    //     result[i].real = parallel_output[i];
    //     // cout << "parallel_output[" << i << "]=" <<  parallel_output[i] << endl;  
    //     cout << "result[" << i << "].real=" <<  parallel_output[i] << endl;      
    //     result[i].imag = parallel_output[i + N];    
    //     cout << "result[" << i << "].imag="  <<  parallel_output[i+ N] << endl;  
    // }

    vector<complex<float>> result(N);
    for (unsigned i = 0; i < N; ++i) {
        result[i] = complex<float>(parallel_output[i], parallel_output[i + N]);
    }
    cout << "reconstruct_complex_from_T_parallel success" << endl;

    return result;
}


/**
 * @brief 设置PEA_FFT缓冲区接口的波形跟踪
 */
template<typename TraceFile, typename DataVecType, typename ValidVecType, 
         typename ControlType1, typename ControlType2, typename ControlType3>
inline void setup_pea_fft_buffer_trace(TraceFile* tf,
                                      DataVecType& data_i_vec, DataVecType& data_o_vec,
                                      ValidVecType& wr_ready_vec, ValidVecType& rd_valid_vec,
                                      ControlType1& wr_start, ControlType1& wr_en,
                                      ControlType2& input_ready, ControlType2& input_empty,
                                      ControlType3& rd_start, ControlType2& output_ready, ControlType2& output_empty,
                                      unsigned num_trace_channels = 8) {
    // Input buffer control
    sc_trace(tf, wr_start, "wr_start_i");
    sc_trace(tf, wr_en, "wr_en_i");
    sc_trace(tf, input_ready, "input_ready_o");
    sc_trace(tf, input_empty, "input_empty_o");
    
    // Output buffer control
    sc_trace(tf, rd_start, "rd_start_i");
    sc_trace(tf, output_ready, "output_ready_o");
    sc_trace(tf, output_empty, "output_empty_o");
    
    // Sample data (limited to reduce waveform size)
    for (unsigned i = 0; i < num_trace_channels; ++i) {
        sc_trace(tf, data_i_vec[i], ("data_i_" + to_string(i)).c_str());
        sc_trace(tf, wr_ready_vec[i], ("wr_ready_" + to_string(i)).c_str());
        sc_trace(tf, data_o_vec[i], ("data_o_" + to_string(i)).c_str());
        sc_trace(tf, rd_valid_vec[i], ("rd_valid_" + to_string(i)).c_str());
    }
}

// ====== 通用时序工具函数 ======

/**
 * @brief 通用时钟周期等待函数
 * @param cycles 等待的时钟周期数
 * @param clock_period 时钟周期（默认为1ns）
 */
inline void wait_cycles(int cycles, sc_time clock_period = sc_time(1, SC_NS)) {
    if (cycles > 0) {
        wait(clock_period * cycles);
    }
}

} // namespace FFTTestUtils

#endif // FFT_TEST_UTILS_H