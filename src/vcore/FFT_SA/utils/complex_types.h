/**
 * @file complex_types.h
 * @brief FFT专用轻量复数类型定义和工具函数
 * 
 * 设计特点：
 * - 硬件友好的结构体定义（无运算符重载魔法）
 * - 显式实虚部域，便于寄存/打拍/饱和控制
 * - 可配置流水线级数和缩放控制
 */

#ifndef COMPLEX_TYPES_H
#define COMPLEX_TYPES_H

#include <iostream>
#include <iomanip>
#include <type_traits>
#include <cmath>
#include <string>

using std::string;
using std::ostream;

// 前向声明
namespace sc_core {
    class sc_trace_file;
}


template<typename T>
struct complex {
    T real;  // 实部
    T imag;  // 虚部
    
    // 构造函数
    complex() : real(0), imag(0) {}
    complex(T r, T i) : real(r), imag(i) {}
    complex(T r) : real(r), imag(0) {}  // 实数构造
    
    // 赋值操作
    complex& operator=(T r) {
        real = r;
        imag = 0;
        return *this;
    }
    
    // 输出流操作（仿真调试用）
    friend ostream& operator<<(ostream& os, const complex& c) {
        os << "(" << c.real;
        if (c.imag >= 0) os << "+";
        os << c.imag << "j)";
        return os;
    }
    
    // 相等比较（测试验证用）
    bool operator==(const complex& other) const {
        return (real == other.real) && (imag == other.imag);
    }
    
    // 近似相等（浮点容差）
    bool approx_equal(const complex& other, T tolerance = 1e-6) const {
        return (fabs(real - other.real) < tolerance) && (fabs(imag - other.imag) < tolerance);
    }
    
    // ====== 运算符重载（为SystemC兼容性） ======
    
    // 复数加法运算符
    complex operator+(const complex& other) const {
        return complex(real + other.real, imag + other.imag);
    }
    
    // 复数减法运算符  
    complex operator-(const complex& other) const {
        return complex(real - other.real, imag - other.imag);
    }
    
    // 复数乘法运算符（标准公式）
    complex operator*(const complex& other) const {
        return complex(real * other.real - imag * other.imag, 
                       real * other.imag + imag * other.real);
    }
    
    // 标量乘法运算符
    complex operator*(const T& scalar) const {
        return complex(real * scalar, imag * scalar);
    }
    
    // 复合赋值运算符
    complex& operator+=(const complex& other) {
        real += other.real;
        imag += other.imag;
        return *this;
    }
    
    complex& operator-=(const complex& other) {
        real -= other.real;
        imag -= other.imag;
        return *this;
    }
    
    complex& operator*=(const complex& other) {
        T temp_re = real * other.real - imag * other.imag;
        T temp_im = real * other.imag + imag * other.real;
        real = temp_re;
        imag = temp_im;
        return *this;
    }
};

// ====== SystemC支持函数声明 ======

/**
 * @brief complex类型sc_trace支持函数声明
 */
// /**
//  * @brief complex类型sc_trace支持实现
//  */
template<typename T>
void sc_trace(sc_trace_file* tf, const complex<T>& c, const string& name) {
    sc_trace(tf, c.real, name + "_real");
    sc_trace(tf, c.imag, name + "_imag");
}


// ====== 基础复数运算函数 ======

/**
 * @brief 复数加法：c = a + b
 */
template<typename T>
inline complex<T> c_add(const complex<T>& a, const complex<T>& b) {
    return complex<T>(a.real + b.real, a.imag + b.imag);
}

/**
 * @brief 复数减法：c = a - b
 */
template<typename T>
inline complex<T> c_sub(const complex<T>& a, const complex<T>& b) {
    return complex<T>(a.real - b.real, a.imag - b.imag);
}

/**
 * @brief 复数共轭：c = conj(a) = a.real - j*a.imag
 */
template<typename T>
inline complex<T> c_conj(const complex<T>& a) {
    return complex<T>(a.real, -a.imag);
}

// ====== 4M2A复数乘法实现 ======
template<typename T>
inline complex<T> c_mul(const complex<T>& b, const complex<T>& W) {
    T t_re = b.real * W.real - b.imag * W.imag;
    T t_im = b.real * W.imag + b.imag * W.real;
    return complex<T>(t_re, t_im);
}
template<typename T>
inline complex<T> c_scale(const complex<T>& a, int shift) {
    T scale_factor = 1.0 / (1 << shift);  // 2^(-shift)
    return complex<T>(a.real * scale_factor, a.imag * scale_factor);
}


#endif // COMPLEX_TYPES_H