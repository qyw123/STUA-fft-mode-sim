/**
 * @file PEA.h
 * @brief Processing Element Array - 脉动阵列顶层模块
 * 
 * 实现矩阵运算: C = A×B + D
 * - A矩阵: 权重预加载到PE阵列 (A[i][j] → PE[j][i])
 * - B矩阵: 通过FIFO_V垂直输入 (B[i][:] → FIFO_V[i])
 * - D矩阵: 通过FIFO_H水平输入 (D[i][:] → FIFO_H[i])
 * - C矩阵: 通过FIFO_O输出收集 (C[i][:] ← FIFO_O[i])
 * 
 * 架构设计(以4*4为例):
 *          FIFO_H[0] FIFO_H[1] FIFO_H[2] FIFO_H[3]
 *               ↓          ↓          ↓          ↓
 * FIFO_V[0] → PE[0][0] → PE[0][1] → PE[0][2] → PE[0][3] 
 *               ↓          ↓          ↓          ↓
 * FIFO_V[1] → PE[1][0] → PE[1][1] → PE[1][2] → PE[1][3] 
 *               ↓          ↓          ↓          ↓
 * FIFO_V[2] → PE[2][0] → PE[2][1] → PE[2][2] → PE[2][3] 
 *               ↓          ↓          ↓          ↓
 * FIFO_V[3] → PE[3][0] → PE[3][1] → PE[3][2] → PE[3][3] 
 *               ↓          ↓          ↓          ↓
 *          FIFO_O[0] FIFO_O[1] FIFO_O[2] FIFO_O[3]
 */

#ifndef PEA_H
#define PEA_H

#include "systemc.h"
#include "pe.h"
#include "in_buf_vec.h"
#include "out_buf_vec.h"

template<typename T = float, int ARRAY_SIZE = 4, int FIFO_DEPTH = 8>
SC_MODULE(PEA) {
    // ====== 基础控制接口 ======
    sc_in_clk clk_i;                          // 系统时钟
    sc_in<bool> rst_i;                        // 复位信号（低有效）
    
    // ====== 矩阵A权重加载接口 ======
    sc_vector<sc_vector<sc_in<T>>> w_data_i;  // [ARRAY_SIZE][ARRAY_SIZE]权重输入矩阵
    sc_in<bool> w_load_start_i;               // 权重加载启动信号
    sc_in<bool> w_load_en_i;                  // 权重加载使能信号
    sc_out<bool> w_load_done_o;               // 权重加载完成信号
    
    // ====== 矩阵B输入接口(FIFO_V垂直输入) ======
    sc_vector<sc_in<T>> b_data_i;             // [ARRAY_SIZE]B矩阵行数据输入
    sc_in<bool> b_wr_start_i;                 // B矩阵写入启动信号
    sc_in<bool> b_wr_en_i;                    // B矩阵写入使能信号
    sc_vector<sc_out<bool>> b_wr_ready_o;     // [ARRAY_SIZE]B矩阵写入就绪信号
    
    // ====== 矩阵D偏置接口(FIFO_H水平输入) ======
    sc_vector<sc_in<T>> d_data_i;             // [ARRAY_SIZE]D矩阵行数据输入
    sc_in<bool> d_wr_start_i;                 // D矩阵写入启动信号
    sc_in<bool> d_wr_en_i;                    // D矩阵写入使能信号
    sc_vector<sc_out<bool>> d_wr_ready_o;     // [ARRAY_SIZE]D矩阵写入就绪信号
    
    // ====== 计算控制接口 ======
    sc_in<bool> compute_start_i;              // 计算启动信号
    sc_out<bool> compute_done_o;              // 计算完成信号
    
    // ====== 矩阵C输出接口(FIFO_O) ======
    sc_vector<sc_in<bool>> c_rd_start_i;      // 🚀 向量化C矩阵读取启动信号
    sc_vector<sc_out<T>> c_data_o;            // [ARRAY_SIZE]C矩阵输出数据
    sc_vector<sc_out<bool>> c_valid_o;        // [ARRAY_SIZE]C矩阵有效信号
    sc_vector<sc_out<bool>> c_ready_o;        // [ARRAY_SIZE]C矩阵就绪信号
    
    // 🚀 新增：变长矩阵尺寸输入接口（用于动态计算完成检测）
    sc_in<int> matrix_M_i;                    // 实际矩阵M尺寸输入
    sc_in<int> matrix_N_i;                    // 实际矩阵N尺寸输入
    sc_in<int> matrix_K_i;                    // 实际矩阵K尺寸输入
    
    // ====== 内部组件实例 ======
    // PE阵列 [ARRAY_SIZE][ARRAY_SIZE]
    PE<T> *pe_array[ARRAY_SIZE][ARRAY_SIZE];
    
    // FIFO_V垂直输入缓冲，管理ARRAY_SIZE行B数据
    IN_BUF_ROW_ARRAY<T, ARRAY_SIZE, FIFO_DEPTH> *fifo_v;
    
    // FIFO_H水平输入缓冲，管理ARRAY_SIZE行D数据
    IN_BUF_ROW_ARRAY<T, ARRAY_SIZE, FIFO_DEPTH> *fifo_h;
    
    // FIFO_O输出缓冲，收集ARRAY_SIZE行C数据
    OUT_BUF_ROW_ARRAY<T, ARRAY_SIZE, FIFO_DEPTH> *fifo_o;
    
    // ====== 内部信号连接 ======
    // PE阵列内部水平数据信号 [ARRAY_SIZE][ARRAY_SIZE-1] (水平传递x信号)
    sc_vector<sc_vector<sc_signal<T>>> h_data_sig;
    sc_vector<sc_vector<sc_signal<bool>>> h_valid_sig;
    
    // PE阵列内部垂直MAC信号 [ARRAY_SIZE-1][ARRAY_SIZE] (垂直传递mac信号)
    sc_vector<sc_vector<sc_signal<T>>> v_mac_sig; 
    sc_vector<sc_vector<sc_signal<bool>>> v_mac_valid_sig;
    
    // FIFO_V到PE第一列的连接信号 [ARRAY_SIZE]
    sc_vector<sc_signal<T>> fifo_v_to_pe_data;
    sc_vector<sc_signal<bool>> fifo_v_to_pe_valid;
    
    // FIFO_H到PE第一行的连接信号 [ARRAY_SIZE]
    sc_vector<sc_signal<T>> fifo_h_to_pe_data;
    sc_vector<sc_signal<bool>> fifo_h_to_pe_valid;
    
    // PE最后一行到FIFO_O的连接信号 [ARRAY_SIZE]
    sc_vector<sc_signal<T>> pe_to_fifo_o_data;
    sc_vector<sc_signal<bool>> pe_to_fifo_o_valid;
    
    // 权重加载控制信号
    sc_vector<sc_vector<sc_signal<bool>>> w_enable_sig;  // [ARRAY_SIZE][ARRAY_SIZE]
    sc_signal<int> w_load_col_cnt;                       // 当前加载列计数
    sc_signal<bool> w_load_active;                       // 权重加载活跃标志
    
    // 计算控制信号
    sc_signal<bool> b_rd_start_sig;                      // B数据读取启动
    sc_signal<bool> d_rd_start_sig;                      // D数据读取启动
    sc_vector<sc_signal<bool>> c_rd_start_sig;          // C数据读取启动
    sc_signal<bool> compute_active;                      // 计算活跃标志
    
    // 🔧 虚拟信号用于绑定最后一列PE的未使用端口
    sc_vector<sc_signal<T>> dummy_x_data_sig;            // [ARRAY_SIZE]最后一列PE的x_o端口虚拟信号
    sc_vector<sc_signal<bool>> dummy_x_valid_sig;        // [ARRAY_SIZE]最后一列PE的x_v_o端口虚拟信号
    
    // ====== 构造函数 ======
    SC_CTOR(PEA) : 
        w_data_i("w_data_i", ARRAY_SIZE),
        b_data_i("b_data_i", ARRAY_SIZE),
        b_wr_ready_o("b_wr_ready_o", ARRAY_SIZE),
        d_data_i("d_data_i", ARRAY_SIZE), 
        d_wr_ready_o("d_wr_ready_o", ARRAY_SIZE),
        c_rd_start_i("c_rd_start_i", ARRAY_SIZE),
        c_data_o("c_data_o", ARRAY_SIZE),
        c_valid_o("c_valid_o", ARRAY_SIZE),
        c_ready_o("c_ready_o", ARRAY_SIZE),
        h_data_sig("h_data_sig", ARRAY_SIZE),
        h_valid_sig("h_valid_sig", ARRAY_SIZE),
        v_mac_sig("v_mac_sig", ARRAY_SIZE-1),
        v_mac_valid_sig("v_mac_valid_sig", ARRAY_SIZE-1),
        fifo_v_to_pe_data("fifo_v_to_pe_data", ARRAY_SIZE),
        fifo_v_to_pe_valid("fifo_v_to_pe_valid", ARRAY_SIZE),
        fifo_h_to_pe_data("fifo_h_to_pe_data", ARRAY_SIZE),
        fifo_h_to_pe_valid("fifo_h_to_pe_valid", ARRAY_SIZE),
        pe_to_fifo_o_data("pe_to_fifo_o_data", ARRAY_SIZE),
        pe_to_fifo_o_valid("pe_to_fifo_o_valid", ARRAY_SIZE),
        w_enable_sig("w_enable_sig", ARRAY_SIZE),
        c_rd_start_sig("c_rd_start_sig", ARRAY_SIZE),  // 🚀 添加C读取启动信号向量初始化
        dummy_x_data_sig("dummy_x_data_sig", ARRAY_SIZE),
        dummy_x_valid_sig("dummy_x_valid_sig", ARRAY_SIZE)
    {
        // 初始化w_data_i二维向量
        for(int i = 0; i < ARRAY_SIZE; i++) {
            w_data_i[i].init(ARRAY_SIZE);
        }
        
        // 初始化内部信号向量
        for(int i = 0; i < ARRAY_SIZE; i++) {
            h_data_sig[i].init(ARRAY_SIZE-1);
            h_valid_sig[i].init(ARRAY_SIZE-1);
            w_enable_sig[i].init(ARRAY_SIZE);
        }
        for(int i = 0; i < ARRAY_SIZE-1; i++) {
            v_mac_sig[i].init(ARRAY_SIZE);
            v_mac_valid_sig[i].init(ARRAY_SIZE);
        }
        
        // 实例化组件
        instantiate_components();
        
        // 连接信号
        connect_signals();
        
        // 注册控制进程
        SC_METHOD(weight_load_control);
        sensitive << clk_i.pos();
        
        SC_METHOD(compute_control);
        sensitive << clk_i.pos();

        SC_METHOD(read_result_control);
        sensitive << clk_i.pos();
        
        // 初始化控制信号
        w_load_col_cnt.write(0);
        w_load_active.write(false);
        compute_active.write(false);
    }
    
    // ====== 组件实例化方法 ======
    void instantiate_components();
    
    // ====== 信号连接方法 ======
    void connect_signals();
    
    // ====== 控制逻辑方法 ======
    void weight_load_control();  // 权重加载控制
    void compute_control();       // 计算控制
    void read_result_control();  // 读取结果控制
    void debug_last_rows_mac();   // MAC信号调试
    
    // ====== 析构函数 ======
    ~PEA() {
        // 清理动态分配的组件
        for(int i = 0; i < ARRAY_SIZE; i++) {
            for(int j = 0; j < ARRAY_SIZE; j++) {
                delete pe_array[i][j];
            }
        }
        delete fifo_v;
        delete fifo_h;
        delete fifo_o;
    }
};

#include "../src/PEA.cpp"

#endif // PEA_H