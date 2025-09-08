/**
 * @file bypass_config.h
 * @brief FFT Bypass模式手动配置参数
 * 
 * 此文件提供了手动配置bypass参数的接口，用户可以修改这些参数
 * 来测试不同的bypass配置组合。
 * 
 * @version 1.0
 * @date 2025-09-08
 */

#ifndef BYPASS_CONFIG_H
#define BYPASS_CONFIG_H

#include <cstdint>

// ====== 手动配置参数 ======

/**
 * @brief 手动配置的bypass测试参数
 * 
 * 用户可以修改这些参数来测试不同的bypass配置：
 * 
 * 对于FftTlm<float, 16>硬件（4级stage）：
 * - bypass_mask = 0x00: 16点FFT (所有stage激活)
 * - bypass_mask = 0x01: 8点FFT  (bypass stage 0)
 * - bypass_mask = 0x03: 4点FFT  (bypass stage 0,1)
 * - bypass_mask = 0x07: 2点FFT  (bypass stage 0,1,2)
 */
namespace BypassConfig {
    
    // ====== 基础配置 ======
    
    /** 硬件FFT规模 */
    static constexpr unsigned HARDWARE_FFT_SIZE = 16;
    
    /** 硬件PE数量 */
    static constexpr unsigned HARDWARE_NUM_PES = HARDWARE_FFT_SIZE / 2;
    
    /** 硬件stage数量 */
    static constexpr unsigned HARDWARE_NUM_STAGES = 4;  // log2(16) = 4
    
    // ====== 用户可修改的测试配置 ======
    
    /**
     * @brief 用户配置结构
     * 
     * 修改这些参数来自定义bypass测试行为
     */
    struct UserConfig {
        // 测试选择
        bool enable_8pt_test  = true;   // 是否测试8点FFT
        bool enable_4pt_test  = true;   // 是否测试4点FFT  
        bool enable_2pt_test  = true;   // 是否测试2点FFT
        bool enable_custom_test = false; // 是否启用自定义测试
        
        // 自定义bypass配置（仅当enable_custom_test=true时生效）
        uint32_t custom_bypass_mask = 0x01;     // 自定义bypass掩码
        unsigned custom_effective_size = 8;     // 自定义有效FFT点数
        
        // 测试参数
        float verification_tolerance = 1e-2f;   // 验证容差
        int timeout_cycles = 2000;              // 超时周期数
        bool verbose_output = true;             // 是否显示详细输出
        
        // 数据生成配置
        enum TestDataType {
            SEQUENTIAL,    // 顺序数据 (1,1), (2,2), ...
            RANDOM,        // 随机数据  
            IMPULSE,       // 脉冲数据 δ[n]
            CUSTOM         // 自定义数据
        };
        TestDataType data_type = SEQUENTIAL;
        
        // 自定义测试数据（仅当data_type=CUSTOM时使用）
        // 用户可以在这里指定具体的测试输入
        // 注意：数组大小应该与测试的FFT点数匹配
    };
    
    // ====== 预定义配置 ======
    
    /** 8点FFT配置 */
    struct Config8pt {
        static constexpr uint32_t bypass_mask = 0x01;     // bypass stage 0
        static constexpr unsigned effective_size = 8;
        static constexpr unsigned active_stages = 3;      // stage 1,2,3
    };
    
    /** 4点FFT配置 */  
    struct Config4pt {
        static constexpr uint32_t bypass_mask = 0x03;     // bypass stage 0,1
        static constexpr unsigned effective_size = 4;
        static constexpr unsigned active_stages = 2;      // stage 2,3
    };
    
    /** 2点FFT配置 */
    struct Config2pt {
        static constexpr uint32_t bypass_mask = 0x07;     // bypass stage 0,1,2
        static constexpr unsigned effective_size = 2;
        static constexpr unsigned active_stages = 1;      // stage 3
    };
    
    // ====== 辅助工具函数 ======
    
    /**
     * @brief 根据bypass掩码计算有效FFT大小
     */
    inline unsigned calculate_effective_size(uint32_t bypass_mask) {
        unsigned bypass_count = __builtin_popcount(bypass_mask);
        unsigned active_stages = HARDWARE_NUM_STAGES - bypass_count;
        return (active_stages > 0) ? (1U << active_stages) : 1;
    }
    
    /**
     * @brief 验证bypass配置是否有效
     */
    inline bool is_valid_bypass_config(uint32_t bypass_mask) {
        unsigned bypass_count = __builtin_popcount(bypass_mask);
        if (bypass_count > HARDWARE_NUM_STAGES) return false;
        
        unsigned effective_size = calculate_effective_size(bypass_mask);
        return (effective_size >= 2 && effective_size <= HARDWARE_FFT_SIZE);
    }
    
    /**
     * @brief 打印bypass配置信息
     */
    inline void print_config_info(uint32_t bypass_mask) {
        unsigned effective_size = calculate_effective_size(bypass_mask);
        unsigned bypass_count = __builtin_popcount(bypass_mask);
        unsigned active_stages = HARDWARE_NUM_STAGES - bypass_count;
        
        std::cout << "Bypass配置信息:" << std::endl;
        std::cout << "  硬件规模: " << HARDWARE_FFT_SIZE << "点" << std::endl;
        std::cout << "  Bypass掩码: 0x" << std::hex << bypass_mask << std::dec << std::endl;
        std::cout << "  Bypass级数: " << bypass_count << "/" << HARDWARE_NUM_STAGES << std::endl;
        std::cout << "  激活级数: " << active_stages << std::endl;
        std::cout << "  有效FFT大小: " << effective_size << "点" << std::endl;
        std::cout << "  有效PE数量: " << (effective_size/2) << "/" << HARDWARE_NUM_PES << std::endl;
    }
}

// ====== 全局配置实例 ======
// 用户可以修改这个实例来定制测试行为
extern BypassConfig::UserConfig g_bypass_user_config;

// ====== 快捷宏定义 ======
#define BYPASS_8PT_MASK  BypassConfig::Config8pt::bypass_mask
#define BYPASS_4PT_MASK  BypassConfig::Config4pt::bypass_mask  
#define BYPASS_2PT_MASK  BypassConfig::Config2pt::bypass_mask

#define EFFECTIVE_8PT_SIZE  BypassConfig::Config8pt::effective_size
#define EFFECTIVE_4PT_SIZE  BypassConfig::Config4pt::effective_size
#define EFFECTIVE_2PT_SIZE  BypassConfig::Config2pt::effective_size

#endif // BYPASS_CONFIG_H