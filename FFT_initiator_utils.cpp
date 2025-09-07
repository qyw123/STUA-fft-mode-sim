/**
 * @file FFT_initiator_utils.cpp
 */

#include "FFT_initiator_utils.h"

using namespace std;

namespace FFTInitiatorUtils {

DecompositionInfo analyze_decomposition_strategy(size_t fft_size, size_t base_n) {
    DecompositionInfo info;
    info.total_points = fft_size;
    info.is_valid = false;

    if (fft_size <= base_n) {
        info.level = 0;
        info.is_valid = true;
        return info;
    }
    // if (fft_size == 32 ) {
    //     cout << "DEBUG: fft_size == 32" << endl;
    //     info.level = 1;
    //     info.level_dims.push_back(make_pair(2, 16));  // 16列×2行
    //     info.is_valid = true;
    //     return info;
    // }
    size_t level1_max = base_n * base_n;
    // Level 1
    if (fft_size <= level1_max) {
        for (size_t n1 = base_n; n1 >= 2; n1--) {
            if (fft_size % n1 == 0) {
                size_t n2 = fft_size / n1;
                if (n2 <= base_n) {
                    info.level = 1;
                    info.level_dims.emplace_back(n2, n1);
                    info.is_valid = true;
                    return info;
                }
            }
            if (n1 == 2) break; // prevent wrap for size_t
        }
        // square fallback
        size_t sqrt_size = static_cast<size_t>(sqrt(fft_size));
        if (sqrt_size * sqrt_size == fft_size && sqrt_size <= base_n) {
            info.level = 1;
            info.level_dims.emplace_back(sqrt_size, sqrt_size);
            info.is_valid = true;
            return info;
        }
    }

    // Level 2
    size_t level2_max = level1_max * level1_max;
    if (fft_size <= level2_max) {
        vector<pair<size_t, size_t>> candidates;
        for (size_t n1 = 1; n1 * n1 <= fft_size; n1++) {
            if (fft_size % n1 == 0) {
                size_t n2 = fft_size / n1;
                if (n1 <= level1_max && n2 <= level1_max) {
                    candidates.emplace_back(n1, n2);
                }
            }
        }
        for (auto& cand : candidates) {
            size_t n1 = cand.first;
            size_t n2 = cand.second;
            bool n1_ok = (n1 <= base_n) || (n1 <= level1_max && can_decompose_level1(n1, base_n));
            bool n2_ok = (n2 <= base_n) || (n2 <= level1_max && can_decompose_level1(n2, base_n));
            if (n1_ok && n2_ok) {
                info.level = 2;
                info.level_dims.emplace_back(n2, n1); // row x col (N2 x N1)
                info.is_valid = true;
                if (n1 > base_n) info.sub_decompositions.push_back(find_level1_decomposition(n1, base_n));
                if (n2 > base_n) info.sub_decompositions.push_back(find_level1_decomposition(n2, base_n));
                return info;
            }
        }
    }

    return info;
}

bool can_decompose_level1(size_t size, size_t base_n) {
    if (size <= base_n) return true;
    for (size_t n1 = base_n; n1 >= 1; n1--) {
        if (size % n1 == 0) {
            size_t n2 = size / n1;
            if (n2 <= base_n) return true;
        }
        if (n1 == 1) break; // prevent wrap
    }
    return false;
}

pair<size_t, size_t> find_level1_decomposition(size_t size, size_t base_n) {
    for (size_t n1 = base_n; n1 >= 1; n1--) {
        if (size % n1 == 0) {
            size_t n2 = size / n1;
            if (n2 <= base_n) return make_pair(n1, n2);
        }
        if (n1 == 1) break; // prevent wrap
    }
    size_t sqrt_size = static_cast<size_t>(sqrt(size));
    return make_pair(sqrt_size, (size + sqrt_size - 1) / sqrt_size);
}

complex<float> compute_twiddle_factor(int k2, int n1, int N) {
    float angle = -2.0f * static_cast<float>(M_PI) * k2 * n1 / N;
    return complex<float>(cos(angle), sin(angle));
}

FFTConfiguration create_fft_configuration(size_t hw_size, size_t real_size) {
    FFTConfiguration config;
    config.fft_mode = true;
    config.fft_shift = 0;
    config.fft_conj_en = false;
    config.fft_size = hw_size;
    config.fft_size_real = real_size;

    int hw_stages = static_cast<int>(std::log2(hw_size));
    int required_stages = static_cast<int>(std::log2(real_size));
    config.stage_bypass_en.resize(hw_stages, false);

    if (real_size < hw_size) {
        int bypass_stages = hw_stages - required_stages;
        std::cout << "  - Bypass configuration: " << bypass_stages << " stages" << std::endl;
        for (int i = 0; i < bypass_stages; i++) config.stage_bypass_en[i] = true;
    }
    return config;
}




//访存相关
uint64_t calculate_ddr_address(unsigned frame_id, unsigned test_fft_size, uint64_t ddr_base_addr) {
    return ddr_base_addr + static_cast<uint64_t>(frame_id) * test_fft_size * sizeof(complex<float>) * 2ULL;
}

uint64_t calculate_am_address(unsigned frame_id, unsigned test_fft_size, uint64_t am_base_addr) {
    return am_base_addr + static_cast<uint64_t>(frame_id) * test_fft_size * sizeof(complex<float>) * 2ULL;
}

} // namespace FFTInitiatorUtils

