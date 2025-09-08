/**
 * @file fft_tlm.cpp
 * @brief TLM2.0 FFT顶层模块实现
 * 
 * @version 1.0
 * @date 2025-01-01
 */

 #include "fft_tlm.h"
 #include <cstring>
 
 // ====== 构造函数实现 ======
 template<typename T, unsigned N>
 FftTlm<T,N>::FftTlm(sc_module_name name) : sc_module(name) {
     // 绑定TLM socket
     tgt_socket.register_b_transport(this, &FftTlm::b_transport);
     tgt_socket.register_nb_transport_fw(this, &FftTlm::nb_transport_fw);
     tgt_socket.register_get_direct_mem_ptr(this, &FftTlm::get_direct_mem_ptr);
     tgt_socket.register_transport_dbg(this, &FftTlm::transport_dbg);
     
     // 创建FFT核心模块
     fft_core = new FftMultiStage<T,N>("fft_core");
     
     // 连接时钟和控制信号
     fft_core->clk_i(internal_clk);
     fft_core->rst_i(rst_sig);
     fft_core->fft_mode_i(fft_mode_sig);
     fft_core->fft_shift_i(fft_shift_sig);
     fft_core->fft_conj_en_i(fft_conj_en_sig);
     
     // 连接bypass信号
     for (unsigned i = 0; i < NUM_STAGES; ++i) {
         fft_core->stage_bypass_en[i](stage_bypass_en_sig[i]);
     }
     
     // 连接数据输入信号
     for (unsigned i = 0; i < NUM_PES; ++i) {
         fft_core->in_a[i](in_a_sig[i]);
         fft_core->in_b[i](in_b_sig[i]);
         fft_core->in_a_v[i](in_a_v_sig[i]);
         fft_core->in_b_v[i](in_b_v_sig[i]);
         
         fft_core->out_y0[i](out_y0_sig[i]);
         fft_core->out_y1[i](out_y1_sig[i]);
         fft_core->out_y0_v[i](out_y0_v_sig[i]);
         fft_core->out_y1_v[i](out_y1_v_sig[i]);
     }
     
     // 连接Twiddle控制信号
     fft_core->tw_load_en(tw_load_en_sig);
     fft_core->tw_stage_idx(tw_stage_idx_sig);
     fft_core->tw_pe_idx(tw_pe_idx_sig);
     fft_core->tw_data(tw_data_sig);
     
     // 初始化缓冲区
     input_buffer_a.resize(NUM_PES);
     input_buffer_b.resize(NUM_PES);
     output_buffer_y0.resize(NUM_PES);
     output_buffer_y1.resize(NUM_PES);
     
     // 初始化状态
     state.busy = false;
     state.done = false;
     state.error = false;
     state.current_stage = 0;
     state.cycle_count = 0;
     
     // 初始化配置
     config.reset = true;
     config.fft_mode = false;
     config.start = false;
     config.fft_shift = 0;
     config.fft_conj_en = false;
     config.stage_bypass_mask = 0;
     
     // 初始化bypass相关配置
     config.effective_fft_size = N;        // 默认全规模FFT
     config.active_stages = NUM_STAGES;    // 默认所有stage都激活
     config.bypass_stage_count = 0;        // 默认不bypass任何stage
     
     // 启动监控进程
     SC_THREAD(monitor_process);
     sensitive << internal_clk.posedge_event();
     
     // 启动数据馈送进程 (暂时禁用，直接在start_fft_processing中驱动)
     // SC_THREAD(feed_input_process);
     // sensitive << internal_clk.posedge_event();
     
     // 启动输出收集进程
     SC_THREAD(collect_output_process);
     sensitive << internal_clk.posedge_event();
     
     // 设置正确的初始复位状态
     rst_sig.write(true);  // 正常工作状态
     
     // 初始复位
     reset_module();
 }
 
 // ====== TLM阻塞传输实现 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::b_transport(tlm_generic_payload& trans, sc_time& delay) {
     tlm_command cmd = trans.get_command();
     sc_dt::uint64 addr = trans.get_address();
     unsigned char* data_ptr = trans.get_data_ptr();
     unsigned int len = trans.get_data_length();
     
     // 检查地址范围
     if (addr > 0x5000) {
         trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
         return;
     }
     
     // 处理扩展命令
     FftPayloadExtension* ext;
     trans.get_extension(ext);
     
     if (ext != nullptr) {
         // 处理扩展命令
         switch (ext->command) {
             case FftPayloadExtension::CMD_CONFIG:
                 // 配置FFT参数
                 config.fft_mode = (ext->data[0].real > 0.5);
                 config.fft_shift = static_cast<sc_uint<4>>(ext->data[1].real);
                 config.fft_conj_en = (ext->data[2].real > 0.5);
                 trans.set_response_status(TLM_OK_RESPONSE);
                 break;
                 
             case FftPayloadExtension::CMD_LOAD_TWIDDLE:
                 // 加载Twiddle因子
                 tw_stage_idx_sig.write(ext->stage_idx);
                 tw_pe_idx_sig.write(ext->pe_idx);
                 tw_data_sig.write(ext->data[0]);
                 tw_load_en_sig.write(true);
                 wait(internal_clk.period());
                 tw_load_en_sig.write(false);
                 trans.set_response_status(TLM_OK_RESPONSE);
                 break;
                 
             case FftPayloadExtension::CMD_PROCESS_DATA:
                 // 处理数据
                 if (!state.busy) {
                     // 加载输入数据到队列
                     std::vector<complex<T>> data_a(ext->data.begin(), ext->data.begin() + NUM_PES);
                     std::vector<complex<T>> data_b(ext->data.begin() + NUM_PES, ext->data.end());
                     input_queue.push(std::make_pair(data_a, data_b));
                     
                     // 启动处理
                     if (!config.start) {
                         config.start = true;
                         start_fft_processing();
                     }
                     trans.set_response_status(TLM_OK_RESPONSE);
                 } else {
                     trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
                 }
                 break;
                 
             case FftPayloadExtension::CMD_READ_STATUS:
                 // 读取状态
                 *reinterpret_cast<uint32_t*>(data_ptr) = 
                     (state.error << 2) | (state.done << 1) | state.busy;
                 trans.set_response_status(TLM_OK_RESPONSE);
                 break;
                 
             case FftPayloadExtension::CMD_READ_RESULT:
                 // 读取结果
                 if (state.done) {
                     ext->data.clear();
                     ext->data.insert(ext->data.end(), output_buffer_y0.begin(), output_buffer_y0.end());
                     ext->data.insert(ext->data.end(), output_buffer_y1.begin(), output_buffer_y1.end());
                     state.done = false;  // 清除完成标志
                     trans.set_response_status(TLM_OK_RESPONSE);
                 } else {
                     trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
                 }
                 break;
                 
             default:
                 trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
                 break;
         }
     } else {
         // 标准内存映射访问
         if (cmd == TLM_WRITE_COMMAND) {
             // 写操作
             if (addr < 0x100) {
                 // 控制寄存器区域
                 uint32_t value = *reinterpret_cast<uint32_t*>(data_ptr);
                 process_control_register(addr, value, true);
             } else if (addr >= REG_INPUT_A_BASE && addr < REG_INPUT_B_BASE) {
                 // 输入A数据
                 process_data_transfer(addr, data_ptr, len, true);
             } else if (addr >= REG_INPUT_B_BASE && addr < REG_OUTPUT_Y0_BASE) {
                 // 输入B数据
                 process_data_transfer(addr, data_ptr, len, true);
             }
             trans.set_response_status(TLM_OK_RESPONSE);
         } else if (cmd == TLM_READ_COMMAND) {
             // 读操作
             if (addr < 0x100) {
                 // 控制寄存器区域
                 uint32_t value = read_register(addr);
                 *reinterpret_cast<uint32_t*>(data_ptr) = value;
             } else if (addr >= REG_OUTPUT_Y0_BASE && addr < REG_OUTPUT_Y1_BASE) {
                 // 输出Y0数据
                 process_data_transfer(addr, data_ptr, len, false);
             } else if (addr >= REG_OUTPUT_Y1_BASE && addr < 0x5000) {
                 // 输出Y1数据
                 process_data_transfer(addr, data_ptr, len, false);
             }
             trans.set_response_status(TLM_OK_RESPONSE);
         }
     }
     
     // 添加处理延时
     delay += estimate_processing_time(len);
 }
 
 // ====== TLM非阻塞传输实现 ======
 template<typename T, unsigned N>
 tlm_sync_enum FftTlm<T,N>::nb_transport_fw(tlm_generic_payload& trans, 
                                            tlm_phase& phase, 
                                            sc_time& delay) {
     if (phase == BEGIN_REQ) {
         // 检查是否忙碌
         if (state.busy) {
             // 返回忙碌，稍后重试
             phase = END_REQ;
             return TLM_UPDATED;
         }
         
         // 接受请求
         phase = END_REQ;
         
         // 异步处理请求
         b_transport(trans, delay);
         
         // 准备响应
         phase = BEGIN_RESP;
         return TLM_COMPLETED;
     }
     
     return TLM_ACCEPTED;
 }
 
 // ====== 直接内存访问实现 ======
 template<typename T, unsigned N>
 bool FftTlm<T,N>::get_direct_mem_ptr(tlm_generic_payload& trans, 
                                      tlm_dmi& dmi_data) {
     // FFT模块不支持直接内存访问
     return false;
 }
 
 // ====== 调试传输实现 ======
 template<typename T, unsigned N>
 unsigned int FftTlm<T,N>::transport_dbg(tlm_generic_payload& trans) {
     // 简单处理，直接调用阻塞传输
     sc_time delay = SC_ZERO_TIME;
     b_transport(trans, delay);
     return trans.get_data_length();
 }
 
 // ====== 控制寄存器处理 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::process_control_register(uint32_t addr, uint32_t data, bool is_write) {
     if (is_write) {
         switch (addr) {
             case REG_CTRL:
                 config.reset = (data & 0x01);
                 config.fft_mode = (data & 0x02) >> 1;
                 config.start = (data & 0x04) >> 2;
                 
                 cout << "[TLM_CTRL] " << sc_time_stamp() << " 控制寄存器更新: " 
                      << "reset=" << config.reset << ", fft_mode=" << config.fft_mode 
                      << ", start=" << config.start << endl;
                 
                 // 立即更新FFT模式信号，以便PE能正确接收Twiddle因子
                 fft_mode_sig.write(config.fft_mode);
                 
                 if (config.reset) {
                     reset_module();
                 }
                 if (config.start && !state.busy) {
                     start_fft_processing();
                 }
                 break;
                 
             case REG_FFT_SHIFT:
                 config.fft_shift = data & 0x0F;
                 fft_shift_sig.write(config.fft_shift);
                 break;
                 
             case REG_FFT_CONJ:
                 config.fft_conj_en = (data & 0x01);
                 fft_conj_en_sig.write(config.fft_conj_en);
                 break;
                 
             case REG_BYPASS_EN:
                 config.stage_bypass_mask = data;
                 for (unsigned i = 0; i < NUM_STAGES; ++i) {
                     stage_bypass_en_sig[i].write((data >> i) & 0x01);
                 }
                 // 更新bypass配置
                 update_bypass_configuration();
                 cout << sc_time_stamp() << " " << name() 
                      << " Bypass配置更新: mask=0x" << hex << data << dec 
                      << ", 有效FFT点数=" << config.effective_fft_size << endl;
                 break;
                 
             case REG_TW_CTRL:
                 tw_pe_idx_sig.write(data & 0xFF);
                 tw_stage_idx_sig.write((data >> 8) & 0xFF);
                 tw_load_en_sig.write((data >> 16) & 0x01);
                 
                 cout << "[TLM_TWIDDLE] " << sc_time_stamp() << " 写入Twiddle控制寄存器: " 
                      << "pe_idx=" << (data & 0xFF) << ", stage_idx=" << ((data >> 8) & 0xFF) 
                      << ", load_en=" << ((data >> 16) & 0x01) << endl;
                 break;
                 
             case REG_TW_DATA_RE:
                 {
                     complex<T> current = tw_data_sig.read();
                     float re = *reinterpret_cast<float*>(&data);
                     tw_data_sig.write(complex<T>(re, current.imag));
                 }
                 break;
                 
             case REG_TW_DATA_IM:
                 {
                     complex<T> current = tw_data_sig.read();
                     float im = *reinterpret_cast<float*>(&data);
                     tw_data_sig.write(complex<T>(current.real, im));
                 }
                 break;
         }
     }
 }
 
 // ====== 数据传输处理 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::process_data_transfer(uint32_t addr, uint8_t* data, 
                                         unsigned len, bool is_write) {
     if (is_write) {
         // 写入输入数据
         if (addr >= REG_INPUT_A_BASE && addr < REG_INPUT_B_BASE) {
             unsigned offset = (addr - REG_INPUT_A_BASE) / sizeof(complex<T>);
             if (offset < NUM_PES) {
                 complex<T>* complex_data = reinterpret_cast<complex<T>*>(data);
                 input_buffer_a[offset] = *complex_data;
             }
         } else if (addr >= REG_INPUT_B_BASE && addr < REG_OUTPUT_Y0_BASE) {
             unsigned offset = (addr - REG_INPUT_B_BASE) / sizeof(complex<T>);
             if (offset < NUM_PES) {
                 complex<T>* complex_data = reinterpret_cast<complex<T>*>(data);
                 input_buffer_b[offset] = *complex_data;
             }
         }
     } else {
         // 读取输出数据
         if (addr >= REG_OUTPUT_Y0_BASE && addr < REG_OUTPUT_Y1_BASE) {
             unsigned offset = (addr - REG_OUTPUT_Y0_BASE) / sizeof(complex<T>);
             if (offset < NUM_PES) {
                 complex<T>* complex_data = reinterpret_cast<complex<T>*>(data);
                 *complex_data = output_buffer_y0[offset];
             }
         } else if (addr >= REG_OUTPUT_Y1_BASE && addr < 0x5000) {
             unsigned offset = (addr - REG_OUTPUT_Y1_BASE) / sizeof(complex<T>);
             if (offset < NUM_PES) {
                 complex<T>* complex_data = reinterpret_cast<complex<T>*>(data);
                 *complex_data = output_buffer_y1[offset];
             }
         }
     }
 }
 
 // ====== 启动FFT处理 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::start_fft_processing() {
     state.busy = true;
     state.done = false;
     state.error = false;
     state.current_stage = 0;
     state.cycle_count = 0;
     
     // 设置模式
     fft_mode_sig.write(config.fft_mode);
     fft_shift_sig.write(config.fft_shift);
     fft_conj_en_sig.write(config.fft_conj_en);
     
     cout << sc_time_stamp() << " " << this->name() << " 设置FFT模式: " << config.fft_mode << endl;
     
     // 设置bypass配置
     for (unsigned i = 0; i < NUM_STAGES; ++i) {
         bool bypass_this_stage = (config.stage_bypass_mask & (1 << i)) != 0;
         stage_bypass_en_sig[i].write(bypass_this_stage);
     }
     
     // 复位已在reset_module()中处理
     
     // 直接将输入缓冲区数据驱动到FFT输入信号
     for (unsigned i = 0; i < NUM_PES; ++i) {
         in_a_sig[i].write(input_buffer_a[i]);
         in_b_sig[i].write(input_buffer_b[i]);
         cout << "  输入PE[" << i << "] A=" << input_buffer_a[i] << " B=" << input_buffer_b[i] << endl;
     }
     
     // 等待一个时钟周期让数据值稳定传播
     wait(internal_clk.period());
     
     // 验证数据是否正确写入
     cout << "  验证写入的数据值:" << endl;
     for (unsigned i = 0; i < NUM_PES; ++i) {
         cout << "    信号[" << i << "] A=" << in_a_sig[i].read() << " B=" << in_b_sig[i].read() << endl;
     }
     
     // 然后设置有效信号
     for (unsigned i = 0; i < NUM_PES; ++i) {
         in_a_v_sig[i].write(true);
         in_b_v_sig[i].write(true);
     }
     
     cout << sc_time_stamp() << " " << name() 
          << " FFT处理启动 - 模式: " << (config.fft_mode ? "FFT" : "GEMM") << endl;
     cout << "  数据值已稳定，有效信号已设置" << endl;
     
     // 等待更多周期确保PE完全处理完成数据，延长有效信号持续时间
     cout << "  有效信号将持续" << (NUM_STAGES * 10) << "个时钟周期" << endl;
     wait(internal_clk.period() * NUM_STAGES * 10);
     for (unsigned i = 0; i < NUM_PES; ++i) {
         in_a_v_sig[i].write(false);
         in_b_v_sig[i].write(false);
     }
     cout << "  有效信号清除" << endl;
 }
 
 // ====== 监控进程 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::monitor_process() {
     while (true) {
         wait();
         
         if (state.busy) {
             state.cycle_count++;
             
             // 检查是否完成
             if (check_processing_complete()) {
                 state.busy = false;
                 state.done = true;
                 config.start = false;
                 
                 cout << sc_time_stamp() << " " << name() 
                      << " FFT处理完成，用时 " << state.cycle_count << " 周期" << endl;
             }
             
             // 超时检测
             if (state.cycle_count > 10000) {
                 state.busy = false;
                 state.error = true;
                 config.start = false;
                 
                 cout << sc_time_stamp() << " " << name() 
                      << " FFT处理超时错误" << endl;
             }
         }
     }
 }
 
 // ====== 输入数据馈送进程 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::feed_input_process() {
     while (true) {
         wait();
         
         if (state.busy && !input_queue.empty()) {
             // 从队列取出数据
             auto data_pair = input_queue.front();
             input_queue.pop();
             
             // 设置输入信号
             for (unsigned i = 0; i < NUM_PES; ++i) {
                 in_a_sig[i].write(data_pair.first[i]);
                 in_b_sig[i].write(data_pair.second[i]);
                 in_a_v_sig[i].write(true);
                 in_b_v_sig[i].write(true);
             }
             
             // 保持一个周期
             wait();
             
             // 清除有效信号
             for (unsigned i = 0; i < NUM_PES; ++i) {
                 in_a_v_sig[i].write(false);
                 in_b_v_sig[i].write(false);
             }
         }
     }
 }
 
 // ====== 输出数据收集进程 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::collect_output_process() {
     while (true) {
         wait();
         
         if (state.busy) {
             // 检查输出有效信号
             bool any_valid = false;
             for (unsigned i = 0; i < NUM_PES; ++i) {
                 if (out_y0_v_sig[i].read() || out_y1_v_sig[i].read()) {
                     any_valid = true;
                     break;
                 }
             }
             
             // 偶尔打印调试信息
             static int debug_counter = 0;
             if (++debug_counter % 50 == 0 && state.cycle_count > 10) {
                 cout << "[DEBUG] 周期" << state.cycle_count << " 输出有效检查: ";
                 for (unsigned i = 0; i < NUM_PES && i < 4; ++i) {
                     cout << "PE[" << i << "]Y0v=" << (out_y0_v_sig[i].read() ? "1" : "0") 
                          << ",Y1v=" << (out_y1_v_sig[i].read() ? "1" : "0") << " ";
                 }
                 cout << endl;
             }
             
             if (any_valid) {
                 // 收集输出数据
                 for (unsigned i = 0; i < NUM_PES; ++i) {
                     if (out_y0_v_sig[i].read()) {
                         output_buffer_y0[i] = out_y0_sig[i].read();
                     }
                     if (out_y1_v_sig[i].read()) {
                         output_buffer_y1[i] = out_y1_sig[i].read();
                     }
                 }
                 
                 cout << sc_time_stamp() << " " << name() 
                      << " 收集输出数据" << endl;
             }
         }
     }
 }
 
 // ====== 检查处理完成 ======
 template<typename T, unsigned N>
 bool FftTlm<T,N>::check_processing_complete() {
     // 简单的完成检测逻辑
     // 实际应用中应根据具体的FFT流水线深度和配置来判断
     
     if (config.fft_mode) {
         // FFT模式：检查是否经过足够的周期
         // FFT需要log2(N)级，每级有延时
         unsigned expected_cycles = NUM_STAGES * (FFT_OPERATION_CYCLES + SHUFFLE_OPERATION_CYCLES + 2) + 10;
         return (state.cycle_count >= expected_cycles);
     } else {
         // GEMM模式：检查是否经过足够的周期
         unsigned expected_cycles = GEMM_OPERATION_CYCLES + 10;
         return (state.cycle_count >= expected_cycles);
     }
 }
 
 // ====== 模块复位 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::reset_module() {
     // 只重置控制信号，不影响复位信号状态
     fft_mode_sig.write(false);
     fft_shift_sig.write(0);
     fft_conj_en_sig.write(false);
     tw_load_en_sig.write(false);
     tw_stage_idx_sig.write(0);
     tw_pe_idx_sig.write(0);
     tw_data_sig.write(complex<T>(0, 0));
     
     for (unsigned i = 0; i < NUM_STAGES; ++i) {
         stage_bypass_en_sig[i].write(false);
     }
     
     cout << sc_time_stamp() << " " << this->name() << " 控制寄存器复位完成" << endl;
     
     // 暂时不清除数据信号，避免覆盖start_fft_processing设置的数据
     for (unsigned i = 0; i < NUM_PES; ++i) {
         // in_a_sig[i].write(complex<T>(0, 0));  // 注释掉，避免清除数据
         // in_b_sig[i].write(complex<T>(0, 0));  // 注释掉，避免清除数据
         in_a_v_sig[i].write(false);  // 只清除有效信号
         in_b_v_sig[i].write(false);
     }
     
     // 清空队列和缓冲区
     while (!input_queue.empty()) {
         input_queue.pop();
     }
     
     std::fill(input_buffer_a.begin(), input_buffer_a.end(), complex<T>(0, 0));
     std::fill(input_buffer_b.begin(), input_buffer_b.end(), complex<T>(0, 0));
     std::fill(output_buffer_y0.begin(), output_buffer_y0.end(), complex<T>(0, 0));
     std::fill(output_buffer_y1.begin(), output_buffer_y1.end(), complex<T>(0, 0));
     
     // 复位状态
     state.busy = false;
     state.done = false;
     state.error = false;
     state.current_stage = 0;
     state.cycle_count = 0;
     
     cout << sc_time_stamp() << " " << name() << " 模块复位完成" << endl;
 }
 
 // ====== 读取寄存器 ======
 template<typename T, unsigned N>
 uint32_t FftTlm<T,N>::read_register(uint32_t addr) {
     switch (addr) {
         case REG_CTRL:
             return (config.start << 2) | (config.fft_mode << 1) | config.reset;
             
         case REG_FFT_SHIFT:
             return config.fft_shift;
             
         case REG_FFT_CONJ:
             return config.fft_conj_en;
             
         case REG_BYPASS_EN:
             return config.stage_bypass_mask;
             
         case REG_STATUS:
             return (state.error << 2) | (state.done << 1) | state.busy;
             
         case REG_TW_CTRL:
             return (tw_load_en_sig.read() << 16) | 
                    (tw_stage_idx_sig.read() << 8) | 
                    tw_pe_idx_sig.read();
             
         case REG_TW_DATA_RE:
             {
                 float re = tw_data_sig.read().real;
                 return *reinterpret_cast<uint32_t*>(&re);
             }
             
         case REG_TW_DATA_IM:
             {
                 float im = tw_data_sig.read().imag;
                 return *reinterpret_cast<uint32_t*>(&im);
             }
             
         default:
             return 0;
     }
 }
 
 // ====== 写入寄存器 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::write_register(uint32_t addr, uint32_t value) {
     process_control_register(addr, value, true);
 }
 
 // ====== 估算处理时间 ======
 template<typename T, unsigned N>
 sc_time FftTlm<T,N>::estimate_processing_time(unsigned data_size) {
     // 基于数据大小和操作类型估算处理时间
     if (config.fft_mode) {
         // FFT模式
         unsigned cycles = NUM_STAGES * (FFT_OPERATION_CYCLES + SHUFFLE_OPERATION_CYCLES);
         return internal_clk.period() * cycles;
     } else {
         // GEMM模式
         unsigned cycles = GEMM_OPERATION_CYCLES * (data_size / sizeof(complex<T>));
         return internal_clk.period() * cycles;
     }
 }
 
 // ====== Bypass相关方法实现 ======
 template<typename T, unsigned N>
 void FftTlm<T,N>::update_bypass_configuration() {
     config.bypass_stage_count = __builtin_popcount(config.stage_bypass_mask);
     config.active_stages = NUM_STAGES - config.bypass_stage_count;
     config.effective_fft_size = calculate_effective_fft_size();
     
     if (!validate_bypass_configuration()) {
         cout << "警告: Bypass配置无效，回退到默认配置" << endl;
         config.stage_bypass_mask = 0;
         config.bypass_stage_count = 0;
         config.active_stages = NUM_STAGES;
         config.effective_fft_size = N;
     }
 }
 
 template<typename T, unsigned N>
 unsigned FftTlm<T,N>::calculate_effective_fft_size() {
     // bypass的stage从低位开始计算，有效FFT大小 = 2^(激活的stage数)
     if (config.active_stages == 0) {
         return 1;  // 全部bypass，只剩1点
     }
     return (1U << config.active_stages);  // 2^active_stages
 }
 
 template<typename T, unsigned N>
 void FftTlm<T,N>::setup_data_mapping_for_bypass(unsigned effective_size) {
     // 根据有效FFT大小调整数据映射
     // 这里主要是逻辑记录，实际数据映射由测试端处理
     cout << sc_time_stamp() << " " << name() 
          << " 数据映射配置: N=" << N << " -> 有效=" << effective_size << endl;
          
     // 对于小点数FFT，只使用部分PE
     unsigned effective_pes = effective_size / 2;
     cout << "激活PE数量: " << effective_pes << "/" << NUM_PES << endl;
 }
 
 template<typename T, unsigned N>
 bool FftTlm<T,N>::validate_bypass_configuration() {
     // 验证bypass配置的合理性
     if (config.bypass_stage_count > NUM_STAGES) {
         return false;  // bypass的stage数不能超过总stage数
     }
     
     if (config.effective_fft_size < 2) {
         return false;  // FFT至少需要2点
     }
     
     if (config.effective_fft_size > N) {
         return false;  // 有效大小不能超过硬件规模
     }
     
     // 检查bypass pattern是否合理（应该从低位开始连续bypass）
     uint32_t mask = config.stage_bypass_mask;
     uint32_t expected_mask = (1U << config.bypass_stage_count) - 1;  // 低位连续1
     if (mask != expected_mask) {
         cout << "警告: 建议按顺序bypass前几级stage (mask=0x" << hex << expected_mask << ")" << dec << endl;
     }
     
     return true;
 }

 // ====== 模板显式实例化 ======
 template class FftTlm<float, 4>;
 template class FftTlm<float, 8>;
 template class FftTlm<float, 16>;
 template class FftTlm<float, 32>;
 template class FftTlm<float, 64>;