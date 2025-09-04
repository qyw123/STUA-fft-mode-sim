/**
 * @file PEA_impl.h
 * @brief PEA模块实现细节
 * 
 * 包含组件实例化、信号连接和控制逻辑的具体实现
 */

#ifndef PEA_IMPL_H
#define PEA_IMPL_H

// ====== 组件实例化实现 ======
template<typename T, int ARRAY_SIZE, int FIFO_DEPTH>
void PEA<T, ARRAY_SIZE, FIFO_DEPTH>::instantiate_components() {
    // 实例化4x4 PE阵列
    for(int i = 0; i < ARRAY_SIZE; i++) {
        for(int j = 0; j < ARRAY_SIZE; j++) {
            char pe_name[32];
            sprintf(pe_name, "PE_%d_%d", i, j);
            pe_array[i][j] = new PE<T>(pe_name);
            
            // 连接PE基础信号
            pe_array[i][j]->clk_i(clk_i);
            pe_array[i][j]->rst_i(rst_i);
        }
    }
    
    // 实例化FIFO_V (垂直输入，管理4行B数据)
    fifo_v = new IN_BUF_ROW_ARRAY<T, ARRAY_SIZE, FIFO_DEPTH>("FIFO_V");
    
    // 连接FIFO_V基础信号
    fifo_v->clk_i(clk_i);
    fifo_v->rst_i(rst_i);
    
    // 连接FIFO_V写入接口
    for(int i = 0; i < ARRAY_SIZE; i++) {
        fifo_v->data_i_vec[i](b_data_i[i]);
        fifo_v->wr_read_o_vec[i](b_wr_ready_o[i]);
    }
    fifo_v->wr_start_i(b_wr_start_i);
    fifo_v->wr_en_i(b_wr_en_i);
    
    // 连接FIFO_V读出接口
    for(int i = 0; i < ARRAY_SIZE; i++) {
        fifo_v->data_o_vec[i](fifo_v_to_pe_data[i]);
        fifo_v->rd_valid_vec[i](fifo_v_to_pe_valid[i]);
    }
    fifo_v->rd_start_i(b_rd_start_sig);
    
    // 实例化FIFO_H (水平输入，管理4行D数据)
    fifo_h = new IN_BUF_ROW_ARRAY<T, ARRAY_SIZE, FIFO_DEPTH>("FIFO_H");
    
    // 连接FIFO_H基础信号
    fifo_h->clk_i(clk_i);
    fifo_h->rst_i(rst_i);
    
    // 连接FIFO_H写入接口
    for(int i = 0; i < ARRAY_SIZE; i++) {
        fifo_h->data_i_vec[i](d_data_i[i]);
        fifo_h->wr_read_o_vec[i](d_wr_ready_o[i]);
    }
    fifo_h->wr_start_i(d_wr_start_i);
    fifo_h->wr_en_i(d_wr_en_i);
    
    // 连接FIFO_H读出接口
    for(int i = 0; i < ARRAY_SIZE; i++) {
        fifo_h->data_o_vec[i](fifo_h_to_pe_data[i]);
        fifo_h->rd_valid_vec[i](fifo_h_to_pe_valid[i]);
    }
    fifo_h->rd_start_i(d_rd_start_sig);
    
    // 实例化FIFO_O (输出，收集4行C数据)
    fifo_o = new OUT_BUF_ROW_ARRAY<T, ARRAY_SIZE, FIFO_DEPTH>("FIFO_O");
    
    // 连接FIFO_O基础信号
    fifo_o->clk_i(clk_i);
    fifo_o->rst_i(rst_i);
    
    // 🚀 连接FIFO_O写入接口 (来自PE最后一行) - 基于PE有效性的精确控制
    for(int i = 0; i < ARRAY_SIZE; i++) {
        fifo_o->data_i_vec[i](pe_to_fifo_o_data[i]);
        fifo_o->wr_en_i_vec[i](pe_to_fifo_o_valid[i]);  // 🎯 关键修改：使用PE有效性信号
    }
    fifo_o->wr_start_i(compute_start_i);  // 计算启动时开始接收
    
    // 🚀 连接FIFO_O读出接口 - 向量化控制
    for(int i = 0; i < ARRAY_SIZE; i++) {
        fifo_o->data_o_vec[i](c_data_o[i]);
        fifo_o->rd_valid_vec[i](c_valid_o[i]);
        fifo_o->wr_ready_o_vec[i](c_ready_o[i]);        // 修正：写就绪信号
        fifo_o->rd_start_i_vec[i](c_rd_start_i[i]);     // 🚀 向量化读启动信号（外部控制）
    }
}

// ====== 信号连接实现 ======
template<typename T, int ARRAY_SIZE, int FIFO_DEPTH>
void PEA<T, ARRAY_SIZE, FIFO_DEPTH>::connect_signals() {
    
    // ===== PE阵列内部连接 =====
    for(int i = 0; i < ARRAY_SIZE; i++) {
        for(int j = 0; j < ARRAY_SIZE; j++) {
            
            // 水平数据传递: x_i → x_o
            if(j == 0) {
                // 第一列PE从FIFO_V接收数据
                pe_array[i][j]->x_i(fifo_v_to_pe_data[i]);
                pe_array[i][j]->x_v_i(fifo_v_to_pe_valid[i]);
            } else {
                // 其他列PE从左侧PE接收数据
                pe_array[i][j]->x_i(h_data_sig[i][j-1]);
                pe_array[i][j]->x_v_i(h_valid_sig[i][j-1]);
            }
            
            if(j < ARRAY_SIZE-1) {
                // 非最后一列PE向右侧传递数据
                pe_array[i][j]->x_o(h_data_sig[i][j]);
                pe_array[i][j]->x_v_o(h_valid_sig[i][j]);
            } else {
                // 🔧 最后一列PE绑定到虚拟信号（解决SystemC端口绑定问题）
                pe_array[i][j]->x_o(dummy_x_data_sig[i]);
                pe_array[i][j]->x_v_o(dummy_x_valid_sig[i]);
            }
            
            // 垂直MAC传递: mac_i → mac_o
            if(i == 0) {
                // 第一行PE从FIFO_H接收MAC初始值(D矩阵偏置)
                pe_array[i][j]->mac_i(fifo_h_to_pe_data[j]);
                pe_array[i][j]->mac_v_i(fifo_h_to_pe_valid[j]);
            } else {
                // 其他行PE从上方PE接收MAC累积值
                pe_array[i][j]->mac_i(v_mac_sig[i-1][j]);
                pe_array[i][j]->mac_v_i(v_mac_valid_sig[i-1][j]);
            }
            
            if(i < ARRAY_SIZE-1) {
                // 非最后一行PE向下方传递MAC结果
                pe_array[i][j]->mac_o(v_mac_sig[i][j]);
                pe_array[i][j]->mac_v_o(v_mac_valid_sig[i][j]);
            } else {
                // 最后一行PE输出到FIFO_O
                pe_array[i][j]->mac_o(pe_to_fifo_o_data[j]);
                pe_array[i][j]->mac_v_o(pe_to_fifo_o_valid[j]);
            }
            
            // 权重加载连接
            pe_array[i][j]->w_i(w_data_i[j][i]);  // 注意：A[i][j] → PE[j][i]
            pe_array[i][j]->wr_en_i(w_enable_sig[i][j]);
        }
    }
    
    // ===== FIFO读启动信号连接 =====
    // FIFO_V和FIFO_H读启动信号已在实例化时连接
}

// ====== 权重加载控制逻辑 ======
template<typename T, int ARRAY_SIZE, int FIFO_DEPTH>
void PEA<T, ARRAY_SIZE, FIFO_DEPTH>::weight_load_control() {
    if(rst_i.read() == false) {
        // 复位状态
        w_load_col_cnt.write(0);
        w_load_active.write(false);
        w_load_done_o.write(false);
        
        // 清除所有权重使能信号
        for(int i = 0; i < ARRAY_SIZE; i++) {
            for(int j = 0; j < ARRAY_SIZE; j++) {
                w_enable_sig[i][j].write(false);
            }
        }
    }
    else if(w_load_start_i.read() && !w_load_active.read()) {
        // 启动权重加载
        w_load_active.write(true);
        w_load_col_cnt.write(0);
        w_load_done_o.write(false);
        
        cout << sc_time_stamp() << " [PEA] 权重加载启动" << endl;
    }
    else if(w_load_active.read() && w_load_en_i.read()) {
        // 执行权重加载 - 按列加载
        int current_col = w_load_col_cnt.read();
        
        // 🚀 变长矩阵支持：获取实际M和K尺寸，加载M列权重，每列K个元素
        int actual_M = matrix_M_i.read();
        int actual_K = matrix_K_i.read();
        int effective_cols = std::min(std::max(actual_M, 1), ARRAY_SIZE);  // M列权重
        int effective_rows = std::min(std::max(actual_K, 1), ARRAY_SIZE);  // 每列K个元素
        
        if(current_col < effective_cols) {
            // 使能当前列的有效行PE权重写入（只加载K行，不是全部ARRAY_SIZE行）
            for(int i = 0; i < effective_rows; i++) {
                w_enable_sig[i][current_col].write(true);
            }
            
            // cout << sc_time_stamp() << " [PEA-VarMatrix] 加载权重列 " << (current_col+1) 
            //      << "/" << effective_cols << " (M=" << actual_M << ", K=" << actual_K << ")" << endl;
            
            // 更新列计数
            w_load_col_cnt.write(current_col + 1);
        }
        
        if(current_col >= effective_cols - 1) {
            // 🚀 变长矩阵支持：有效列加载完成
            w_load_active.write(false);
            w_load_done_o.write(true);
            
            cout << sc_time_stamp() << " [PEA-VarMatrix] 权重加载完成 (" << effective_cols 
                 << "列，每列" << effective_rows << "个元素，M=" << actual_M << ", K=" << actual_K << ")" << endl;
        }
    }
    else {
        // 非加载周期，清除权重使能信号
        for(int i = 0; i < ARRAY_SIZE; i++) {
            for(int j = 0; j < ARRAY_SIZE; j++) {
                w_enable_sig[i][j].write(false);
            }
        }
    }
}

// ====== 计算控制逻辑 ======
template<typename T, int ARRAY_SIZE, int FIFO_DEPTH>
void PEA<T, ARRAY_SIZE, FIFO_DEPTH>::compute_control() {
    if(rst_i.read() == false) {
        // 复位状态
        compute_active.write(false);
        compute_done_o.write(false);
        b_rd_start_sig.write(false);
        d_rd_start_sig.write(false);
    }
    else if(compute_start_i.read() && !compute_active.read()) {
        // 启动计算
        compute_active.write(true);
        compute_done_o.write(false);
        
        // 启动FIFO读取（平行四边形数据流）
        b_rd_start_sig.write(true);
        d_rd_start_sig.write(true);
        
        cout << sc_time_stamp() << " [PEA COMPUTE] 🚀 计算启动: compute_active=true, compute_done_o=false" << endl;
        cout << sc_time_stamp() << " [PEA COMPUTE] FIFO读取启动: b_rd_start=true, d_rd_start=true" << endl;
        cout << sc_time_stamp() << " [PEA COMPUTE] 开始监控PE[" << (ARRAY_SIZE-1) << "][" << (ARRAY_SIZE-1) << "]的mac_v_o信号" << endl;
    }
    else if(compute_active.read()) {
        // 🚀 变长矩阵支持 - 动态监控有效区域最后一个PE的mac_v_o下降沿
        static bool last_pe_mac_valid_prev = false;
        
        // 🚀 获取实际矩阵尺寸，计算有效区域的最后一个PE坐标
        int actual_M = matrix_M_i.read();
        int actual_N = matrix_N_i.read();
        int actual_K = matrix_K_i.read();
        
        // 🚀 修正：脉动阵列数据流特性要求监控PE[ARRAY_SIZE-1][actual_M-1]
        int last_row = ARRAY_SIZE - 1;      // 永远监控最后一行PE（数据流终点）
        int last_col = actual_M - 1;        // 监控输出矩阵的最后一列
        
        // 读取有效区域最后一个PE的MAC有效信号
        bool current_mac_valid = pe_array[last_row][last_col]->mac_v_o.read();
        
        // Debug: 打印动态PE坐标的状态变化
        if (current_mac_valid != last_pe_mac_valid_prev) {
            cout << sc_time_stamp() << " [PEA MONITOR] PE[" << last_row << "][" << last_col 
                 << "] mac_v_o: " << last_pe_mac_valid_prev << " -> " << current_mac_valid 
                 << " (Matrix=" << actual_M << "×" << actual_K << "×" << actual_N << ")" << endl;
        }
        
        // 检测下降沿：prev=true, current=false
        if (last_pe_mac_valid_prev && !current_mac_valid) {
            // 检测到有效区域最后一个PE的mac_v_o下降沿，表示计算完成
            compute_active.write(false);
            compute_done_o.write(true);
            b_rd_start_sig.write(false);
            d_rd_start_sig.write(false);
            
            cout << sc_time_stamp() << " [PEA COMPLETE] ✅ 检测到PE[" << last_row << "][" << last_col 
                 << "].mac_v_o下降沿! (变长矩阵: " << actual_M << "×" << actual_K << "×" << actual_N << ")" << endl;
            cout << sc_time_stamp() << " [PEA COMPLETE] 计算完成: compute_done_o=true, compute_active=false" << endl;
            cout << sc_time_stamp() << " [PEA COMPLETE] 停止FIFO读取: b_rd_start=false, d_rd_start=false" << endl;
        }
        
        // 更新前一状态
        last_pe_mac_valid_prev = current_mac_valid;
        
        // 🔍 Ultra Debug: 700ns后每周期打印最后三行PE的MAC信号
        //debug_last_rows_mac();
    }
}

// ====== 结果读取控制逻辑 ======
template<typename T, int ARRAY_SIZE, int FIFO_DEPTH>
void PEA<T, ARRAY_SIZE, FIFO_DEPTH>::read_result_control() {
    if(rst_i.read() == false) {
        // 复位状态 - 清除所有C读取启动信号
        for(int i = 0; i < ARRAY_SIZE; i++) {
            c_rd_start_sig[i].write(false);
        }
    }
    else {
        // 🚀 信号桥接：将外部输入c_rd_start_i连接到内部信号c_rd_start_sig
        for(int i = 0; i < ARRAY_SIZE; i++) {
            c_rd_start_sig[i].write(c_rd_start_i[i].read());
        }
    }
}

// ====== MAC信号调试逻辑 ======
template<typename T, int ARRAY_SIZE, int FIFO_DEPTH>
void PEA<T, ARRAY_SIZE, FIFO_DEPTH>::debug_last_rows_mac() {
    // 仅在700ns后进行调试，避免过多输出
    sc_time current_time = sc_time_stamp();
    if (current_time < sc_time(700, SC_NS)) {
        return;
    }
    
    // 获取实际矩阵尺寸
    int actual_M = matrix_M_i.read();
    
    // 调试最后三行PE的MAC信号 (PE[13], PE[14], PE[15])
    int start_row = std::max(0, ARRAY_SIZE - 3);
    int end_row = std::min(ARRAY_SIZE - 1, actual_M - 1);
    
    for(int row = start_row; row <= end_row; row++) {
        // 每行选择几个有代表性的列进行调试
        for(int col = 0; col < 4 && col < ARRAY_SIZE; col++) {
            T mac_i_val = pe_array[row][col]->mac_i.read();
            T mac_o_val = pe_array[row][col]->mac_o.read();
            bool mac_v_i = pe_array[row][col]->mac_v_i.read();
            bool mac_v_o = pe_array[row][col]->mac_v_o.read();
            
            cout << current_time << " [MAC-DEBUG] PE[" << row << "][" << col << "] "
                 << "mac_i=" << std::fixed << std::setprecision(2) << mac_i_val 
                 << "(v=" << mac_v_i << ") -> "
                 << "mac_o=" << mac_o_val << "(v=" << mac_v_o << ")" << endl;
        }
    }
}

#endif // PEA_IMPL_H