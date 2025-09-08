/**
 * @file matrix_test_utils.h
 * @brief 统一矩阵测试工具库 - Think Ultra瘦身版
 * 
 * 🚀 Think Ultra 精简设计：
 * 1. 仅保留核心数据结构和类型定义
 * 2. 删除未使用的工具类和方法
 * 3. 专注于支持分块GEMM计算
 */

#ifndef MATRIX_TEST_UTILS_H
#define MATRIX_TEST_UTILS_H

#include <random>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <vector>
#include <memory>

/**
 * @brief 矩阵测试配置结构 - 精简版
 */
struct MatrixTestConfig {
    // 数据类型配置
    enum DataType {
        INTEGER_TYPE = 0,   // 整数型测试数据
        DECIMAL_TYPE = 1    // 小数型测试数据
    };
};

/**
 * @brief 多帧测试配置结构 - 精简版
 */
struct FrameTestConfig {
    // 测试模式枚举
    enum TestMode {
        FIXED_MULTI_FRAME = 0,      // N帧固定尺寸测试
        VARIABLE_MULTI_FRAME = 1,   // N帧变长矩阵测试
        MIXED_MULTI_FRAME = 2       // N帧混合尺寸测试
    };
    
    // 基础配置
    int frame_count = 4;
    TestMode test_mode = FIXED_MULTI_FRAME;
};

/**
 * @brief 单帧配置结构 - 精简版
 */
struct SingleFrameConfig {
    // 矩阵尺寸
    int M = 16, K = 16, N = 16;
    bool is_variable_size = false;
    
    // 数据类型
    MatrixTestConfig::DataType data_type = MatrixTestConfig::DataType::INTEGER_TYPE;
    int seed_offset = 0;
    
    // 构造函数 - 固定尺寸
    explicit SingleFrameConfig(int size = 16) : M(size), K(size), N(size), is_variable_size(false) {}
    
    // 构造函数 - 变长尺寸
    SingleFrameConfig(int m, int k, int n) : M(m), K(k), N(n), is_variable_size(true) {}
};

/**
 * @brief 统一矩阵数据集结构
 */
template<int MAX_SIZE>
struct MatrixSet {
    // 实际矩阵尺寸
    int M, K, N;
    bool is_variable_size;  // 标识是否为变长矩阵
    
    // 动态存储（用于变长矩阵）
    std::vector<float> A_data, B_data, D_data, C_data, C_expected;
    
    // 固定存储（用于兼容原有接口）
    float A_fixed[MAX_SIZE][MAX_SIZE];
    float B_fixed[MAX_SIZE][MAX_SIZE];
    float D_fixed[MAX_SIZE][MAX_SIZE];
    float C_fixed[MAX_SIZE][MAX_SIZE];
    float C_expected_fixed[MAX_SIZE][MAX_SIZE];
    
    // 构造函数 - 固定尺寸矩阵
    explicit MatrixSet(int size = MAX_SIZE) : M(size), K(size), N(size), is_variable_size(false) {
        memset(A_fixed, 0, sizeof(A_fixed));
        memset(B_fixed, 0, sizeof(B_fixed));
        memset(D_fixed, 0, sizeof(D_fixed));
        memset(C_fixed, 0, sizeof(C_fixed));
        memset(C_expected_fixed, 0, sizeof(C_expected_fixed));
    }
    
    // 构造函数 - 变长尺寸矩阵
    MatrixSet(int m, int k, int n) : M(m), K(k), N(n), is_variable_size(true) {
        A_data.resize(M * K);
        B_data.resize(K * N);
        D_data.resize(M * N);
        C_data.resize(M * N);
        C_expected.resize(M * N);
    }
    
    // 🚀 统一矩阵元素访问接口
private:
    template<typename VectorType, typename ArrayType>
    float& matrix_element_impl(VectorType& vec, ArrayType& arr, int i, int j, int stride) {
        return is_variable_size ? vec[i * stride + j] : arr[i][j];
    }
    
    template<typename VectorType, typename ArrayType>
    const float& matrix_element_impl(const VectorType& vec, const ArrayType& arr, int i, int j, int stride) const {
        return is_variable_size ? vec[i * stride + j] : arr[i][j];
    }

public:
    // 矩阵元素访问器（非const版本）
    float& A(int i, int j) { return matrix_element_impl(A_data, A_fixed, i, j, K); }
    float& B(int i, int j) { return matrix_element_impl(B_data, B_fixed, i, j, N); }
    float& D(int i, int j) { return matrix_element_impl(D_data, D_fixed, i, j, N); }
    float& C(int i, int j) { return matrix_element_impl(C_data, C_fixed, i, j, N); }
    float& C_exp(int i, int j) { return matrix_element_impl(C_expected, C_expected_fixed, i, j, N); }
    
    // 矩阵元素访问器（const版本）
    const float& A(int i, int j) const { return matrix_element_impl(A_data, A_fixed, i, j, K); }
    const float& B(int i, int j) const { return matrix_element_impl(B_data, B_fixed, i, j, N); }
    const float& D(int i, int j) const { return matrix_element_impl(D_data, D_fixed, i, j, N); }
    const float& C(int i, int j) const { return matrix_element_impl(C_data, C_fixed, i, j, N); }
    const float& C_exp(int i, int j) const { return matrix_element_impl(C_expected, C_expected_fixed, i, j, N); }
    
    // 获取原始指针（用于与TLM接口兼容）
    float* A_ptr() {
        return is_variable_size ? A_data.data() : &A_fixed[0][0];
    }
    
    float* B_ptr() {
        return is_variable_size ? B_data.data() : &B_fixed[0][0];
    }
    
    float* D_ptr() {
        return is_variable_size ? D_data.data() : &D_fixed[0][0];
    }
    
    float* C_ptr() {
        return is_variable_size ? C_data.data() : &C_fixed[0][0];
    }
    
    float* C_expected_ptr() {
        return is_variable_size ? C_expected.data() : &C_expected_fixed[0][0];
    }
};

/**
 * @brief 多帧矩阵数据集 - 管理N个MatrixSet的容器
 * @tparam MAX_SIZE 最大支持的矩阵尺寸
 */
template<int MAX_SIZE>
class MultiFrameMatrixSet {
public:
    using MatrixSetType = MatrixSet<MAX_SIZE>;
    using Config = FrameTestConfig;
    
    // 多帧数据存储
    std::vector<std::unique_ptr<MatrixSetType>> frames;
    std::vector<SingleFrameConfig> frame_configs;
    
    // 全局配置
    Config global_config;
    
    // 构造函数
    explicit MultiFrameMatrixSet(const Config& config = Config()) 
        : global_config(config) {
        frames.reserve(config.frame_count);
        frame_configs.reserve(config.frame_count);
    }
    
    // 获取帧数
    int get_frame_count() const { return frames.size(); }
    
    // 获取指定帧
    MatrixSetType* get_frame(int frame_id) {
        if (frame_id >= 0 && frame_id < frames.size()) {
            return frames[frame_id].get();
        }
        return nullptr;
    }
    
    const MatrixSetType* get_frame(int frame_id) const {
        if (frame_id >= 0 && frame_id < frames.size()) {
            return frames[frame_id].get();
        }
        return nullptr;
    }
    
    // 添加帧数据
    void add_frame(std::unique_ptr<MatrixSetType> matrix_set, const SingleFrameConfig& config) {
        frames.push_back(std::move(matrix_set));
        frame_configs.push_back(config);
    }
    
    // 清空所有帧数据
    void clear() {
        frames.clear();
        frame_configs.clear();
    }
};

#endif // MATRIX_TEST_UTILS_H