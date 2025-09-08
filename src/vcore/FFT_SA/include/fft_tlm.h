/**
 * @file fft_tlm.h
 * @brief TLM2.0 FFT顶层模块接口
 * 
 * 功能特性：
 * - 封装fft_multi_stage模块
 * - 提供TLM2.0标准接口
 * - 支持配置和数据传输事务
 * - 内存映射寄存器配置
 * 
 * @version 1.0
 * @date 2025-01-01
 */

 #ifndef FFT_TLM_H
 #define FFT_TLM_H
 
#include "systemc.h"
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "fft_multi_stage.h"
#include "../utils/complex_types.h"
#include "../utils/config.h"
#include <queue>
#include <vector>
 
 using namespace tlm;
 using namespace sc_core;
 
 /**
  * @brief TLM事务载荷扩展 - FFT专用数据
  */
 struct FftPayloadExtension : public tlm_extension<FftPayloadExtension> {
     enum CommandType {
         CMD_CONFIG,      // 配置命令
         CMD_LOAD_TWIDDLE, // 加载Twiddle因子
         CMD_PROCESS_DATA, // 处理数据
         CMD_READ_STATUS,  // 读取状态
         CMD_READ_RESULT   // 读取结果
     };
     
     CommandType command;
     unsigned stage_idx;
     unsigned pe_idx;
     std::vector<complex<float>> data;
     
     // TLM扩展必需的方法
     virtual tlm_extension_base* clone() const {
         FftPayloadExtension* ext = new FftPayloadExtension;
         ext->command = command;
         ext->stage_idx = stage_idx;
         ext->pe_idx = pe_idx;
         ext->data = data;
         return ext;
     }
     
     virtual void copy_from(const tlm_extension_base& ext) {
         const FftPayloadExtension& other = static_cast<const FftPayloadExtension&>(ext);
         command = other.command;
         stage_idx = other.stage_idx;
         pe_idx = other.pe_idx;
         data = other.data;
     }
 };
 
 /**
  * @brief FFT TLM2.0顶层模块
  * @tparam T 数据类型
  * @tparam N FFT大小
  */
 template<typename T, unsigned N>
 SC_MODULE(FftTlm) {
     static_assert(N >= 2 && ((N & (N-1)) == 0), "N must be power of two");
     static constexpr unsigned NUM_PES = N/2;
     static constexpr unsigned NUM_STAGES = log2_const(N);
     
     // ====== TLM接口 ======
     tlm_utils::simple_target_socket<FftTlm> tgt_socket{"tgt_socket"};
     
     // ====== 内存映射寄存器地址定义 ======
     enum RegisterMap {
         // 控制寄存器
         REG_CTRL        = 0x0000,  // [0]: rst, [1]: fft_mode, [2]: start
         REG_FFT_SHIFT   = 0x0004,  // FFT移位配置
         REG_FFT_CONJ    = 0x0008,  // FFT共轭使能
         REG_BYPASS_EN   = 0x000C,  // 各级bypass使能
         REG_STATUS      = 0x0010,  // [0]: busy, [1]: done, [2]: error
         
         // Twiddle配置
         REG_TW_CTRL     = 0x0020,  // [7:0]: pe_idx, [15:8]: stage_idx, [16]: load_en
         REG_TW_DATA_RE  = 0x0024,  // Twiddle实部
         REG_TW_DATA_IM  = 0x0028,  // Twiddle虚部
         
         // 数据输入基地址
         REG_INPUT_A_BASE = 0x1000,  // 输入A数据 (NUM_PES个复数)
         REG_INPUT_B_BASE = 0x2000,  // 输入B数据 (NUM_PES个复数)
         
         // 数据输出基地址
         REG_OUTPUT_Y0_BASE = 0x3000, // 输出Y0数据 (NUM_PES个复数)
         REG_OUTPUT_Y1_BASE = 0x4000  // 输出Y1数据 (NUM_PES个复数)
     };
     
     // ====== 内部组件 ======
     FftMultiStage<T, N>* fft_core;
     
     // ====== 内部信号 ======
     sc_clock internal_clk{"internal_clk", 10, SC_NS};
     sc_signal<bool> rst_sig{"rst_sig"};
     sc_signal<bool> fft_mode_sig{"fft_mode_sig"};
     sc_signal<sc_uint<4>> fft_shift_sig{"fft_shift_sig"};
     sc_signal<bool> fft_conj_en_sig{"fft_conj_en_sig"};
     sc_vector<sc_signal<bool>> stage_bypass_en_sig{"stage_bypass_en_sig", NUM_STAGES};
     
     // 输入数据信号
     sc_vector<sc_signal<complex<T>>> in_a_sig{"in_a_sig", NUM_PES};
     sc_vector<sc_signal<complex<T>>> in_b_sig{"in_b_sig", NUM_PES};
     sc_vector<sc_signal<bool>> in_a_v_sig{"in_a_v_sig", NUM_PES};
     sc_vector<sc_signal<bool>> in_b_v_sig{"in_b_v_sig", NUM_PES};
     
     // 输出数据信号
     sc_vector<sc_signal<complex<T>>> out_y0_sig{"out_y0_sig", NUM_PES};
     sc_vector<sc_signal<complex<T>>> out_y1_sig{"out_y1_sig", NUM_PES};
     sc_vector<sc_signal<bool>> out_y0_v_sig{"out_y0_v_sig", NUM_PES};
     sc_vector<sc_signal<bool>> out_y1_v_sig{"out_y1_v_sig", NUM_PES};
     
     // Twiddle控制信号
     sc_signal<bool> tw_load_en_sig{"tw_load_en_sig"};
     sc_signal<sc_uint<8>> tw_stage_idx_sig{"tw_stage_idx_sig"};
     sc_signal<sc_uint<8>> tw_pe_idx_sig{"tw_pe_idx_sig"};
     sc_signal<complex<T>> tw_data_sig{"tw_data_sig"};
     
     // ====== 内部状态 ======
     struct FftState {
         bool busy;
         bool done;
         bool error;
         unsigned current_stage;
         unsigned cycle_count;
     } state;
     
     // 配置寄存器
     struct ConfigRegs {
         bool reset;
         bool fft_mode;
         bool start;
         sc_uint<4> fft_shift;
         bool fft_conj_en;
         sc_uint<32> stage_bypass_mask;
         
         // Bypass相关配置
         unsigned effective_fft_size;    // 当前有效FFT点数
         unsigned active_stages;         // 激活的stage数量
         unsigned bypass_stage_count;    // bypass的stage数量
     } config;
     
     // 数据缓冲区
     std::vector<complex<T>> input_buffer_a;
     std::vector<complex<T>> input_buffer_b;
     std::vector<complex<T>> output_buffer_y0;
     std::vector<complex<T>> output_buffer_y1;
     
     // 输入数据队列
     std::queue<std::pair<std::vector<complex<T>>, std::vector<complex<T>>>> input_queue;
     
 public:
     // ====== 构造函数 ======
     SC_CTOR(FftTlm);
     
     // ====== TLM接口方法 ======
     virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
     virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, 
                                           tlm_phase& phase, 
                                           sc_time& delay);
     virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, 
                                     tlm_dmi& dmi_data);
     virtual unsigned int transport_dbg(tlm_generic_payload& trans);
     
 private:
     // ====== 内部方法 ======
     void process_control_register(uint32_t addr, uint32_t data, bool is_write);
     void process_twiddle_config(uint32_t addr, uint32_t data, bool is_write);
     void process_data_transfer(uint32_t addr, uint8_t* data, unsigned len, bool is_write);
     void start_fft_processing();
     void monitor_process();
     void feed_input_process();
     void collect_output_process();
     bool check_processing_complete();
     void reset_module();
     
     // 辅助方法
     uint32_t read_register(uint32_t addr);
     void write_register(uint32_t addr, uint32_t value);
     sc_time estimate_processing_time(unsigned data_size);
     
     // Bypass相关方法
     void update_bypass_configuration();
     unsigned calculate_effective_fft_size();
     void setup_data_mapping_for_bypass(unsigned effective_size);
     bool validate_bypass_configuration();
 };
 
 // ====== 便捷类型定义 ======
 typedef FftTlm<float, 4> FftTlm4;
 typedef FftTlm<float, 8> FftTlm8;
 typedef FftTlm<float, 16> FftTlm16;
 typedef FftTlm<float, 32> FftTlm32;
 typedef FftTlm<float, 64> FftTlm64;
 
 #endif // FFT_TLM_H