/**
 * @file fft_pipeline_sim.h
 * @brief FFT多级流水线延时模拟器
 * 
 * 用于估计多帧在FftMultiStage中各级流水线执行时的并行延时。
 * 不修改SystemC硬件模型，作为独立的分析工具。
 * 
 * @version 1.0
 * @date 2025-08-30
 */

#ifndef FFT_PIPELINE_SIM_H
#define FFT_PIPELINE_SIM_H

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include "config.h"

using namespace std;

/**
 * @brief FFT多级流水线延时模拟器
 * 
 * 用于估计多帧在FftMultiStage中各级流水线执行时的并行延时。
 * 不修改SystemC硬件模型，作为独立的分析工具。
 */
class PipelineLatencySimulator {
public:
    struct StageConfig {
        string name;           // Stage名称
        unsigned latency;      // Stage延时(周期数)
        unsigned stage_idx;    // Stage索引
    };
    
    struct FrameTimeline {
        unsigned frame_id;                          // 帧ID
        vector<pair<unsigned, unsigned>> stage_windows; // 各stage的[开始时间, 结束时间]
        unsigned total_start_time;                  // 总开始时间
        unsigned total_completion_time;             // 总完成时间
    };
    
    struct PipelineAnalysis {
        vector<FrameTimeline> frame_timelines;      // 各帧时间线
        unsigned total_pipeline_cycles;             // 流水线总周期数
        float pipeline_throughput;                  // 流水线吞吐率(帧/周期)
        unsigned serial_total_cycles;               // 串行模式总周期数
        float speedup_ratio;                        // 加速比
    };

private:
    vector<StageConfig> stages;                     // 流水线stage配置
    unsigned num_frames;                           // 帧数
    unsigned initiation_interval;                  // 帧间间隔
    
public:
    /**
     * @brief 构造函数 - 配置8点FFT流水线
     * @param frames 测试帧数
     */
    PipelineLatencySimulator(unsigned frames) : num_frames(frames), initiation_interval(1) {
        // 配置8点FFT标准流水线: Stage0->Shuffle0->Stage1->Shuffle1->Stage2
        stages = {
            {"StageRow0", FFT_OPERATION_CYCLES, 0},
            {"Shuffle0", SHUFFLE_OPERATION_CYCLES, 0},
            {"StageRow1", FFT_OPERATION_CYCLES, 1}, 
            {"Shuffle1", SHUFFLE_OPERATION_CYCLES, 1},
            {"StageRow2", FFT_OPERATION_CYCLES, 2}
        };
    }
    
    /**
     * @brief 计算多帧流水线执行时序
     * @return 完整的流水线分析结果
     */
    PipelineAnalysis simulate_pipeline_execution() {
        PipelineAnalysis analysis;
        analysis.frame_timelines.resize(num_frames);
        
        // 计算每帧在各stage的执行时间窗口
        for (unsigned frame = 0; frame < num_frames; frame++) {
            analysis.frame_timelines[frame] = compute_frame_timeline(frame);
        }
        
        // 计算整体性能指标
        compute_performance_metrics(analysis);
        
        return analysis;
    }
    
    /**
     * @brief 设置自定义帧间间隔
     * @param interval 帧间间隔(周期数)
     */
    void set_initiation_interval(unsigned interval) {
        initiation_interval = interval;
    }

    /**
     * @brief 打印流水线分析报告
     * @param analysis 分析结果
     */
    void print_pipeline_analysis_report(const PipelineAnalysis& analysis) {
        cout << "\n" << string(60, '=') << "\n";
        cout << "FFT Multi-Stage Pipeline Latency Analysis Report\n";
        cout << string(60, '=') << "\n";
        
        cout << "\nPipeline Configuration:\n";
        cout << "  Total Frames: " << num_frames << "\n";
        cout << "  Initiation Interval: " << initiation_interval << " cycles\n";
        cout << "  Pipeline Stages: " << stages.size() << "\n";
        for (const auto& stage : stages) {
            cout << "    " << stage.name << ": " << stage.latency << " cycles\n";
        }
        
        cout << "\nPerformance Metrics:\n";
        cout << "  Pipeline Mode Total: " << analysis.total_pipeline_cycles << " cycles\n";
        cout << "  Serial Mode Total: " << analysis.serial_total_cycles << " cycles\n";
        cout << "  Speedup Ratio: " << fixed << setprecision(2) << analysis.speedup_ratio << "x\n";
        cout << "  Pipeline Throughput: " << fixed << setprecision(4) 
             << analysis.pipeline_throughput << " frames/cycle\n";
        
        cout << "\nFrame-by-Frame Timeline:\n";
        print_frame_timeline_table(analysis.frame_timelines);
        
        cout << "\nPipeline Execution Visualization:\n";
        print_pipeline_visualization(analysis.frame_timelines);
        
        cout << "\nAnalysis Complete - Pipeline latency estimation finished!\n";
        cout << string(60, '=') << "\n";
    }

private:
    /**
     * @brief 计算单帧的时间线
     * @param frame_id 帧ID
     * @return 该帧的完整时间线
     */
    FrameTimeline compute_frame_timeline(unsigned frame_id) {
        FrameTimeline timeline;
        timeline.frame_id = frame_id;
        timeline.stage_windows.resize(stages.size());
        
        // 计算帧开始时间（考虑帧间间隔）
        unsigned frame_start_time = frame_id * initiation_interval;
        unsigned current_time = frame_start_time;
        
        // 逐stage计算时间窗口
        for (size_t stage = 0; stage < stages.size(); stage++) {
            unsigned stage_start = current_time;
            unsigned stage_end = current_time + stages[stage].latency;
            
            timeline.stage_windows[stage] = {stage_start, stage_end};
            current_time = stage_end;
        }
        
        timeline.total_start_time = frame_start_time;
        timeline.total_completion_time = current_time;
        
        return timeline;
    }
    
    /**
     * @brief 计算性能指标
     * @param analysis 分析结果引用
     */
    void compute_performance_metrics(PipelineAnalysis& analysis) {
        if (analysis.frame_timelines.empty()) return;
        
        // 找到最后完成的帧时间
        unsigned max_completion_time = 0;
        for (const auto& timeline : analysis.frame_timelines) {
            max_completion_time = max(max_completion_time, timeline.total_completion_time);
        }
        
        analysis.total_pipeline_cycles = max_completion_time;
        
        // 计算吞吐率（帧/周期）
        if (max_completion_time > 0) {
            analysis.pipeline_throughput = float(num_frames) / float(max_completion_time);
        }
        
        // 计算串行模式总周期（每帧=3*FFT+2*SHUFFLE+余量周期）
        const unsigned SERIAL_FRAME_CYCLES = 3 * FFT_OPERATION_CYCLES + 2 * SHUFFLE_OPERATION_CYCLES + 10; // 添加10周期余量
        analysis.serial_total_cycles = num_frames * SERIAL_FRAME_CYCLES;
        
        // 计算加速比
        if (analysis.total_pipeline_cycles > 0) {
            analysis.speedup_ratio = float(analysis.serial_total_cycles) / float(analysis.total_pipeline_cycles);
        }
    }

    /**
     * @brief 打印帧时间线表格
     */
    void print_frame_timeline_table(const vector<FrameTimeline>& timelines) {
        cout << "  Frame | Start | End  | Duration | Stage Windows\n";
        cout << "  ------|-------|------|----------|--------------------------------------------------\n";
        
        for (const auto& timeline : timelines) {
            cout << "  " << setw(5) << timeline.frame_id 
                 << " | " << setw(5) << timeline.total_start_time
                 << " | " << setw(4) << timeline.total_completion_time
                 << " | " << setw(8) << (timeline.total_completion_time - timeline.total_start_time) << " |";
            
            for (size_t s = 0; s < timeline.stage_windows.size(); s++) {
                cout << " [" << timeline.stage_windows[s].first 
                     << "-" << timeline.stage_windows[s].second << "]";
            }
            cout << "\n";
        }
    }
    
    /**
     * @brief 打印ASCII艺术风格的流水线可视化
     */
    void print_pipeline_visualization(const vector<FrameTimeline>& timelines) {
        if (timelines.empty()) return;
        
        // 找到最大时间用于确定图表宽度
        unsigned max_time = 0;
        for (const auto& timeline : timelines) {
            max_time = max(max_time, timeline.total_completion_time);
        }
        
        // 创建可视化网格
        unsigned time_scale = max(1u, max_time / 80);  // 将时间轴缩放到80字符内
        
        cout << "  Time:  ";
        for (unsigned t = 0; t <= max_time; t += 10 * time_scale) {
            cout << setw(10) << t;
        }
        cout << "\n";
        
        cout << "  Scale: ";
        for (unsigned t = 0; t <= max_time; t += time_scale) {
            cout << (t % (10 * time_scale) == 0) ? "|" : ".";
        }
        cout << "\n\n";
        
        // 为每个stage绘制时间线
        for (size_t stage_idx = 0; stage_idx < stages.size(); stage_idx++) {
            cout << "  " << setw(10) << stages[stage_idx].name << ": ";
            
            vector<char> timeline_viz(max_time / time_scale + 1, ' ');
            
            // 标记每帧在当前stage的执行时间段
            for (const auto& frame_timeline : timelines) {
                unsigned start = frame_timeline.stage_windows[stage_idx].first / time_scale;
                unsigned end = frame_timeline.stage_windows[stage_idx].second / time_scale;
                
                for (unsigned t = start; t < end && t < timeline_viz.size(); t++) {
                    timeline_viz[t] = '0' + (frame_timeline.frame_id % 10);  // 用数字表示帧ID
                }
            }
            
            for (char c : timeline_viz) {
                cout << c;
            }
            cout << "\n";
        }
        
        cout << "\n  Legend: 0,1,2,3... = Frame IDs executing in each stage\n";
        cout << "          Each character represents " << time_scale << " clock cycle(s)\n";
    }
};

#endif // FFT_PIPELINE_SIM_H