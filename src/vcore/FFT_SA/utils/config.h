#ifndef CONFIG_H
#define CONFIG_H

// FFT操作延时周期数
constexpr int FFT_OPERATION_CYCLES = 3;

// GEMM操作延时周期数
constexpr int GEMM_OPERATION_CYCLES = 2;

// Shuffle操作延时周期数
constexpr int SHUFFLE_OPERATION_CYCLES = 2;

// log2函数（编译时计算）
constexpr unsigned log2_const(unsigned n) {
    return (n <= 1) ? 0 : 1 + log2_const(n >> 1);
}

#endif