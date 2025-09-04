#ifndef TOOLS_H
#define TOOLS_H

#include "../src/vcore/FFT_SA/utils/complex_types.h"
#include "const.h"


//混洗输入数据
template <typename T>
void shuffle_data(const vector<complex<T>>& data_complex_read, vector<complex<T>>& data_A, vector<complex<T>>& data_B, 
                vector<int>&index_A, vector<int>&index_B, uint32_t point_num, uint32_t level) {
    
    data_A.resize(point_num / 2);
    data_B.resize(point_num / 2);
    index_A.resize(point_num / 2);
    index_B.resize(point_num / 2);

    uint32_t group_size = point_num / (1 << (level-1)); // 每组的大小
    uint32_t half_group_size = group_size / 2; // 每组中A和B各自的大小

    for (uint32_t g = 0; g < (1 << (level-1)); g++) {
        for (uint32_t i = 0; i < half_group_size; i++) {
            // 计算当前组的起始索引
            uint32_t base_index = g * group_size;
            uint32_t a_idx = g * half_group_size + i;
            uint32_t b_idx = g * half_group_size + i;
            
            // A取每组的前半部分
            data_A[a_idx] = data_complex_read[base_index + i];
            index_A[a_idx] = base_index + i; // 记录原始索引
            
            // B取每组的后半部分
            data_B[b_idx] = data_complex_read[base_index + i + half_group_size];
            index_B[b_idx] = base_index + i + half_group_size; // 记录原始索引
        }
    }
}

// 工具函数：计算FFT旋转因子
// 参数：point_num - FFT点数（必须是2的幂次）
// 返回：包含所有级别旋转因子的向量
template <typename T>
vector<complex<T>> calculate_twiddle_factors(uint32_t point_num) {
    // 检查point_num是否为2的幂次
    if ((point_num & (point_num - 1)) != 0) {
        cout << "错误：FFT点数必须是2的幂次,point_num = " << dec << point_num << endl;
        return vector<complex<T>>();
    }
    
    // 计算蝶形级数
    uint32_t butterfly_level_num = static_cast<uint32_t>(log2(point_num));
    
    // 初始化旋转因子数组，总大小为butterfly_level_num * point_num / 2
    vector<complex<T>> W_N(butterfly_level_num * point_num / 2);
    
    // 第0级旋转因子 W^0, W^1, W^2, W^3, ... (基本旋转因子)
    for (uint32_t i = 0; i < point_num / 2; i++) {
        double angle = 2 * M_PI * i / point_num;
        W_N[i].real = cos(angle);
        W_N[i].imag = -sin(angle);
    }
    
        // 第1级到butterfly_level_num-1级的旋转因子
    for (uint32_t level = 1; level < butterfly_level_num; level++) {
        uint32_t level_offset = level * point_num / 2;
        uint32_t group_count = 1 << level; // 2^level
        uint32_t elements_per_group = point_num / (2 * group_count);
        
        for (uint32_t g = 0; g < group_count; g++) {
            // 计算当前组的旋转因子步长
            uint32_t stride = 1 << level; // 2^(level)
            
            // 填充当前组的旋转因子
            for (uint32_t i = 0; i < elements_per_group; i++) {
                uint32_t idx = g * elements_per_group + i;
                uint32_t power = (g * elements_per_group + i) * stride;


                W_N[level_offset + idx] = W_N[power % (point_num / 2)];
            }
        }
    }
    return W_N;
}

// 平均池化函数: 对三维输入特征图执行平均池化操作
// 参数:
//   output_data - 输出数据数组 (一维表示的三维数据)
//   input_data - 输入数据数组 (一维表示的三维数据)
//   channel_num - 通道数
//   kernel_size - 池化核大小 (正方形核, 如2表示2x2)
//   stride - 步长
template <typename T>
void AvgPool_function(std::vector<T>& output_data, const std::vector<T>& input_data, 
                     uint32_t channel_num, uint32_t kernel_size, uint32_t stride) {
    // 从输入数据大小计算输入高度和宽度
    uint32_t input_size = input_data.size() / channel_num;
    uint32_t input_height = static_cast<uint32_t>(sqrt(input_size));
    uint32_t input_width = input_height; // 假设输入是正方形
    
    // 计算输出尺寸
    uint32_t output_height = (input_height - kernel_size) / stride + 1;
    uint32_t output_width = (input_width - kernel_size) / stride + 1;
    
    // 调整输出向量大小
    output_data.resize(channel_num * output_height * output_width);
    
    // 执行平均池化操作
    for (uint32_t c = 0; c < channel_num; ++c) {
        for (uint32_t oh = 0; oh < output_height; ++oh) {
            for (uint32_t ow = 0; ow < output_width; ++ow) {
                // 计算输入特征图中对应的左上角位置
                uint32_t in_h_start = oh * stride;
                uint32_t in_w_start = ow * stride;
                
                // 计算池化窗口内的平均值
                T sum = 0;
                for (uint32_t kh = 0; kh < kernel_size; ++kh) {
                    for (uint32_t kw = 0; kw < kernel_size; ++kw) {
                        uint32_t in_h = in_h_start + kh;
                        uint32_t in_w = in_w_start + kw;
                        
                        // 计算输入索引
                        uint32_t input_idx = c * input_height * input_width + 
                                            in_h * input_width + in_w;
                        
                        sum += input_data[input_idx];
                    }
                }
                
                // 计算平均值
                T avg = sum / (kernel_size * kernel_size);
                
                // 计算输出索引并存储结果
                uint32_t output_idx = c * output_height * output_width + 
                                     oh * output_width + ow;
                
                output_data[output_idx] = avg;
            }
        }
    }
}

template <typename T>
vector<complex<T>> calculate_twiddle_factors_ifft(uint32_t point_num) {
    // 检查point_num是否为2的幂次
    if ((point_num & (point_num - 1)) != 0) {
        cout << "错误：FFT点数必须是2的幂次,point_num = " << dec << point_num << endl;
        return vector<complex<T>>();
    }
    
    // 计算蝶形级数
    uint32_t butterfly_level_num = static_cast<uint32_t>(log2(point_num));
    
    // 初始化旋转因子数组，总大小为butterfly_level_num * point_num / 2
    vector<complex<T>> W_N(butterfly_level_num * point_num / 2);
    
    // 第0级旋转因子 W^0, W^1, W^2, W^3, ... (基本旋转因子)
    for (uint32_t i = 0; i < point_num / 2; i++) {
        double angle = - 2 * M_PI * i / point_num; //iFFT的旋转因子角度正负号相反的
        W_N[i].real = cos(angle);
        W_N[i].imag = -sin(angle);
    }
    
        // 第1级到butterfly_level_num-1级的旋转因子
    for (uint32_t level = 1; level < butterfly_level_num; level++) {
        uint32_t level_offset = level * point_num / 2;
        uint32_t group_count = 1 << level; // 2^level
        uint32_t elements_per_group = point_num / (2 * group_count);
        
        for (uint32_t g = 0; g < group_count; g++) {
            // 计算当前组的旋转因子步长
            uint32_t stride = 1 << level; // 2^(level)
            
            // 填充当前组的旋转因子
            for (uint32_t i = 0; i < elements_per_group; i++) {
                uint32_t idx = g * elements_per_group + i;
                uint32_t power = (g * elements_per_group + i) * stride;


                W_N[level_offset + idx] = W_N[power % (point_num / 2)];
            }
        }
    }
    return W_N;
}

template <typename T>
vector<complex<T>> calculate_rotation_factors_compensate(uint32_t N1, uint32_t N2) {
    vector<complex<T>> rotation_factors(N1 * N2);
    T N = static_cast<T>(N1 * N2);
    for (uint32_t k1 = 0; k1 < N1; ++k1) {
        for (uint32_t k2 = 0; k2 < N2; ++k2) {
            T angle = -2.0 * M_PI * k1 * k2 / N;
            complex<T> w;
            w.real = cos(angle);
            w.imag = sin(angle);
            rotation_factors[k1 * N2 + k2] = w;
        }
    }
    return rotation_factors;
}

// 调试函数：打印旋转因子
template <typename T>
void print_twiddle_factors(const vector<complex<T>>& W_N, uint32_t point_num) {
    uint32_t butterfly_level_num = static_cast<uint32_t>(log2(point_num));
    
    cout << "旋转因子计算完成，总计 " << W_N.size() << " 个旋转因子" << endl;
    for (uint32_t level = 0; level < butterfly_level_num; level++) {
        cout << "第" << level << "级旋转因子: ";
        uint32_t start_idx = level * point_num / 2;
        uint32_t end_idx = (level + 1) * point_num / 2;
        for (uint32_t i = start_idx; i < end_idx; i++) {
            cout << "(" << W_N[i].real << "," << W_N[i].imag << ") ";
        }
        cout << endl;
    }
}


//计算位反序后的索引列表,比如8点的位反序索引列表为0,4,2,6,1,5,3,7
inline void calculate_reverse_index(vector<uint32_t>& output_index_list, uint32_t length) {
    output_index_list.resize(length);
    uint32_t num_bits = static_cast<uint32_t>(log2(length)); // 计算需要的位数
    for (uint32_t i = 0; i < length; i++) {
        output_index_list[i] = 0;
        for (uint32_t j = 0; j < num_bits; j++) {
            output_index_list[i] = (output_index_list[i] << 1) | ((i >> j) & 1);
        }
    }
}

inline void wait_for_OK_response(tlm::tlm_generic_payload& trans){
    while(trans.get_response_status() != tlm::TLM_OK_RESPONSE){
        wait(SYSTEM_CLOCK);
    }
}

//计算传输数据所需时钟周期
inline uint64_t calculate_clock_cycles(uint64_t data_size, uint64_t data_width) {
    return (data_size + data_width - 1) / data_width;
}
//三个向量合并为一个向量
template <typename T>
void merge_vectors(const vector<T>& vec1, const vector<T>& vec2, const vector<T>& vec3, vector<T>& merged_vec) {
    //vec1,vec2,vec3的元素个数必须相等
    if(vec1.size() != vec2.size() || vec1.size() != vec3.size()){
        SC_REPORT_ERROR("merge_vectors", "三个向量的元素个数不相等");
        return;
    }
    merged_vec.resize(vec1.size() + vec2.size() + vec3.size());
    for(int i = 0; i < vec1.size(); i++) {
        memcpy(&merged_vec[i*3], &vec1[i], sizeof(T));
        memcpy(&merged_vec[i*3 + 1], &vec2[i], sizeof(T));
        memcpy(&merged_vec[i*3 + 2], &vec3[i], sizeof(T));
    }
}
//一个向量拆分为三个向量
template <typename T>
void split_vector(const vector<T>& merged_vec, vector<T>& vec1, vector<T>& vec2, vector<T>& vec3) {
    vec1.resize(merged_vec.size() / 3);
    vec2.resize(merged_vec.size() / 3);
    vec3.resize(merged_vec.size() / 3);
    for(int i = 0; i < merged_vec.size() / 3; i++) {
        memcpy(&vec1[i], &merged_vec[i*3], sizeof(T));
        memcpy(&vec2[i], &merged_vec[i*3 + 1], sizeof(T));
        memcpy(&vec3[i], &merged_vec[i*3 + 2], sizeof(T));
    }
}


// 加载文件数据到 vector
template <typename T>
void load_from_file(std::vector<T>& data_buffer, std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SC_REPORT_ERROR("load_from_file", "Failed to open file.");
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        T value;
        while (stream >> value) {
            data_buffer.push_back(value);
        }
    }
    file.close();
}
//加载文件复数数据到vector<complex<T>>
template <typename T>
void load_complex_data_from_file_3d(std::vector<complex<T>>& data_buffer, const std::string& file_path, uint32_t& channel_num, uint32_t& row_num, uint32_t& col_num) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SC_REPORT_ERROR("load_complex_data_from_file_3d", "Failed to open file.");
        return;
    }

    // Read dimensions from the first line
    std::string line;
    std::getline(file, line);
    if (line.find("Dimensions:") != std::string::npos) {
        // Parse dimensions from format: "Dimensions: {channels} channels x {rows} rows x {cols} cols"
        std::istringstream iss(line);
        std::string dummy;
        
        // Skip "Dimensions:"
        iss >> dummy;
        
        // Read channel number
        iss >> channel_num >> dummy;
        
        // Skip "x"
        iss >> dummy;
        
        // Read row number
        iss >> row_num >> dummy;
        
        // Skip "x"
        iss >> dummy;
        
        // Read column number
        iss >> col_num >> dummy;
        
        if (channel_num <= 0 || row_num <= 0 || col_num <= 0) {
            SC_REPORT_ERROR("load_complex_data_from_file_3d", "Invalid dimension values in file.");
            return;
        }
    } else {
        SC_REPORT_ERROR("load_complex_data_from_file_3d", "Missing dimensions in file header.");
        return;
    }

    // Resize the buffer to hold all data points
    data_buffer.resize(channel_num * row_num * col_num);
    
    // Track current channel, row, and position in buffer
    uint32_t current_channel = 0;
    uint32_t current_row = 0;
    uint32_t buffer_index = 0;
    
    // Process the rest of the file
    while (std::getline(file, line)) {
        // Check for channel separator
        if (line == "---") {
            current_channel++;
            current_row = 0;
            continue;
        }
        
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        
        // Process data line
        std::istringstream iss(line);
        T real, imag;
        
        // Read each complex number pair (real, imag) from the line
        for (uint32_t col = 0; col < col_num; col++) {
            if (!(iss >> real >> imag)) {
                SC_REPORT_ERROR("load_complex_data_from_file_3d", "Failed to parse complex values.");
                return;
            }
            
            // Calculate buffer index using channels as outermost dimension
            buffer_index = (current_channel * row_num * col_num) + (current_row * col_num) + col;
            data_buffer[buffer_index] = complex<T>(real, imag);
        }
        
        current_row++;
    }
    
    file.close();
}
//加载文件复数数据到vector<complex<T>>
template <typename T>
void load_complex_data_from_file(std::vector<complex<T>>& data_buffer, const std::string& file_path, uint32_t& row_num, uint32_t& col_num) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SC_REPORT_ERROR("load_complex_data_from_file", "Failed to open file.");
        return;
    }
    
    std::string line;
    T real_part, imag_part;
    std::string j_suffix;
    row_num = 0;
    col_num = 0;
    
    // 清空输入缓冲区，准备加载新数据
    data_buffer.clear();
    
    // 逐行读取文件内容
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        uint32_t col_count = 0;
        
        // 从每行中读取多个复数对
        while (iss >> real_part >> imag_part) {
            // 处理可能存在的j后缀
            std::string next;
            if (iss >> next && next.find('j') != std::string::npos) {
                // j后缀已处理，继续读取下一对数据
            } else {
                // 如果没有j后缀，回退读取位置
                iss.unget();
            }
            
            // 创建复数并添加到缓冲区
            complex<T> complex_value;
            complex_value.real = real_part;
            complex_value.imag = imag_part;
            data_buffer.push_back(complex_value);
            col_count++;
        }
        
        // 记录列数（仅在第一行时）
        if (row_num == 0) {
            col_num = col_count;
        } else if (col_count != col_num) {
            // 检查每行的列数是否一致
            SC_REPORT_WARNING("load_complex_data_from_file", "Inconsistent number of columns in data file.");
        }
        
        row_num++;
    }
    
    file.close();
}
//加载文件3D实数数据到vector<T>
template <typename T>
void load_real_data_from_file_3d(std::vector<T>& data_buffer, const std::string& file_path, uint32_t& channel_num, uint32_t& row_num, uint32_t& col_num) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SC_REPORT_ERROR("load_real_data_from_file_3d", "Failed to open file.");
        return;
    }
    cout << "=================load_real_data_from_file_3d 开始=================" << endl;
    
    std::string line;
    // 读取第一行，获取维度信息
    std::getline(file, line);
    // 解析维度信息，格式为 "Dimensions: channels channels x rows rows x cols cols"
    // 如果格式不匹配，使用传入的维度参数
    if (line.find("Dimensions:") != std::string::npos) {
        std::istringstream iss(line);
        std::string dim_label, channels_label, rows_label, cols_label, x1, x2;
        uint32_t ch, r, c;
        iss >> dim_label >> ch >> channels_label >> x1 >> r >> rows_label >> x2 >> c >> cols_label;
        
        // 只有当成功解析到三个维度时才更新参数
        if (ch > 0 && r > 0 && c > 0) {
            channel_num = ch;
            row_num = r;
            col_num = c;
        }
    }
    
    // 调整buffer大小以适应所有数据
    data_buffer.resize(channel_num * row_num * col_num);
    
    // 逐通道读取数据
    uint32_t data_index = 0;
    uint32_t channel = 0;
    
    while (channel < channel_num && std::getline(file, line)) {
        // 每个通道的数据读取
        for (uint32_t row = 0; row < row_num && !line.empty(); ++row) {
            if (line.find("---") != std::string::npos) {
                // 发现通道分隔符，跳过
                ++channel;
                break;
            }
            
            std::istringstream iss(line);
            for (uint32_t col = 0; col < col_num; ++col) {
                T value;
                if (iss >> value) {
                    data_buffer[data_index++] = value;
                } else {
                    SC_REPORT_WARNING("load_real_data_from_file_3d", "Failed to parse value at position.");
                }
            }
            
            // 读取下一行
            if (row < row_num - 1 || channel < channel_num - 1) {
                if (!std::getline(file, line)) {
                    break;
                }
            }
        }
    }
    
    // 检查是否读取了足够的数据
    if (data_index < channel_num * row_num * col_num) {
        SC_REPORT_WARNING("load_real_data_from_file_3d", "Not enough data in file.");
    }
    
    cout << "成功加载 " << dec << data_index << " 个数据元素，通道数: " << channel_num 
         << ", 行数: " << row_num << ", 列数: " << col_num << endl;
    cout << "=================load_real_data_from_file_3d 完成=================" << endl;
}

template <typename T>
void load_real_data_from_file_4d(std::vector<T>& data_buffer, const std::string& file_path, uint32_t channel_output_num, uint32_t channel_input_num, uint32_t row_num, uint32_t col_num) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SC_REPORT_ERROR("load_real_data_from_file_4d", "Failed to open file.");
        return;
    }

    // 读取第一行的维度信息
    std::string line;
    std::getline(file, line);
    if (line.find("Dimensions:") != std::string::npos) {
        // 解析维度信息：格式为 "Dimensions: {output_channels} output_channels x {input_channels} input_channels x {rows} rows x {cols} cols"
        std::istringstream iss(line);
        std::string dummy;
        
        // 跳过 "Dimensions:"
        iss >> dummy;
        
        // 读取输出通道数
        iss >> channel_output_num >> dummy;
        if(channel_output_num == 0){
            cout << "load_real_data_from_file_4d:channel out = 0" << endl;
            return;
        }
        
        // 跳过 "x"
        iss >> dummy;
        
        // 读取输入通道数
        iss >> channel_input_num >> dummy;
        if(channel_input_num == 0){
            cout << "load_real_data_from_file_4d:channel in = 0" << endl;
            return;
        }
        
        // 跳过 "x"
        iss >> dummy;
        
        // 读取行数
        iss >> row_num >> dummy;
        
        // 跳过 "x"
        iss >> dummy;
        
        // 读取列数
        iss >> col_num >> dummy;
        
        if (channel_output_num <= 0 || channel_input_num <= 0 || row_num <= 0 || col_num <= 0) {
            SC_REPORT_ERROR("load_real_data_from_file_4d", "Invalid dimension values in file.");
            return;
        }
    } else {
        SC_REPORT_ERROR("load_real_data_from_file_4d", "Missing dimensions in file header.");
        return;
    }

    // 计算数据总数
    uint32_t total_elements = channel_output_num * channel_input_num * row_num * col_num;
    data_buffer.resize(total_elements);
    
    // 用于跟踪当前位置的变量
    uint32_t output_channel = 0;
    uint32_t input_channel = 0;
    uint32_t current_row = 0;
    
    // 循环读取数据
    while (std::getline(file, line)) {
        // 检查通道分隔符
        if (line == "---output_channel---") {
            output_channel++;
            input_channel = 0;
            current_row = 0;
            continue;
        } else if (line == "---input_channel---") {
            input_channel++;
            current_row = 0;
            continue;
        }
        
        // 跳过空行
        if (line.empty()) {
            continue;
        }
        
        // 解析一行的数据
        std::istringstream iss(line);
        T value;
        uint32_t col = 0;
        
        while (iss >> value && col < col_num) {
            // 计算一维数组中的索引：(输出通道 * 输入通道总数 * 行总数 * 列总数) + 
            //                      (输入通道 * 行总数 * 列总数) + 
            //                      (当前行 * 列总数) + 当前列
            uint32_t index = (output_channel * channel_input_num * row_num * col_num) + 
                            (input_channel * row_num * col_num) + 
                            (current_row * col_num) + col;
            
            if (index < total_elements) {
                data_buffer[index] = value;
            } else {
                SC_REPORT_ERROR("load_real_data_from_file_4d", "Index out of bounds.");
                return;
            }
            
            col++;
        }
        
        // 检查是否读取了足够的列
        if (col != col_num) {
            SC_REPORT_WARNING("load_real_data_from_file_4d", "Line does not contain expected number of columns.");
        }
        
        current_row++;
        
        // 检查是否读取了太多行
        if (current_row > row_num) {
            SC_REPORT_WARNING("load_real_data_from_file_4d", "Too many rows in input channel.");
            current_row = 0;
            input_channel++;
        }
    }
    
    // 验证是否读取了预期数量的数据
    if (output_channel != channel_output_num - 1 || 
        input_channel != channel_input_num - 1 || 
        current_row != row_num) {
        SC_REPORT_WARNING("load_real_data_from_file_4d", "Data dimensions do not match expected dimensions.");
    }
    
    file.close();
}
// 将vector<complex<T>>数据写入文件
template <typename T>
void write_complex_data_to_file(std::vector<complex<T>>& data_buffer, const std::string& file_path) {
    ofstream outfile(file_path);
    if (!outfile.is_open()) {
        SC_REPORT_ERROR("write_complex_data_to_file", "Failed to open file.");
        return;
    }
    
    // 逐个写入复数数据，每行一个复数（实部 虚部）
    for (size_t i = 0; i < data_buffer.size(); ++i) {
        outfile << data_buffer[i].real << " " << data_buffer[i].imag << "\n";
    }
    
    outfile.close();
}

template <typename T>
void write_complex_data_to_file_2d(std::vector<complex<T>>& data_buffer, const std::string& file_path, uint32_t row_num, uint32_t col_num) {
    ofstream outfile(file_path);
    if (!outfile.is_open()) {
        SC_REPORT_ERROR("write_complex_data_to_file_with_j", "Failed to open file.");
        return;
    }
    
    // 检查数据大小是否匹配行列数
    if (data_buffer.size() != row_num * col_num) {
        SC_REPORT_ERROR("write_complex_data_to_file_2d", "Data size does not match row_num * col_num");
        return;
    }
    
    // 按行列格式写入数据
    for (uint32_t i = 0; i < row_num; ++i) {
        for (uint32_t j = 0; j < col_num; ++j) {
            // 获取当前位置的复数
            complex<T> value = data_buffer[i * col_num + j];
            
            // 写入实部和虚部，以空格分隔
            outfile << value.real << " " << value.imag;
            
            // 如果不是行末元素，添加更多空格分隔
            if (j < col_num - 1) {
                outfile << "  ";
            }
        }
        // 每行结束后换行
        outfile << "\n";
    }
    
    outfile.close();
}
template <typename T>
void write_complex_data_to_file_3d(std::vector<complex<T>>& data_buffer, const std::string& file_path, uint32_t channel_num, uint32_t row_num, uint32_t col_num) {
    ofstream outfile(file_path);
    if (!outfile.is_open()) {
        SC_REPORT_ERROR("write_complex_data_to_file_3d", "Failed to open file.");
        return;
    }
    
    // 检查数据大小是否匹配行列数
    if (data_buffer.size() != channel_num * row_num * col_num) {
        SC_REPORT_ERROR("write_complex_data_to_file_3d", "Data size does not match channel_num * row_num * col_num");
        return;
    }
    
    // 写入维度信息到第一行
    outfile << "Dimensions: " << channel_num << " channels x " << row_num << " rows x " << col_num << " cols" << std::endl;
    
    // 按通道、行、列格式写入数据
    for (uint32_t c = 0; c < channel_num; ++c) {
        for (uint32_t i = 0; i < row_num; ++i) {
            for (uint32_t j = 0; j < col_num; ++j) {
                // 计算当前元素在一维数组中的索引
                uint32_t index = (c * row_num * col_num) + (i * col_num) + j;
                
                // 获取当前位置的复数
                complex<T> value = data_buffer[index];
                
                // 写入实部和虚部，以空格分隔
                outfile << value.real << " " << value.imag << " ";
            }
            // 每行结束后换行
            outfile << "\n";
        }
        // 每个通道之间添加分隔符
        if (c < channel_num - 1) {
            outfile << "---\n";
        }
    }
    
    outfile.close();
}

template <typename T>
void write_real_data_to_file_3d(std::vector<T>& data_buffer, const std::string& file_path, int channel_num, int row_num, int col_num) {
    ofstream outfile(file_path);
    if (!outfile.is_open()) {
        SC_REPORT_ERROR("write_real_data_to_file_3d", "Failed to open file.");
        return;
    }
    
    // 检查数据大小是否匹配维度
    if (data_buffer.size() != channel_num * row_num * col_num) {
        SC_REPORT_ERROR("write_real_data_to_file_3d", "Data size does not match channel_num * row_num * col_num");
        return;
    }
    
    // 写入维度信息到第一行
    outfile << "Dimensions: " << channel_num << " channels x " << row_num << " rows x " << col_num << " cols" << std::endl;
    
    // 按通道、行、列格式写入数据
    for (int c = 0; c < channel_num; ++c) {
        for (int i = 0; i < row_num; ++i) {
            for (int j = 0; j < col_num; ++j) {
                // 计算当前元素在一维数组中的索引
                int index = (c * row_num * col_num) + (i * col_num) + j;
                
                // 获取当前位置的实数值
                T value = data_buffer[index];
                
                // 写入实数值，添加空格分隔
                outfile << value << " ";
            }
            // 每行结束后换行
            outfile << "\n";
        }
        // 每个通道之间添加分隔符
        if (c < channel_num - 1) {
            outfile << "---\n";
        }
    }
    
    outfile.close();
}

// 记录矩阵形状
template <typename T>
void record_matrix_shape(const std::string& file_path, int& rows, int& cols) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        SC_REPORT_ERROR("record_matrix_shape", "Failed to open file.");
        return;
    }
    if(rows == 0 && cols == 0){
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        int current_cols = 0;
        T value;
        while (stream >> value) {
            ++current_cols;
        }
        if (cols == 0) {
            cols = current_cols;
        } else if (cols != current_cols) {
            SC_REPORT_ERROR("record_matrix_shape", "Inconsistent column sizes.");
            return;
        }
        ++rows;
    }
    file.close();
    }
    else{
        SC_REPORT_INFO("record_matrix_shape", "矩阵形状提前已设定");
        return;
    }
}

template <typename T>
void convert1DTo3D(const vector<T>& input, vector<vector<vector<T>>>& output, int channel_num, int row_num, int col_num) {
    // Check if input size matches expected total size
    int total_elements = channel_num * row_num * col_num;
    if (input.size() != total_elements) {
        SC_REPORT_ERROR("convert1DTo3D", "Input vector size does not match the product of dimensions");
        return;
    }
    
    // Resize output to proper dimensions
    output.resize(channel_num);
    for (int c = 0; c < channel_num; c++) {
        output[c].resize(row_num);
        for (int r = 0; r < row_num; r++) {
            output[c][r].resize(col_num);
        }
    }
    
    // Fill the 3D vector from the 1D input
    for (int c = 0; c < channel_num; c++) {
        for (int r = 0; r < row_num; r++) {
            for (int col = 0; col < col_num; col++) {
                // Calculate 1D index from 3D coordinates
                int idx = c * (row_num * col_num) + r * col_num + col;
                output[c][r][col] = input[idx];
            }
        }
    }
}

template <typename T>
void convert3DTo1D(const vector<vector<vector<T>>>& input, vector<T>& output) {
    // 获取三维向量的尺寸
    int channel_num = input.size();
    if (channel_num == 0) {
        output.clear();
        return;
    }
    
    int row_num = input[0].size();
    if (row_num == 0) {
        output.clear();
        return;
    }
    
    int col_num = input[0][0].size();
    
    // 调整输出向量的大小
    output.resize(channel_num * row_num * col_num);
    
    // 将三维数据转换为一维数据
    for (int c = 0; c < channel_num; c++) {
        for (int r = 0; r < row_num; r++) {
            for (int col = 0; col < col_num; col++) {
                // 计算一维索引
                int idx = c * (row_num * col_num) + r * col_num + col;
                output[idx] = input[c][r][col];
            }
        }
    }
}
template <typename T>
void convert1DTo4D(const vector<T>& input, vector<vector<vector<vector<T>>>>& output, int output_channel_num, int input_channel_num, int kernel_height, int kernel_width) {
    // Check if input size matches expected total size
    int total_elements = output_channel_num * input_channel_num * kernel_height * kernel_width;
    if (input.size() != total_elements) {
        SC_REPORT_ERROR("convert1DTo4D", "Input vector size does not match the product of dimensions");
        return;
    }
    
    // Resize output to proper dimensions
    output.resize(output_channel_num);
    for (int oc = 0; oc < output_channel_num; oc++) {
        output[oc].resize(input_channel_num);
        for (int ic = 0; ic < input_channel_num; ic++) {
            output[oc][ic].resize(kernel_height);
            for (int kh = 0; kh < kernel_height; kh++) {
                output[oc][ic][kh].resize(kernel_width);
            }
        }
    }
    
    // Fill the 4D vector from the 1D input
    for (int oc = 0; oc < output_channel_num; oc++) {
        for (int ic = 0; ic < input_channel_num; ic++) {
            for (int kh = 0; kh < kernel_height; kh++) {
                for (int kw = 0; kw < kernel_width; kw++) {
                    // Calculate 1D index from 4D coordinates
                    // Format: [output_channel][input_channel][kernel_height][kernel_width]
                    int idx = ((oc * input_channel_num + ic) * kernel_height + kh) * kernel_width + kw;
                    output[oc][ic][kh][kw] = input[idx];
                }
            }
        }
    }
}

template <typename T>
void convertTo2D(const vector<T>& input, vector<vector<T>>& output, int rows, int cols) {
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++) {
            output[i][j] = input[i * cols + j];
        }
    }
}
template <typename T>
void convert2DTo1D(const vector<vector<T>>& input, vector<T>& output) {
    int rows = input.size();
    int cols = input[0].size();
    output.resize(rows * cols);
    
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++) {
            output[i * cols + j] = input[i][j];
        }
    }
}
// 矩阵乘法并保存结果到文件的函数
template <typename T>
void multiplyAndSaveMatrices(const vector<vector<T>>& mat1, 
                             const vector<vector<T>>& mat2, 
                             const string& filename) {
    if (mat1[0].size() != mat2.size()) {
        throw invalid_argument("Matrix dimensions do not match for multiplication.");
    }

    size_t rows = mat1.size();
    size_t cols = mat2[0].size();
    size_t inner = mat2.size();

    vector<vector<double>> result(rows, vector<double>(cols, 0.0));

    // 矩阵乘法
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            for (size_t k = 0; k < inner; ++k) {
                result[i][j] += mat1[i][k] * mat2[k][j];
            }
        }
    }

    // 保存结果到文件
    ofstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Failed to open file.");
    }

    for (const auto& row : result) {
        for (const auto& elem : row) {
            file << elem << " ";
        }
        file << "\n";
    }

    file.close();
}

template <typename T>
void check_all_zero(const vector<T> buffer){
    for(int i = 0; i < buffer.size(); i++){
        if(buffer[i] != 0){
            //cout << "buffer is not all zero"<<endl;
            return;
        }
    }
    cout << "buffer is all zero!!!!!!!!!!!!!!!!!!!!"<<endl;
    return;
}

// 重排序函数：将3D复数矩阵重排为3D实数矩阵，实部和虚部分开保存
// 输入：复数三维矩阵，维度(channel_num, row_num, col_num/2+1)
// 输出：实数三维矩阵，维度(channel_num*2, row_num, col_num/2+1)
template <typename T>
void rearrange_complex_to_real_3d(const std::vector<complex<T>>& complex_data, 
                                 std::vector<T>& real_data,
                                 uint32_t channel_num, 
                                 uint32_t row_num, 
                                 uint32_t col_half_plus_one) {
    // 计算元素总数
    uint32_t complex_size = channel_num * row_num * col_half_plus_one;
    uint32_t real_size = channel_num * 2 * row_num * col_half_plus_one;
    
    // 检查输入数据大小
    if (complex_data.size() != complex_size) {
        SC_REPORT_ERROR("rearrange_complex_to_real_3d", "输入复数数据大小与指定维度不匹配");
        return;
    }
    
    // 调整输出向量大小
    real_data.resize(real_size);
    
    // 重排数据：实部和虚部分开保存
    for (uint32_t c = 0; c < channel_num; ++c) {
        // 实部保存在前半部分
        for (uint32_t i = 0; i < row_num; ++i) {
            for (uint32_t j = 0; j < col_half_plus_one; ++j) {
                uint32_t complex_idx = (c * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                uint32_t real_idx = (c * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                real_data[real_idx] = complex_data[complex_idx].real;
            }
        }
        
        // 虚部保存在后半部分
        for (uint32_t i = 0; i < row_num; ++i) {
            for (uint32_t j = 0; j < col_half_plus_one; ++j) {
                uint32_t complex_idx = (c * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                uint32_t imag_idx = ((c + channel_num) * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                real_data[imag_idx] = complex_data[complex_idx].imag;
            }
        }
    }
}

// 逆重排序函数：将3D实数矩阵重排为3D复数矩阵
// 输入：实数三维矩阵，维度(channel_num*2, row_num, col_num/2+1)
// 输出：复数三维矩阵，维度(channel_num, row_num, col_num/2+1)
template <typename T>
void rearrange_real_to_complex_3d(const std::vector<T>& real_data,
                                 std::vector<complex<T>>& complex_data,
                                 uint32_t channel_num, 
                                 uint32_t row_num, 
                                 uint32_t col_half_plus_one) {
    // 计算元素总数
    uint32_t complex_size = channel_num * row_num * col_half_plus_one;
    uint32_t real_size = channel_num * 2 * row_num * col_half_plus_one;
    
    // 检查输入数据大小
    if (real_data.size() != real_size) {
        SC_REPORT_ERROR("rearrange_real_to_complex_3d", "输入实数数据大小与指定维度不匹配");
        return;
    }
    
    // 调整输出向量大小
    complex_data.resize(complex_size);
    
    // 重排数据：合并实部和虚部
    for (uint32_t c = 0; c < channel_num; ++c) {
        for (uint32_t i = 0; i < row_num; ++i) {
            for (uint32_t j = 0; j < col_half_plus_one; ++j) {
                uint32_t complex_idx = (c * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                uint32_t real_idx = (c * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                uint32_t imag_idx = ((c + channel_num) * row_num * col_half_plus_one) + (i * col_half_plus_one) + j;
                
                complex_data[complex_idx].real = real_data[real_idx];
                complex_data[complex_idx].imag = real_data[imag_idx];
            }
        }
    }
}

#endif
