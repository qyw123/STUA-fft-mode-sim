#ifndef CONST_H
#define CONST_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <iomanip>
#include <cmath>
using namespace sc_core;
using namespace sc_dt;
using namespace std;
using namespace tlm;

const int FFT_TLM_N = 16;                    //FFT模式阵列一次处理帧长度,决定了模型中阵列的规模
const int FFT_TLM_buf_depth = 8;
// const int TEST_FFT_SIZE = 4;     //实际测试的 FFT点数

//中转模块的目标个数，指令通过SPU转发给CAC、DMA和VPU；CAC连接DMA(Vcore)、DDR、GSM；DMA连接CAC、AM和SM
const uint64_t DDR_id = 0;
const uint64_t N_TARGETS = 2;
const uint64_t GSM_id = 1;
const uint64_t VCore_id = 2;

// Complex types are now defined in src/vcore/FFT_SA/utils/complex_types.h
//系统频率1GHz,时钟周期1ns
const sc_time SYSTEM_CLOCK = sc_time(1, SC_NS);

// DDR configurations
const uint64_t DDR_BASE_ADDR = 0x080000000;  // DDR base address
const uint64_t DDR_SIZE = 16L * 1024 * 1024 * 1024;  // DDR size (16GB)
const uint64_t DDR_DATA_WIDTH = 64;  //每拍传输的字节数
// GSM configurations
const uint64_t GSM_BASE_ADDR = 0x070000000;  // GSM base address
const uint64_t GSM_SIZE = 8L * 1024 * 1024;  // GSM size (8MB)
const uint64_t GSM_DATA_WIDTH = 64;  //每拍传输的字节数
// VCore configurations
const uint64_t VCORE_BASE_ADDR = 0x010000000;  // VCore base address,000000-3fffff
const uint64_t VCORE_SIZE = 4L * 1024 * 1024;  // VCore size (4MB)
//SPU configurations(SPU和SM暂时分开设置)
const uint64_t SPU_BASE_ADDR = 0x010000000;  // SPU base address,00000-fffff
const uint64_t SPU_SIZE = 64L * 1024 ;  // SPU size (64KB)
// SM configurations(SPU和SM暂时分开设置)
const uint64_t SM_BASE_ADDR = 0x010010000;  // SM base address,010000-02ffff
const uint64_t SM_SIZE = 128L * 1024 ;  // SM size (128KB)
// AM configurations
const uint64_t AM_BASE_ADDR = 0x010030000;  // AM base address,030000-0effff
const uint64_t AM_SIZE = 768L * 1024 ;  // AM size (768KB)
const uint64_t SM_AM_DATA_WIDTH = 64;  //每拍传输的字节数
//DMA configurations
const uint64_t DMA_BASE_ADDR = 0x0100f0000;  // DMA base address,0f0000-0fffff
const uint64_t DMA_SIZE = 63L * 1024 ;  // DMA size (63KB)
// MAC configurations,在VCore中，且不影响AM和SM的空间
const uint64_t VPU_BASE_ADDR = 0x010100000;  // MAC base address,100000-10ffff
const uint64_t VPU_REGISTER_SIZE = 64L * 64 ;  // 64个64位寄存器
const uint64_t MAC_PER_VPU = 64;

//PEA configurations
const uint64_t PEA_BASE_ADDR = 0x010110000;  // PEA base address,110000-11ffff
const uint64_t PEA_SIZE = 64L * 1024 ;  // PEA size (64KB)

//FFT_TLM configurations
const uint64_t FFT_BASE_ADDR = 0x010120000;  // FFT_TLM base address,120000-12ffff
const uint64_t FFT_SIZE = 64L * 1024 ;  // FFT_TLM size (64KB)

//延迟
const sc_time DDR_LATENCY=sc_time(4, SC_NS);
const sc_time GSM_LATENCY=sc_time(8, SC_NS);
const sc_time SM_LATENCY=sc_time(8, SC_NS);
const sc_time AM_LATENCY=sc_time(4, SC_NS);
const sc_time MAC_LATENCY = sc_time(18, SC_NS); 
const sc_time ADD_LATENCY = sc_time(2, SC_NS);
const sc_time SUB_LATENCY = sc_time(2, SC_NS);

//GEMM分块参数
const int cu_max = 64;
const int k_gsm_max = 384;
const int m_gsm_max = 384;
const int sm_max = 12;

#endif
