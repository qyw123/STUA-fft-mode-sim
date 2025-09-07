/**
 * @file FFT_initiator_utils.h
 * @brief Helper utilities for FFT_Initiator to keep core logic lean
 */

#ifndef FFT_INITIATOR_UTILS_H
#define FFT_INITIATOR_UTILS_H

#include <vector>
#include <utility>
#include <cstdint>
#include <cmath>
#include <iostream>

#include "src/vcore/FFT_SA/include/FFT_TLM.h"
#include "src/vcore/FFT_SA/utils/complex_types.h"

namespace FFTInitiatorUtils {

// Decomposition analysis info
struct DecompositionInfo {
    int level = 0;
    size_t total_points = 0;
    std::vector<std::pair<size_t, size_t>> level_dims;     // per-level dims
    bool is_valid = false;
    std::vector<std::pair<size_t, size_t>> sub_decompositions; // optional
};

// Decomposition helpers (independent of class state)
DecompositionInfo analyze_decomposition_strategy(size_t fft_size, size_t base_n);
bool can_decompose_level1(size_t size, size_t base_n);
std::pair<size_t, size_t> find_level1_decomposition(size_t size, size_t base_n);

// Twiddle helper
complex<float> compute_twiddle_factor(int k2, int n1, int N);

// Matrix reshape helpers (header-only templates)
template <typename T>
inline std::vector<std::vector<complex<T>>> reshape_to_matrix(const std::vector<complex<T>>& input,
                                                              int rows, int cols) {
    if (rows <= 0 || cols <= 0 || input.size() != static_cast<size_t>(rows * cols)) {
        return std::vector<std::vector<complex<T>>>(rows, std::vector<complex<T>>(cols, complex<T>(0, 0)));
    }
    std::vector<std::vector<complex<T>>> matrix(rows, std::vector<complex<T>>(cols));
    for (int i = 0; i < static_cast<int>(input.size()); ++i) {
        matrix[i / cols][i % cols] = input[i];
    }
    return matrix;
}

template <typename T>
inline std::vector<complex<T>> reshape_to_vector(const std::vector<std::vector<complex<T>>>& matrix) {
    if (matrix.empty() || matrix[0].empty()) {
        return std::vector<complex<T>>();
    }
    std::vector<complex<T>> output;
    output.reserve(matrix.size() * matrix[0].size());
    for (const auto& row : matrix) {
        for (const auto& val : row) output.push_back(val);
    }
    return output;
}

// Config helper
FFTConfiguration create_fft_configuration(size_t hw_size, size_t real_size);

// Addressing helpers

uint64_t calculate_ddr_address(unsigned frame_id, unsigned test_fft_size, uint64_t ddr_base_addr);
uint64_t calculate_am_address(unsigned frame_id, unsigned test_fft_size, uint64_t am_base_addr);

} // namespace FFTInitiatorUtils

#endif // FFT_INITIATOR_UTILS_H

