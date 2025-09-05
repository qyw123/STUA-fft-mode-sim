#ifndef CONFIG_H
#define CONFIG_H
    
// ======  极简延时配置 ======
const int FFT_OPERATION_CYCLES = 20;     // FFT蝶型运算延时(单位：拍)
const int GEMM_OPERATION_CYCLES = 9;     // GEMM MAC运算延时(单位：拍)
const int SHUFFLE_OPERATION_CYCLES = 2;  // 混洗运算延时(单位：拍)

// ======  FFT TLM 时序控制配置 ======
// 系统复位和初始化延时
const int FFT_RESET_ASSERT_CYCLES = 3;        // 复位信号有效持续时间
const int FFT_RESET_DEASSERT_CYCLES = 5;      // 复位释放稳定时间
const int FFT_INIT_STARTUP_CYCLES = 1;        // 初始化启动延时

// FFT配置和控制延时
const int FFT_CONFIG_SETUP_CYCLES = 1;        // FFT模式配置建立时间
const int FFT_TWIDDLE_LOAD_CYCLES = 1;        // 单个旋转因子加载时间
const int FFT_TWIDDLE_STABILIZE_CYCLES = 10;  // 旋转因子加载后稳定时间

// 数据缓冲区操作延时
const int FFT_INPUT_WRITE_SETUP_CYCLES = 1;   // 输入写使能建立时间
const int FFT_INPUT_WRITE_HOLD_CYCLES = 2;    // 输入写信号保持时间
const int FFT_OUTPUT_READ_SETUP_CYCLES = 4;   // 输出读启动建立时间
const int FFT_OUTPUT_READ_HOLD_CYCLES = 1;    // 输出读信号保持时间

// FFT处理管线延时
const int FFT_START_PULSE_CYCLES = 4;         // FFT启动脉冲宽度
const int FFT_START_ACTIVE_CYCLES = 3;        // FFT启动信号有效时间
const int FFT_INPUT_BUFFER_CYCLES = 10;       // 输入缓冲延时
const int FFT_PIPELINE_PROCESSING_CYCLES = 30; // FFT管线处理延时(8点FFT)
const int FFT_OUTPUT_BUFFER_CYCLES = 10;      // 输出缓冲延时
const int FFT_PIPELINE_MARGIN_CYCLES = 20;    // 管线处理余量

// 管线控制和监控延时
const int FFT_PIPELINE_MONITOR_CYCLES = 10;   // 管线状态监控周期
const int FFT_FRAME_PROCESSING_CYCLES = 5;    // 帧处理控制延时
const int FFT_PIPELINE_COMPLETE_CYCLES = 50;  // 管线处理完成等待时间

// TLM接口响应延时
const int FFT_TLM_RESET_CYCLES = 5;           // TLM复位命令响应延时
const int FFT_TLM_CONFIG_CYCLES = 5;          // TLM配置命令响应延时
const int FFT_TLM_TWIDDLE_CYCLES = 15;        // TLM旋转因子加载响应延时
const int FFT_TLM_INPUT_CYCLES = 10;          // TLM输入数据写入响应延时
const int FFT_TLM_PROCESSING_CYCLES = 50;     // TLM FFT处理命令响应延时
const int FFT_TLM_OUTPUT_CYCLES = 15;         // TLM输出数据读取响应延时
const int FFT_TLM_STATUS_CYCLES = 1;          // TLM状态查询响应延时
const int FFT_TLM_PARAM_CYCLES = 2;           // TLM参数设置响应延时

#endif