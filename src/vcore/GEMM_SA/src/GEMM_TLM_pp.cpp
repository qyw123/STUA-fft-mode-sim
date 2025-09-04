/**
 * @file GEMM_TLM.cpp
 * @brief GEMM_TLM Ultra并行优化版本实现 - 测试2专用
 */

#include "../include/GEMM_TLM_pp.h"
#include <iostream>

using namespace std;

// ====== 构造函数实现 ======
template<typename T, int SIZE>
GEMM_TLM<T, SIZE>::GEMM_TLM(sc_module_name name) : 
    sc_module(name), 
    target_socket("target_socket") {
    // 注册TLM接口回调
    target_socket.register_b_transport(this, &GEMM_TLM::b_transport);
    
    // 初始化二维信号向量
    for(int i = 0; i < SIZE; i++) {
        w_data_sig[i].init(SIZE);
    }
    
    // 创建PEA核心实例
    pea_core = new PEA<T, SIZE, 32>("pea_core");
    
    // 连接PEA接口信号
    connect_pea_signals();
    
    // 初始化状态
    current_state = IDLE;
    computation_complete = false;
    total_computation_time = sc_time(0, SC_NS);
    
    // 注册控制进程
    SC_METHOD(state_machine_control);
    sensitive << clk.posedge_event() << w_load_done_sig;
    
    SC_METHOD(monitor_computation);
    sensitive << compute_done_sig;
    dont_initialize();
    
    // 复位进程
    SC_THREAD(reset_sequence);
    
    // 🚀 Ultra并行控制线程
    SC_THREAD(load_A_thread);
    SC_THREAD(load_B_thread);
    SC_THREAD(load_D_thread);
}

// ====== PEA模块信号连接实现 ======
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::connect_pea_signals() {
    // 基础控制信号
    pea_core->clk_i(clk);
    pea_core->rst_i(rst);
    
    // 权重加载接口连接
    for(int i = 0; i < SIZE; i++) {
        for(int j = 0; j < SIZE; j++) {
            pea_core->w_data_i[i][j](w_data_sig[i][j]);
        }
    }
    pea_core->w_load_start_i(w_load_start_sig);
    pea_core->w_load_en_i(w_load_en_sig);
    pea_core->w_load_done_o(w_load_done_sig);
    
    // B矩阵输入接口连接
    for(int i = 0; i < SIZE; i++) {
        pea_core->b_data_i[i](b_data_sig[i]);
        pea_core->b_wr_ready_o[i](b_wr_ready_sig[i]);
    }
    pea_core->b_wr_start_i(b_wr_start_sig);
    pea_core->b_wr_en_i(b_wr_en_sig);
    
    // D矩阵输入接口连接  
    for(int i = 0; i < SIZE; i++) {
        pea_core->d_data_i[i](d_data_sig[i]);
        pea_core->d_wr_ready_o[i](d_wr_ready_sig[i]);
    }
    pea_core->d_wr_start_i(d_wr_start_sig);
    pea_core->d_wr_en_i(d_wr_en_sig);
    
    // 计算控制接口连接
    pea_core->compute_start_i(compute_start_sig);
    pea_core->compute_done_o(compute_done_sig);
    
    // C矩阵输出接口连接
    for(int i = 0; i < SIZE; i++) {
        pea_core->c_rd_start_i[i](c_rd_start_sig[i]);
        pea_core->c_data_o[i](c_data_sig[i]);
        pea_core->c_valid_o[i](c_valid_sig[i]);
        pea_core->c_ready_o[i](c_ready_sig[i]);
    }
    
    // 🚀 新增：矩阵尺寸信号连接
    pea_core->matrix_M_i(matrix_M_sig);
    pea_core->matrix_N_i(matrix_N_sig);
    pea_core->matrix_K_i(matrix_K_sig);
}


// ====== 复位序列实现 ======
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::reset_sequence() {
    while(true){
        wait(reset_trigger_event);
        cout << sc_time_stamp() << ": GEMM_TLM复位序列开始" << endl;
        
        // 激活复位
        rst.write(false);
        wait(RESET_DELAY);
        
        // 释放复位  
        rst.write(true);
        
        cout << sc_time_stamp() << ": GEMM_TLM复位完成" << endl;
    }
}

// ====== 状态机控制实现 ======
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::state_machine_control() {
    if (!rst.read()) {
        current_state = IDLE;
        computation_complete = false;
        // 🚀 复位时重置双重检测状态
        compute_done_prev = false;
        compute_done_double_checked = false;
        return;
    }
    
    switch(current_state) {
        case IDLE:
            // 空闲状态，等待外部命令
            break;
            
        case LOADING_PARALLEL:
            // 并行加载状态，检查所有线程是否完成
            if (load_A_finished && load_B_finished && load_D_finished) {
                cout << sc_time_stamp() << ": [GEMM_TLM状态机] 并行加载完成" << endl;
                current_state = IDLE;
                all_matrices_loaded.notify();
            }
            break;
            
        case COMPUTING: {
            // 计算中状态 - 连续两次检测到有效才确认完成
            bool current_compute_done = compute_done_sig.read();
            
            // 🚀 修复：只在状态机中处理双重检测逻辑，避免与监控函数冲突
            if (current_compute_done && compute_done_prev && !compute_done_double_checked) {
                // 连续两次检测到有效，确认计算完成
                cout << sc_time_stamp() << ": [GEMM_TLM状态机] ✅ 计算完成 (双重确认)" << endl;
                current_state = RESULT_READY;
                computation_complete = true;
                compute_done_double_checked = true;
                computation_done_event.notify();
            }
            
            // 更新前一次状态
            compute_done_prev = current_compute_done;
            break;
        }
            
        case RESULT_READY:
            // 结果就绪状态
            cout << sc_time_stamp() << ": [GEMM_TLM状态机] 结果就绪" << endl;
            cout << sc_time_stamp() <<  ": [GEMM_TLM状态机] 重置计算状态" << endl;
            current_state = IDLE;
            computation_complete = false;
            break;
            
        case ERROR_STATE:
            // 错误状态
            cout << sc_time_stamp() << ": GEMM_TLM处于错误状态" << endl;
            error_occurred_event.notify();
            break;
            
        // === 🚀 双缓冲流水线状态扩展 ===
        case PIPELINE_LOADING:
            // 流水线加载阶段
            if (pipeline_config.enable_debug_trace) {
                cout << sc_time_stamp() << ": [Pipeline-State] 流水线加载阶段 - 阶段" << current_pipeline_stage << endl;
            }
            
            // 检查当前阶段是否完成
            if (current_pipeline_stage == 0) { // Load阶段
                if (load_A_finished && load_B_finished && load_D_finished) {
                    current_pipeline_stage = 1;
                    current_state = PIPELINE_COMPUTING;
                    pipeline_stage_complete[0].notify();
                    
                    if (pipeline_config.enable_debug_trace) {
                        cout << "  ├─ 加载阶段完成，切换到计算阶段" << endl;
                    }
                }
            }
            break;
            
        case PIPELINE_COMPUTING: {
            // 流水线计算阶段 - 连续两次检测到有效才确认完成
            if (pipeline_config.enable_debug_trace && current_pipeline_stage == 1) {
                cout << sc_time_stamp() << ": [Pipeline-State] 流水线计算阶段" << endl;
            }
            
            bool current_compute_done = compute_done_sig.read();
            
            if (current_compute_done && compute_done_prev && !compute_done_double_checked) {
                // 连续两次检测到有效，确认流水线计算完成
                current_pipeline_stage = 2;
                current_state = PIPELINE_READING;
                computation_complete = true;
                pipeline_stage_complete[1].notify();
                compute_done_double_checked = true;
                
                if (pipeline_config.enable_debug_trace) {
                    cout << "  ├─ 计算阶段完成 (双重确认)，切换到读取阶段" << endl;
                }
            }
            
            // 更新前一次状态
            compute_done_prev = current_compute_done;
            break;
        }
            
        case PIPELINE_READING:
            // 流水线读取阶段
            if (pipeline_config.enable_debug_trace && current_pipeline_stage == 2) {
                cout << sc_time_stamp() << ": [Pipeline-State] 流水线读取阶段" << endl;
            }
            
            // 模拟读取完成条件 (实际应该基于读取信号)
            // 这里简化为延时后自动完成
            wait(DEFAULT_DELAY * 2);
            
            current_pipeline_stage = 0; // 重置为加载阶段
            pipeline_stage_complete[2].notify();
            pipeline_frame_complete.notify();
            
            // 检查是否还有更多帧要处理
            if (current_frame_index < total_frames_to_process) {
                current_state = PIPELINE_SWITCHING;
                if (pipeline_config.enable_debug_trace) {
                    cout << "  ├─ 读取阶段完成，切换缓冲区处理下一帧" << endl;
                }
            } else {
                current_state = PIPELINE_FINALIZING;
                if (pipeline_config.enable_debug_trace) {
                    cout << "  ├─ 所有帧处理完成，进入最终化阶段" << endl;
                }
            }
            break;
            
        case PIPELINE_MULTI_FRAME:
            // 多帧流水线处理状态 - 🚀 修复：直接完成，避免死循环
            if (pipeline_config.enable_debug_trace) {
                cout << sc_time_stamp() << ": [Pipeline-State] 多帧流水线处理状态 - 直接完成" << endl;
            }
            
            // 直接切换到最终化状态
            current_state = PIPELINE_FINALIZING;
            multi_frame_complete.notify();
            
            if (pipeline_config.enable_debug_trace) {
                cout << "  └─ 多帧处理状态完成，切换到最终化" << endl;
            }
            break;
            
        case PIPELINE_SWITCHING:
            // 流水线缓冲区切换状态
            if (pipeline_config.enable_debug_trace) {
                cout << sc_time_stamp() << ": [Pipeline-State] 缓冲区切换中..." << endl;
            }
            
            // 模拟缓冲区切换延时
            wait(DEFAULT_DELAY);
            
            // 切换到下一帧处理
            if (current_frame_index < total_frames_to_process) {
                current_state = PIPELINE_LOADING;
                if (pipeline_config.enable_debug_trace) {
                    cout << "  ├─ 缓冲区切换完成，开始处理帧 " << current_frame_index << endl;
                }
            } else {
                current_state = PIPELINE_FINALIZING;
            }
            break;
            
        case PIPELINE_FINALIZING:
            // 流水线结束处理状态
            if (pipeline_config.enable_debug_trace) {
                cout << sc_time_stamp() << ": [Pipeline-State] 流水线最终化处理" << endl;
            }
            
            // 完成统计计算
            if (pipeline_mode_enabled) {
                calculate_pipeline_timing();
                if (pipeline_config.enable_detailed_stats) {
                    analyze_overlap_potential();
                }
            }
            
            // 返回空闲状态
            current_state = IDLE;
            current_pipeline_stage = 0;
            multi_frame_complete.notify();
            
            if (pipeline_config.enable_debug_trace) {
                cout << "  └─ 流水线处理全部完成，返回空闲状态" << endl;
            }
            break;
    }
}

// ====== 监控计算完成实现 ======
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::monitor_computation() {
    bool current_compute_done = compute_done_sig.read();
    cout << sc_time_stamp() << ": [GEMM_TLMMonitor] compute_done_sig=" << current_compute_done 
         << ", current_state=" << current_state << endl;
    
    // 🚀 修复：仅作为监控输出，双重检测逻辑交由状态机处理
    // 避免多处处理导致的竞态条件和死锁
}

// ====== 🚀 Ultra并行控制线程实现 ======

// 🚀 优化：加载线程通用模板实现
template<typename T, int SIZE>
template<typename LoadFunc>
void GEMM_TLM<T, SIZE>::generic_load_thread(sc_event& start_event, sc_event& complete_event, 
                                           bool& finished_flag, LoadFunc load_function, const char* thread_name) {
    while(true) {
        wait(start_event);
        cout << sc_time_stamp() << ": [GEMM_TLM-" << thread_name << "] 矩阵加载线程启动" << endl;
        
        // 执行具体的加载逻辑
        load_function();
        
        cout << sc_time_stamp() << ": [GEMM_TLM-" << thread_name << "] 矩阵加载完成" << endl;
        
        // 设置完成标志位
        finished_flag = true;
        complete_event.notify();
    }
}

// A矩阵加载线程
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::load_A_thread() {
    auto load_A_func = [this]() {
        // 使用全局指针加载A矩阵数据
        if (global_A_ptr != nullptr) {
            cout << sc_time_stamp() << ": [A-Thread] 开始加载A[" << matrix_M << "×" << matrix_K << "]矩阵" << endl;
            
            // 🚀 变长矩阵支持：只加载有效矩阵区域A[0:M-1][0:K-1]
            for(int i = 0; i < matrix_M; i++) {
                for(int j = 0; j < matrix_K; j++) {
                    w_data_sig[i][j].write(global_A_ptr[i * matrix_K + j]);
                }
                // 🚀 关键优化：[K:SIZE-1]列填充零权重，确保无效PE不参与计算
                for(int j = matrix_K; j < SIZE; j++) {
                    w_data_sig[i][j].write(0.0f);
                }
            }
            
            // 🚀 关键优化：[M:SIZE-1]行全部填充零权重
            for(int i = matrix_M; i < SIZE; i++) {
                for(int j = 0; j < SIZE; j++) {
                    w_data_sig[i][j].write(0.0f);
                }
            }
            
            cout << sc_time_stamp() << ": [A-Thread] A矩阵数据写入完成，有效区域：[0:" << (matrix_M-1) 
                 << "][0:" << (matrix_K-1) << "]" << endl;
        }
        
        // 启动权重加载
        w_load_start_sig.write(true);
        wait(DEFAULT_DELAY);
        w_load_start_sig.write(false);
        
        // 🚀 变长矩阵支持：逐列加载权重，加载M列权重（对应输出行数）
        int effective_cols = std::max(matrix_M, 1);  // 至少加载1列，避免死锁
        for(int col = 0; col < effective_cols; col++) {
            w_load_en_sig.write(true);
            wait(DEFAULT_DELAY);
            w_load_en_sig.write(false);
            wait(DEFAULT_DELAY);  // 给PEA处理时间
        }
        
        cout << sc_time_stamp() << ": [A-Thread] 权重加载信号发送完成，有效列数：" << effective_cols << endl;
        
        // 等待硬件确认加载完成
        while(!w_load_done_sig.read()) {
            wait(DEFAULT_DELAY);
        }
    };
    
    generic_load_thread(load_A_start, load_A_complete, load_A_finished, load_A_func, "ThreadA");
}

// B矩阵加载线程  
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::load_B_thread() {
    auto load_B_func = [this]() {
        cout << sc_time_stamp() << ": [B-Thread] 开始加载B[" << matrix_K << "×" << matrix_N << "]矩阵" << endl;
        
        // 启动写入操作
        b_wr_start_sig.write(true);
        wait(DEFAULT_DELAY);
        b_wr_start_sig.write(false);
        
        // 使用全局指针加载B矩阵数据
        if (global_B_ptr != nullptr) {
            // 🚀 变长矩阵支持：按列加载B[0:K-1][0:N-1]有效区域
            for(int col = 0; col < matrix_N; col++) {
                // 加载有效行数据 B[0:K-1][col]
                for(int row = 0; row < matrix_K; row++) {
                    b_data_sig[row].write(global_B_ptr[row * matrix_N + col]);
                }
                // 🚀 关键优化：[K:SIZE-1]行填充零，确保无效数据不影响计算
                for(int row = matrix_K; row < SIZE; row++) {
                    b_data_sig[row].write(0.0f);
                }
                
                b_wr_en_sig.write(true);
                wait(DEFAULT_DELAY);
                b_wr_en_sig.write(false);
                
                if (col % 4 == 3) {  // 每4列输出一次进度
                    cout << sc_time_stamp() << ": [B-Thread] 已加载B矩阵列 " << (col+1) << "/" << matrix_N << endl;
                }
            }
            
            cout << sc_time_stamp() << ": [B-Thread] B矩阵加载完成，有效区域：[0:" << (matrix_K-1) 
                 << "][0:" << (matrix_N-1) << "]" << endl;
        }
    };
    
    generic_load_thread(load_B_start, load_B_complete, load_B_finished, load_B_func, "ThreadB");
}

// D矩阵加载线程
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::load_D_thread() {
    auto load_D_func = [this]() {
        cout << sc_time_stamp() << ": [D-Thread] 开始加载D[" << matrix_M << "×" << matrix_N << "]矩阵" << endl;
        
        // 启动写入操作
        d_wr_start_sig.write(true);
        wait(DEFAULT_DELAY);
        d_wr_start_sig.write(false);
        
        // 使用全局指针加载D矩阵数据
        if (global_D_ptr != nullptr) {
            // 🚀 变长矩阵支持：按列加载D[0:M-1][0:N-1]有效区域
            for(int col = 0; col < matrix_N; col++) {
                // 加载有效行数据 D[0:M-1][col]
                for(int row = 0; row < matrix_M; row++) {
                    d_data_sig[row].write(global_D_ptr[row * matrix_N + col]);
                }
                // 🚀 关键优化：[M:SIZE-1]行填充零，确保无效偏置不影响计算
                for(int row = matrix_M; row < SIZE; row++) {
                    d_data_sig[row].write(0.0f);
                }
                
                d_wr_en_sig.write(true);
                wait(DEFAULT_DELAY);
                d_wr_en_sig.write(false);
                
                if (col % 4 == 3) {  // 每4列输出一次进度
                    cout << sc_time_stamp() << ": [D-Thread] 已加载D矩阵列 " << (col+1) << "/" << matrix_N << endl;
                }
            }
            
            cout << sc_time_stamp() << ": [D-Thread] D矩阵加载完成，有效区域：[0:" << (matrix_M-1) 
                 << "][0:" << (matrix_N-1) << "]" << endl;
        }
    };
    
    generic_load_thread(load_D_start, load_D_complete, load_D_finished, load_D_func, "ThreadD");
}

// ====== 辅助函数实现 ======
template<typename T, int SIZE>
sc_time GEMM_TLM<T, SIZE>::compute_gemm() {
    cout << sc_time_stamp() << ": [GEMM_TLM] 启动GEMM计算..." << endl;
    
    current_state = COMPUTING;
    computation_complete = false;
    
    // 🚀 重置双重检测状态
    compute_done_prev = false;
    compute_done_double_checked = false;
    
    compute_start_sig.write(true);
    wait(DEFAULT_DELAY * 2);
    compute_start_sig.write(false);
    
    // 等待计算完成
    wait(computation_done_event);
    return sc_time(0, SC_NS);
}

template<typename T, int SIZE>
sc_time GEMM_TLM<T, SIZE>::read_result_C(T C[SIZE][SIZE]) {
    cout << sc_time_stamp() << ": [GEMM_TLM] 开始读取结果矩阵C[" << matrix_M << "×" << matrix_N << "]..." << endl;
    
    if (!compute_done_double_checked) {
        cout << "警告: 计算尚未通过双重确认完成!" << endl;
        return sc_time(0, SC_NS);
    }
    
    // 🚀 变长矩阵支持：只启动有效行的读取信号
    for(int i = 0; i < matrix_M; i++) {
        c_rd_start_sig[i].write(true);
    }
    // 确保无效行的读取信号为false
    for(int i = matrix_M; i < SIZE; i++) {
        c_rd_start_sig[i].write(false);
    }
    wait(DEFAULT_DELAY);
    
    // 🚀 变长矩阵支持：只读取有效区域C[0:M-1][0:N-1]的数据
    for(int col = 0; col < matrix_N; col++) {
        wait(DEFAULT_DELAY);
        for(int row = 0; row < matrix_M; row++) {
            float* C_ptr = reinterpret_cast<float*>(C);
            C_ptr[row * matrix_N + col] = c_data_sig[row].read();
            
            // // 调试输出（可选）
            // if (row < 2 && col < 2) {  // 只显示左上角4个元素，避免输出过多
            //     cout << "  └─ C[" << row << "][" << col << "] = " << C_ptr[row * matrix_N + col] << endl;
            // }
        }
        
        // if (col % 4 == 3) {  // 每4列输出一次进度
        //     cout << "  └─ 已读取结果矩阵C第 " << (col+1) << "/" << matrix_N << " 列" << endl;
        // }
    }
    
    // 关闭所有读取信号
    for(int i = 0; i < SIZE; i++) {
        c_rd_start_sig[i].write(false);
    }
    
    current_state = IDLE;
    
    // 🚀 变长矩阵支持：计算实际读取的数据量
    int actual_elements = matrix_M * matrix_N;
    sc_time actual_read_time = sc_time(actual_elements * 20, SC_NS);
    
    cout << sc_time_stamp() << ": [GEMM_TLM] 结果矩阵C读取完成，有效元素: " << actual_elements 
         << " (" << matrix_M << "×" << matrix_N << "), 耗时: " << actual_read_time << endl;
    
    return actual_read_time;
}

// ====== TLM传输接口实现 ======
template<typename T, int SIZE>
void GEMM_TLM<T, SIZE>::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    access_mutex.lock();
    
    gemm_payload_extension* ext = trans.get_extension<gemm_payload_extension>();
    uint8_t* data_ptr = trans.get_data_ptr();
    
    if (ext == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        access_mutex.unlock();
        return;
    }
    
    switch(ext->operation) {
        case gemm_operation_t::LOAD_ALL_MATRICES: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到LOAD_ALL_MATRICES命令" << endl;
            
            // 🚀 Ultra延时统计：记录加载开始时间
            current_timing_stats.load_start_time = sc_time_stamp();
            operation_start_timestamp = sc_time_stamp();
            
            if (trans.get_data_length() != sizeof(parallel_matrix_data)) {
                cout << "错误：并行矩阵数据大小不匹配" << endl;
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            parallel_matrix_data* matrix_data = reinterpret_cast<parallel_matrix_data*>(data_ptr);
            
            //🚀 优化：使用通用验证函数
            if (!validate_matrix_dimensions(matrix_data->M, matrix_data->K, matrix_data->N, "LOAD_ALL_MATRICES")) {
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            // 设置全局矩阵指针和尺寸
            global_A_ptr = matrix_data->matrix_A_ptr;
            global_B_ptr = matrix_data->matrix_B_ptr;
            global_D_ptr = matrix_data->matrix_D_ptr;
            matrix_M = matrix_data->M;
            matrix_K = matrix_data->K;
            matrix_N = matrix_data->N;
            
            // 🚀 新增：更新矩阵尺寸信号，传递给PEA模块
            matrix_M_sig.write(matrix_M);
            matrix_K_sig.write(matrix_K);
            matrix_N_sig.write(matrix_N);
            
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 矩阵尺寸信号已更新: M=" << matrix_M 
                 << ", K=" << matrix_K << ", N=" << matrix_N << endl;
            
            // 重置标志位
            load_A_finished = load_B_finished = load_D_finished = false;
            
            // 同时启动三个加载线程
            load_A_start.notify();
            load_B_start.notify(); 
            load_D_start.notify();
            
            // 等待所有线程完成
            while(!(load_A_finished && load_B_finished && load_D_finished)) {
                wait(DEFAULT_DELAY);
            }
            
            // 🚀 Ultra延时统计：计算真实硬件加载时间
            current_timing_stats.load_hardware_time = sc_time_stamp() - operation_start_timestamp;
            
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 所有矩阵并行加载完成，真实耗时: " 
                 << current_timing_stats.load_hardware_time << endl;
            
            // 返回真实硬件时间而非固定值
            delay += current_timing_stats.load_hardware_time;
            break;
        }
        
        case gemm_operation_t::START_COMPUTE: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到START_COMPUTE命令" << endl;
            
            // 🚀 Ultra延时统计：记录计算开始时间
            current_timing_stats.compute_start_time = sc_time_stamp();
            operation_start_timestamp = sc_time_stamp();
            
            // 执行计算（compute_gemm内部会等待硬件完成）
            sc_time compute_delay = compute_gemm();
            
            // 🚀 Ultra延时统计：计算真实硬件计算时间
            current_timing_stats.compute_hardware_time = sc_time_stamp() - operation_start_timestamp;
            
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 计算完成，真实耗时: " 
                 << current_timing_stats.compute_hardware_time << endl;
            
            // 返回真实硬件时间
            delay += current_timing_stats.compute_hardware_time;
            break;
        }
        
        case gemm_operation_t::READ_MATRIX_C: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到READ_MATRIX_C命令" << endl;
            
            // 🚀 Ultra延时统计：记录读取开始时间
            current_timing_stats.read_start_time = sc_time_stamp();
            operation_start_timestamp = sc_time_stamp();
            
            T(*C)[SIZE] = reinterpret_cast<T(*)[SIZE]>(data_ptr);
            sc_time read_delay = read_result_C(C);
            
            // 🚀 Ultra延时统计：计算真实硬件读取时间
            current_timing_stats.read_hardware_time = sc_time_stamp() - operation_start_timestamp;
            
            // 🚀 Ultra延时统计：计算总执行时间
            current_timing_stats.calculate_total_time();
            
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 读取完成，真实耗时: " 
                 << current_timing_stats.read_hardware_time << endl;
            
            
            // 返回真实硬件时间
            delay += current_timing_stats.read_hardware_time;
            break;
        }
        
        case gemm_operation_t::GET_STATUS: {
            uint32_t* status = reinterpret_cast<uint32_t*>(data_ptr);
            *status = static_cast<uint32_t>(current_state);
            break;
        }
        
        case gemm_operation_t::RESET_MODULE: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到RESET_MODULE命令" << endl;
            
            current_state = IDLE;
            computation_complete = false;
            
            // 🚀 Ultra延时统计：重置统计数据
            current_timing_stats.reset();
            computation_count++;
            
            reset_trigger_event.notify();
            break;
        }
        
        // === 🚀 双缓冲流水线TLM命令扩展 ===
        case gemm_operation_t::CONFIGURE_PIPELINE: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到CONFIGURE_PIPELINE命令" << endl;
            
            if (trans.get_data_length() != sizeof(PipelineConfig)) {
                cout << "错误: Pipeline配置数据大小不匹配" << endl;
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            PipelineConfig* config = reinterpret_cast<PipelineConfig*>(data_ptr);
            configure_pipeline(*config);
            
            delay += DEFAULT_DELAY;
            break;
        }
        
        case gemm_operation_t::ENABLE_PIPELINE_MODE: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到ENABLE_PIPELINE_MODE命令" << endl;
            
            bool success = enable_pipeline_mode();
            if (!success) {
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            }
            
            delay += DEFAULT_DELAY;
            break;
        }
        
        
        case gemm_operation_t::PROCESS_MULTI_FRAMES: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到PROCESS_MULTI_FRAMES命令" << endl;
            
            if (!pipeline_mode_enabled) {
                cout << "错误: 流水线模式未启用" << endl;
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            // 从payload中获取帧数
            if (trans.get_data_length() < sizeof(int)) {
                cout << "错误: 多帧处理数据不完整" << endl;
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            int* frame_count_ptr = reinterpret_cast<int*>(data_ptr);
            int frame_count = *frame_count_ptr;
            
            if (frame_count <= 0 || frame_count > 100) { // 限制最大帧数
                cout << "错误: 无效的帧数: " << frame_count << endl;
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            // 设置多帧处理参数
            total_frames_to_process = frame_count;
            current_frame_index = 0;
            
            // 🚀 修复死循环：直接执行模拟，不进入状态机循环
            sc_time multi_frame_time = simulate_multi_frame_execution(frame_count);
            
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 多帧模拟完成，耗时: " << multi_frame_time << endl;
            
            delay += multi_frame_time;
            break;
        }
        
        case gemm_operation_t::GET_PIPELINE_STATS: {
            cout << sc_time_stamp() << ": [GEMM_TLM-TLM] 接收到GET_PIPELINE_STATS命令" << endl;
            
            if (trans.get_data_length() != sizeof(UltraTimingStats)) {
                cout << "错误: Pipeline统计数据缓冲区大小不匹配" << endl;
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                break;
            }
            
            // 确保数据一致性
            if (current_timing_stats.total_execution_time == sc_time(0, SC_NS)) {
                current_timing_stats.calculate_total_time();
            }
            calculate_pipeline_timing();
            
            UltraTimingStats* stats_buffer = reinterpret_cast<UltraTimingStats*>(data_ptr);
            *stats_buffer = get_pipeline_stats();
            
            delay += DEFAULT_DELAY;
            break;
        }
        

    }
    
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    delay += DEFAULT_DELAY;
    access_mutex.unlock();
}

// 🚀 优化：矩阵验证通用函数实现（支持变长矩阵）
template<typename T, int SIZE>
bool GEMM_TLM<T, SIZE>::validate_matrix_dimensions(int M, int K, int N, const char* context) {
    // 检查是否超出PE阵列物理限制
    if (M > SIZE || K > SIZE || N > SIZE) {
        cout << "错误：矩阵尺寸超出PE阵列限制 [" << context << "] - PE阵列大小(" << SIZE << "x" << SIZE 
             << "), 请求尺寸(" << M << "x" << K << "x" << N << ")" << endl;
        return false;
    }
    
    // 检查矩阵乘法的K维度匹配
    // A[M][K] × B[K][N] = C[M][N]，这里的K必须匹配
    if (M <= 0 || K <= 0 || N <= 0) {
        cout << "错误：矩阵尺寸必须为正数 [" << context << "] - (" << M << "x" << K << "x" << N << ")" << endl;
        return false;
    }
    
    cout << "✅ 矩阵尺寸验证通过 [" << context << "] - A[" << M << "×" << K 
         << "] × B[" << K << "×" << N << "] = C[" << M << "×" << N << "]" << endl;
    return true;
}

// 🚀 优化：静态常量定义
template<typename T, int SIZE>
const sc_time GEMM_TLM<T, SIZE>::DEFAULT_DELAY = sc_time(10, SC_NS);

template<typename T, int SIZE>
const sc_time GEMM_TLM<T, SIZE>::COMPUTE_EXTRA_DELAY = sc_time(100, SC_NS);

template<typename T, int SIZE>
const sc_time GEMM_TLM<T, SIZE>::RESET_DELAY = sc_time(10, SC_NS);


// 显式模板实例化
template class GEMM_TLM<float, 4>;
template class GEMM_TLM<float, 6>;
template class GEMM_TLM<float, 8>;
template class GEMM_TLM<float, 16>;