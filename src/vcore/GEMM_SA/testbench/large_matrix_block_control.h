/**
 * @file large_matrix_block_control.h
 * @brief 大矩阵分块控制器 - 支持超过MAX_SIZE的GEMM计算
 * 
 * 🚀 Think Ultra 大矩阵架构设计：
 * 1. 智能分块算法：当M/K/N任一维度超过MAX_SIZE时自动分块
 * 2. 多帧流水线集成：将分块转换为现有的多帧序列处理
 * 3. 高效内存管理：最小化数据复制，优化内存访问模式
 * 4. 结果重构：准确合并分块结果为完整的大矩阵结果
 * 5. 性能分析：扩展现有性能统计支持大矩阵操作
 * 
 * 兼容性保证：
 * - 不修改src_gemm/路径下的任何硬件IP设计
 * - 完全兼容现有的matrix_test_utils.h和gemm_pingpong_test.cpp
 * - 纯头文件实现，无需修改构建系统
 */

#ifndef LARGE_MATRIX_BLOCK_CONTROL_H
#define LARGE_MATRIX_BLOCK_CONTROL_H

#include <vector>
#include <memory>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cmath>

// 依赖现有的矩阵测试工具
#include "matrix_test_utils.h"

/**
 * @brief 分块操作类型枚举
 */
enum class BlockType {
    SIMPLE_ROW_BLOCK = 0,       // 简单行分块 (M > MAX_SIZE)
    SIMPLE_COL_BLOCK = 1,       // 简单列分块 (N > MAX_SIZE)  
    INNER_DIM_BLOCK = 2,        // 内部维度分块 (K > MAX_SIZE)
    MIXED_2D_BLOCK = 3,         // 2D混合分块 (M,N > MAX_SIZE)
    MIXED_3D_BLOCK = 4          // 3D全维分块 (M,K,N > MAX_SIZE)
};

/**
 * @brief 单个分块信息结构
 */
struct BlockInfo {
    // 分块尺寸 (保证 ≤ MAX_SIZE)
    int block_M, block_K, block_N;
    
    // 在原始大矩阵中的偏移位置
    int offset_M, offset_K, offset_N;
    
    // 分块类型和索引
    BlockType type;
    int block_index;                    // 当前分块在总序列中的索引
    
    // 计算控制标志
    bool requires_accumulation;         // 是否需要累加到结果矩阵
    bool is_first_k_block;             // K维分块时的第一块标记
    bool is_last_k_block;              // K维分块时的最后一块标记
    
    // 构造函数
    BlockInfo() : block_M(0), block_K(0), block_N(0), 
                 offset_M(0), offset_K(0), offset_N(0),
                 type(BlockType::SIMPLE_ROW_BLOCK), block_index(0),
                 requires_accumulation(false), is_first_k_block(true), is_last_k_block(true) {}
    
    BlockInfo(int bM, int bK, int bN, int oM, int oK, int oN, BlockType t, int idx) 
        : block_M(bM), block_K(bK), block_N(bN),
          offset_M(oM), offset_K(oK), offset_N(oN),
          type(t), block_index(idx),
          requires_accumulation(false), is_first_k_block(true), is_last_k_block(true) {}
    
    // 打印分块信息
    void print_info() const {
        std::cout << "  Block[" << block_index << "]: [" << block_M << "×" << block_K << "×" << block_N 
                  << "] at offset (" << offset_M << "," << offset_K << "," << offset_N << ")";
        switch(type) {
            case BlockType::SIMPLE_ROW_BLOCK: std::cout << " (行分块)"; break;
            case BlockType::SIMPLE_COL_BLOCK: std::cout << " (列分块)"; break;
            case BlockType::INNER_DIM_BLOCK: std::cout << " (内维分块)"; break;
            case BlockType::MIXED_2D_BLOCK: std::cout << " (2D分块)"; break;
            case BlockType::MIXED_3D_BLOCK: std::cout << " (3D分块)"; break;
        }
        if (requires_accumulation) std::cout << " [需累加]";
        std::cout << std::endl;
    }
};

/**
 * @brief 分块执行计划结构
 */
struct BlockPlan {
    // 原始大矩阵尺寸
    int original_M, original_K, original_N;
    
    // 分块策略信息
    BlockType primary_strategy;         // 主要分块策略
    int total_blocks;                   // 总分块数量
    
    // 分块尺寸统计
    int num_blocks_M, num_blocks_K, num_blocks_N;  // 各维度分块数量
    int optimal_block_M, optimal_block_K, optimal_block_N;  // 最优分块尺寸
    
    // 分块执行序列
    std::vector<BlockInfo> block_sequence;
    
    
    // 构造函数
    BlockPlan() : original_M(0), original_K(0), original_N(0),
                 primary_strategy(BlockType::SIMPLE_ROW_BLOCK), total_blocks(0),
                 num_blocks_M(1), num_blocks_K(1), num_blocks_N(1),
                 optimal_block_M(16), optimal_block_K(16), optimal_block_N(16) {}
    
    BlockPlan(int M, int K, int N) : original_M(M), original_K(K), original_N(N),
                                    primary_strategy(BlockType::SIMPLE_ROW_BLOCK), total_blocks(0),
                                    num_blocks_M(1), num_blocks_K(1), num_blocks_N(1),
                                    optimal_block_M(16), optimal_block_K(16), optimal_block_N(16) {}
    
    // 打印分块计划摘要
    void print_summary() const {
        std::cout << "📋 大矩阵分块计划摘要:" << std::endl;
        std::cout << "  ├─ 原始尺寸: A[" << original_M << "×" << original_K 
                  << "] × B[" << original_K << "×" << original_N << "]" << std::endl;
        std::cout << "  ├─ 分块策略: ";
        switch(primary_strategy) {
            case BlockType::SIMPLE_ROW_BLOCK: std::cout << "行分块"; break;
            case BlockType::SIMPLE_COL_BLOCK: std::cout << "列分块"; break;
            case BlockType::INNER_DIM_BLOCK: std::cout << "内维分块"; break;
            case BlockType::MIXED_2D_BLOCK: std::cout << "2D混合分块"; break;
            case BlockType::MIXED_3D_BLOCK: std::cout << "3D全维分块"; break;
        }
        std::cout << std::endl;
        std::cout << "  ├─ 分块数量: " << total_blocks << " 个 (" << num_blocks_M 
                  << "×" << num_blocks_K << "×" << num_blocks_N << ")" << std::endl;
        std::cout << "  └─ 最优块尺寸: [" << optimal_block_M << "×" << optimal_block_K 
                  << "×" << optimal_block_N << "]" << std::endl;
    }
};


/**
 * @brief 大矩阵分块控制器主类
 * @tparam MAX_SIZE 硬件PE阵列最大支持尺寸（默认16）
 */
template<int MAX_SIZE = 16>
class LargeMatrixBlockController {
private:
    using MatrixSet = ::MatrixSet<MAX_SIZE>;
    using MultiFrameSet = MultiFrameMatrixSet<MAX_SIZE>;
    
public:
    /**
     * @brief 🚀 核心接口：执行大矩阵GEMM分块计算
     * @param large_A 大矩阵A指针 [M×K]
     * @param large_B 大矩阵B指针 [K×N] 
     * @param large_D 大矩阵D指针 [M×N]
     * @param result_C 结果矩阵C指针 [M×N] (输出)
     * @param M 矩阵A行数
     * @param K 内部维度（A列数=B行数）
     * @param N 矩阵B列数
     * @return 多帧测试数据集（用于现有测试框架）
     */
    static std::unique_ptr<MultiFrameSet> execute_large_gemm(
        const float* large_A, const float* large_B, const float* large_D,
        float* result_C, int M, int K, int N) {
        
        std::cout << "🚀 启动大矩阵GEMM计算: A[" << M << "×" << K << "] × B[" 
                  << K << "×" << N << "] + D[" << M << "×" << N << "]" << std::endl;
        
        // 步骤1: 检查是否需要分块
        if (M <= MAX_SIZE && K <= MAX_SIZE && N <= MAX_SIZE) {
            std::cout << "📋 矩阵尺寸在硬件限制内，使用单块处理" << std::endl;
            return create_single_block_processing(large_A, large_B, large_D, result_C, M, K, N);
        }
        
        // 步骤2: 生成分块计划
        BlockPlan plan = generate_block_plan(M, K, N);
        plan.print_summary();
        
        // 步骤3: 创建多帧数据集
        auto multi_frame_set = create_multi_frame_from_blocks(
            large_A, large_B, large_D, result_C, plan);
        
        std::cout << "✅ 大矩阵分块处理完成，生成 " << plan.total_blocks << " 个计算帧" << std::endl;
        return multi_frame_set;
    }
    
    /**
     * @brief 🚀 便捷接口：重构计算结果到目标大矩阵
     * @param multi_frame_result 多帧计算结果
     * @param plan 原分块计划
     * @param result_C 目标结果矩阵 [M×N]
     */
    static void reconstruct_large_result(const MultiFrameSet& multi_frame_result,
                                        const BlockPlan& plan, float* result_C) {
        
        std::cout << "🔄 开始重构大矩阵计算结果..." << std::endl;
        
        // 初始化结果矩阵（用于累加情况）
        std::memset(result_C, 0, sizeof(float) * plan.original_M * plan.original_N);
        
        // 处理每个分块结果
        for (int frame_idx = 0; frame_idx < multi_frame_result.get_frame_count(); frame_idx++) {
            const auto* matrix_set = multi_frame_result.get_frame(frame_idx);
            const auto& block_info = plan.block_sequence[frame_idx];
            
            if (!matrix_set) {
                std::cout << "❌ Frame " << frame_idx << " 结果为空，跳过" << std::endl;
                continue;
            }
            
            // 将分块结果写入大矩阵对应位置
            for (int i = 0; i < block_info.block_M; i++) {
                for (int j = 0; j < block_info.block_N; j++) {
                    int orig_i = block_info.offset_M + i;
                    int orig_j = block_info.offset_N + j;
                    
                    if (orig_i < plan.original_M && orig_j < plan.original_N) {
                        if (block_info.requires_accumulation) {
                            // 累加模式（K维分块）
                            result_C[orig_i * plan.original_N + orig_j] += matrix_set->C(i, j);
                        } else {
                            // 直接赋值模式（M,N维分块）
                            result_C[orig_i * plan.original_N + orig_j] = matrix_set->C(i, j);
                        }
                    }
                }
            }
        }
        
        std::cout << "✅ 大矩阵结果重构完成" << std::endl;
    }

    /**
     * @brief 生成最优分块计划（公开接口，用于测试和调试）
     */
    static BlockPlan generate_block_plan(int M, int K, int N) {
        BlockPlan plan(M, K, N);
        
        // 🚀 Think Ultra：智能分块策略选择
        if (M > MAX_SIZE && K <= MAX_SIZE && N <= MAX_SIZE) {
            // 策略1: 行分块 (仅M超限)
            plan.primary_strategy = BlockType::SIMPLE_ROW_BLOCK;
            generate_row_block_plan(plan);
            
        } else if (M <= MAX_SIZE && K <= MAX_SIZE && N > MAX_SIZE) {
            // 策略2: 列分块 (仅N超限)
            plan.primary_strategy = BlockType::SIMPLE_COL_BLOCK;
            generate_col_block_plan(plan);
            
        } else if (M <= MAX_SIZE && K > MAX_SIZE && N <= MAX_SIZE) {
            // 策略3: 内维分块 (仅K超限，需要累加)
            plan.primary_strategy = BlockType::INNER_DIM_BLOCK;
            generate_inner_dim_block_plan(plan);
            
        } else if (M > MAX_SIZE && K <= MAX_SIZE && N > MAX_SIZE) {
            // 策略4: 2D分块 (M,N超限)
            plan.primary_strategy = BlockType::MIXED_2D_BLOCK;
            generate_2d_block_plan(plan);
            
        } else {
            // 策略5: 3D全维分块 (M,K,N都超限)
            plan.primary_strategy = BlockType::MIXED_3D_BLOCK;
            generate_3d_block_plan(plan);
        }
        
        
        return plan;
    }

private:
    
    /**
     * @brief 策略1: 行分块实现 (M > MAX_SIZE)
     */
    static void generate_row_block_plan(BlockPlan& plan) {
        int M = plan.original_M;
        int K = plan.original_K; 
        int N = plan.original_N;
        
        // 计算最优行分块数量
        plan.num_blocks_M = (M + MAX_SIZE - 1) / MAX_SIZE;  // 向上取整
        plan.num_blocks_K = 1;
        plan.num_blocks_N = 1;
        plan.total_blocks = plan.num_blocks_M;
        
        plan.optimal_block_M = std::min(MAX_SIZE, M);
        plan.optimal_block_K = K;
        plan.optimal_block_N = N;
        
        // 生成分块序列
        plan.block_sequence.clear();
        plan.block_sequence.reserve(plan.total_blocks);
        
        for (int block_m = 0; block_m < plan.num_blocks_M; block_m++) {
            int start_m = block_m * MAX_SIZE;
            int end_m = std::min(start_m + MAX_SIZE, M);
            int actual_block_M = end_m - start_m;
            
            BlockInfo block_info(actual_block_M, K, N, start_m, 0, 0, 
                                BlockType::SIMPLE_ROW_BLOCK, block_m);
            block_info.requires_accumulation = false;  // 行分块不需要累加
            
            plan.block_sequence.push_back(block_info);
        }
        
        std::cout << "📋 行分块策略: " << plan.num_blocks_M << " 个行块" << std::endl;
    }
    
    /**
     * @brief 策略2: 列分块实现 (N > MAX_SIZE)
     */
    static void generate_col_block_plan(BlockPlan& plan) {
        int M = plan.original_M;
        int K = plan.original_K;
        int N = plan.original_N;
        
        // 计算最优列分块数量
        plan.num_blocks_M = 1;
        plan.num_blocks_K = 1;
        plan.num_blocks_N = (N + MAX_SIZE - 1) / MAX_SIZE;  // 向上取整
        plan.total_blocks = plan.num_blocks_N;
        
        plan.optimal_block_M = M;
        plan.optimal_block_K = K;
        plan.optimal_block_N = std::min(MAX_SIZE, N);
        
        // 生成分块序列
        plan.block_sequence.clear();
        plan.block_sequence.reserve(plan.total_blocks);
        
        for (int block_n = 0; block_n < plan.num_blocks_N; block_n++) {
            int start_n = block_n * MAX_SIZE;
            int end_n = std::min(start_n + MAX_SIZE, N);
            int actual_block_N = end_n - start_n;
            
            BlockInfo block_info(M, K, actual_block_N, 0, 0, start_n,
                                BlockType::SIMPLE_COL_BLOCK, block_n);
            block_info.requires_accumulation = false;  // 列分块不需要累加
            
            plan.block_sequence.push_back(block_info);
        }
        
        std::cout << "📋 列分块策略: " << plan.num_blocks_N << " 个列块" << std::endl;
    }
    
    /**
     * @brief 策略3: 内维分块实现 (K > MAX_SIZE, 需要累加)
     */
    static void generate_inner_dim_block_plan(BlockPlan& plan) {
        int M = plan.original_M;
        int K = plan.original_K;
        int N = plan.original_N;
        
        // 计算最优内维分块数量
        plan.num_blocks_M = 1;
        plan.num_blocks_K = (K + MAX_SIZE - 1) / MAX_SIZE;  // 向上取整
        plan.num_blocks_N = 1;
        plan.total_blocks = plan.num_blocks_K;
        
        plan.optimal_block_M = M;
        plan.optimal_block_K = std::min(MAX_SIZE, K);
        plan.optimal_block_N = N;
        
        // 生成分块序列
        plan.block_sequence.clear();
        plan.block_sequence.reserve(plan.total_blocks);
        
        for (int block_k = 0; block_k < plan.num_blocks_K; block_k++) {
            int start_k = block_k * MAX_SIZE;
            int end_k = std::min(start_k + MAX_SIZE, K);
            int actual_block_K = end_k - start_k;
            
            BlockInfo block_info(M, actual_block_K, N, 0, start_k, 0,
                                BlockType::INNER_DIM_BLOCK, block_k);
            
            // 🚀 关键：内维分块需要累加逻辑
            block_info.requires_accumulation = (block_k > 0);  // 第一块不累加，后续块累加
            block_info.is_first_k_block = (block_k == 0);
            block_info.is_last_k_block = (block_k == plan.num_blocks_K - 1);
            
            plan.block_sequence.push_back(block_info);
        }
        
        std::cout << "📋 内维分块策略: " << plan.num_blocks_K << " 个K块 (需累加)" << std::endl;
    }
    
    /**
     * @brief 策略4: 2D分块实现 (M,N > MAX_SIZE)
     */
    static void generate_2d_block_plan(BlockPlan& plan) {
        int M = plan.original_M;
        int K = plan.original_K;
        int N = plan.original_N;
        
        // 计算2D分块数量
        plan.num_blocks_M = (M + MAX_SIZE - 1) / MAX_SIZE;
        plan.num_blocks_K = 1;
        plan.num_blocks_N = (N + MAX_SIZE - 1) / MAX_SIZE;
        plan.total_blocks = plan.num_blocks_M * plan.num_blocks_N;
        
        plan.optimal_block_M = std::min(MAX_SIZE, M);
        plan.optimal_block_K = K;
        plan.optimal_block_N = std::min(MAX_SIZE, N);
        
        // 生成2D分块序列
        plan.block_sequence.clear();
        plan.block_sequence.reserve(plan.total_blocks);
        
        int block_index = 0;
        for (int block_m = 0; block_m < plan.num_blocks_M; block_m++) {
            for (int block_n = 0; block_n < plan.num_blocks_N; block_n++) {
                int start_m = block_m * MAX_SIZE;
                int end_m = std::min(start_m + MAX_SIZE, M);
                int actual_block_M = end_m - start_m;
                
                int start_n = block_n * MAX_SIZE;
                int end_n = std::min(start_n + MAX_SIZE, N);
                int actual_block_N = end_n - start_n;
                
                BlockInfo block_info(actual_block_M, K, actual_block_N, 
                                    start_m, 0, start_n,
                                    BlockType::MIXED_2D_BLOCK, block_index);
                block_info.requires_accumulation = false;  // 2D分块不需要累加
                
                plan.block_sequence.push_back(block_info);
                block_index++;
            }
        }
        
        std::cout << "📋 2D分块策略: " << plan.num_blocks_M << "×" << plan.num_blocks_N 
                  << " = " << plan.total_blocks << " 个2D块" << std::endl;
    }
    
    /**
     * @brief 策略5: 3D全维分块实现 (M,K,N都 > MAX_SIZE) 
     */
    static void generate_3d_block_plan(BlockPlan& plan) {
        int M = plan.original_M;
        int K = plan.original_K;
        int N = plan.original_N;
        
        // 计算3D分块数量
        plan.num_blocks_M = (M + MAX_SIZE - 1) / MAX_SIZE;
        plan.num_blocks_K = (K + MAX_SIZE - 1) / MAX_SIZE;
        plan.num_blocks_N = (N + MAX_SIZE - 1) / MAX_SIZE;
        plan.total_blocks = plan.num_blocks_M * plan.num_blocks_K * plan.num_blocks_N;
        
        plan.optimal_block_M = std::min(MAX_SIZE, M);
        plan.optimal_block_K = std::min(MAX_SIZE, K);
        plan.optimal_block_N = std::min(MAX_SIZE, N);
        
        // 生成3D分块序列 (按M-K-N顺序嵌套)
        plan.block_sequence.clear();
        plan.block_sequence.reserve(plan.total_blocks);
        
        int block_index = 0;
        for (int block_m = 0; block_m < plan.num_blocks_M; block_m++) {
            for (int block_n = 0; block_n < plan.num_blocks_N; block_n++) {
                for (int block_k = 0; block_k < plan.num_blocks_K; block_k++) {
                    int start_m = block_m * MAX_SIZE;
                    int end_m = std::min(start_m + MAX_SIZE, M);
                    int actual_block_M = end_m - start_m;
                    
                    int start_k = block_k * MAX_SIZE;
                    int end_k = std::min(start_k + MAX_SIZE, K);
                    int actual_block_K = end_k - start_k;
                    
                    int start_n = block_n * MAX_SIZE;
                    int end_n = std::min(start_n + MAX_SIZE, N);
                    int actual_block_N = end_n - start_n;
                    
                    BlockInfo block_info(actual_block_M, actual_block_K, actual_block_N,
                                        start_m, start_k, start_n,
                                        BlockType::MIXED_3D_BLOCK, block_index);
                    
                    // 🚀 3D分块累加逻辑：只有K维度的第一块不累加
                    block_info.requires_accumulation = (block_k > 0);
                    block_info.is_first_k_block = (block_k == 0);
                    block_info.is_last_k_block = (block_k == plan.num_blocks_K - 1);
                    
                    plan.block_sequence.push_back(block_info);
                    block_index++;
                }
            }
        }
        
        std::cout << "📋 3D分块策略: " << plan.num_blocks_M << "×" << plan.num_blocks_K 
                  << "×" << plan.num_blocks_N << " = " << plan.total_blocks << " 个3D块" << std::endl;
    }
    
    
    /**
     * @brief 单块处理（矩阵尺寸在硬件限制内）
     */
    static std::unique_ptr<MultiFrameSet> create_single_block_processing(
        const float* large_A, const float* large_B, const float* large_D,
        float* result_C, int M, int K, int N) {
        
        // 创建单帧配置
        FrameTestConfig frame_config;
        frame_config.frame_count = 1;
        frame_config.test_mode = FrameTestConfig::TestMode::MIXED_MULTI_FRAME;
        
        auto multi_frame_set = std::make_unique<MultiFrameSet>(frame_config);
        
        // 创建单个矩阵集
        auto matrix_set = std::make_unique<MatrixSet>(M, K, N);
        
        // 复制数据到矩阵集
        copy_matrix_data(*matrix_set, large_A, large_B, large_D, M, K, N);
        
        // 配置单帧
        SingleFrameConfig single_config(M, K, N);
        single_config.is_variable_size = (M != K || K != N);
        single_config.data_type = MatrixTestConfig::DataType::DECIMAL_TYPE;
        
        // 添加到多帧集合
        multi_frame_set->add_frame(std::move(matrix_set), single_config);
        
        return multi_frame_set;
    }
    
    /**
     * @brief 从分块计划创建多帧数据集
     */
    static std::unique_ptr<MultiFrameSet> create_multi_frame_from_blocks(
        const float* large_A, const float* large_B, const float* large_D,
        float* result_C, const BlockPlan& plan) {
        
        // 创建多帧配置
        FrameTestConfig frame_config;
        frame_config.frame_count = plan.total_blocks;
        frame_config.test_mode = FrameTestConfig::TestMode::MIXED_MULTI_FRAME;
        
        auto multi_frame_set = std::make_unique<MultiFrameSet>(frame_config);
        
        // 为每个分块创建矩阵集
        for (const auto& block_info : plan.block_sequence) {
            auto matrix_set = extract_block_matrices(large_A, large_B, large_D, 
                                                    plan.original_M, plan.original_K, plan.original_N,
                                                    block_info);
            
            // 配置帧信息
            SingleFrameConfig single_config(block_info.block_M, block_info.block_K, block_info.block_N);
            single_config.is_variable_size = (block_info.block_M != block_info.block_K || 
                                             block_info.block_K != block_info.block_N);
            single_config.data_type = MatrixTestConfig::DataType::DECIMAL_TYPE;
            single_config.seed_offset = block_info.block_index;
            
            // 添加到多帧集合
            multi_frame_set->add_frame(std::move(matrix_set), single_config);
        }
        
        return multi_frame_set;
    }
    
    /**
     * @brief 从大矩阵中提取分块子矩阵
     */
    static std::unique_ptr<MatrixSet> extract_block_matrices(
        const float* large_A, const float* large_B, const float* large_D,
        int orig_M, int orig_K, int orig_N, const BlockInfo& block) {
        
        auto matrix_set = std::make_unique<MatrixSet>(block.block_M, block.block_K, block.block_N);
        
        // 🚀 提取A矩阵块 [block_M × block_K]
        for (int i = 0; i < block.block_M; i++) {
            for (int j = 0; j < block.block_K; j++) {
                int orig_i = block.offset_M + i;
                int orig_j = block.offset_K + j;
                if (orig_i < orig_M && orig_j < orig_K) {
                    matrix_set->A(i, j) = large_A[orig_i * orig_K + orig_j];
                } else {
                    matrix_set->A(i, j) = 0.0f;  // 边界填充
                }
            }
        }
        
        // 🚀 提取B矩阵块 [block_K × block_N]
        for (int i = 0; i < block.block_K; i++) {
            for (int j = 0; j < block.block_N; j++) {
                int orig_i = block.offset_K + i;
                int orig_j = block.offset_N + j;
                if (orig_i < orig_K && orig_j < orig_N) {
                    matrix_set->B(i, j) = large_B[orig_i * orig_N + orig_j];
                } else {
                    matrix_set->B(i, j) = 0.0f;  // 边界填充
                }
            }
        }
        
        // 🚀 提取D矩阵块 [block_M × block_N]
        for (int i = 0; i < block.block_M; i++) {
            for (int j = 0; j < block.block_N; j++) {
                int orig_i = block.offset_M + i;
                int orig_j = block.offset_N + j;
                if (orig_i < orig_M && orig_j < orig_N) {
                    if (block.is_first_k_block) {
                        // K维第一块：使用原始D值
                        matrix_set->D(i, j) = large_D[orig_i * orig_N + orig_j];
                    } else {
                        // K维后续块：D设为0（因为要累加）
                        matrix_set->D(i, j) = 0.0f;
                    }
                } else {
                    matrix_set->D(i, j) = 0.0f;  // 边界填充
                }
            }
        }
        
        return matrix_set;
    }
    
    /**
     * @brief 复制矩阵数据到MatrixSet（单块情况）
     */
    static void copy_matrix_data(MatrixSet& matrix_set, 
                                const float* A, const float* B, const float* D,
                                int M, int K, int N) {
        // 复制A矩阵 [M×K]
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < K; j++) {
                matrix_set.A(i, j) = A[i * K + j];
            }
        }
        
        // 复制B矩阵 [K×N]
        for (int i = 0; i < K; i++) {
            for (int j = 0; j < N; j++) {
                matrix_set.B(i, j) = B[i * N + j];
            }
        }
        
        // 复制D矩阵 [M×N]
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                matrix_set.D(i, j) = D[i * N + j];
            }
        }
    }
    
};

// 🚀 便捷类型别名
using LargeMatrixController16 = LargeMatrixBlockController<16>;

#endif // LARGE_MATRIX_BLOCK_CONTROL_H