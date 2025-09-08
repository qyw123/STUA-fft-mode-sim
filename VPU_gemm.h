#ifndef Gemm_H
#define Gemm_H

#include "./util/const.h"
#include "./util/tools.h"
#include <chrono>
template <typename T>
class MatrixBlockTransfer {
private:
    string transfer_name;
    
    void read_data(uint64_t addr, vector<T>& values, const tlm::tlm_dmi& dmi, unsigned int data_num) {
        ins::read_from_dmi(addr, values, dmi, data_num, transfer_name);
    }
    
    void write_data(uint64_t start_addr, uint64_t& end_addr, const vector<T>& values, 
                   const tlm::tlm_dmi& dmi, unsigned int data_num) {
        ins::write_to_dmi(start_addr, end_addr, values, dmi, data_num, transfer_name);
    }

public:
    MatrixBlockTransfer(const string& name) 
        : transfer_name(name) {}

    void transfer(
        uint64_t start_addr, 
        uint64_t& end_addr,
        uint64_t& next_block_start_addr,
        uint64_t matrix_start_addr,
        uint64_t matrix_end_addr,
        uint64_t target_start_addr,
        uint64_t& target_end_addr,
        int m_rows, 
        int m_cols, 
        int block_rows, 
        int block_cols,
        int& real_block_rows, 
        int& real_block_cols,
        const tlm::tlm_dmi& source_dmi, 
        const tlm::tlm_dmi& target_dmi,
        bool traverse_by_row,
        bool& rowloop_complete
    ) {
        // 参数验证
        if (block_rows <= 0 || block_cols <= 0 || m_rows <= 0 || m_cols <= 0) {
            cout << transfer_name << " ERROR: Invalid dimensions detected!\n"
                << "block_rows=" << block_rows << ", block_cols=" << block_cols << "\n"
                << "m_rows=" << m_rows << ", m_cols=" << m_cols << endl;
            sc_stop();
            return;
        }

        // ... [其余验证代码保持不变，只是在错误输出时加上transfer_name] ...
        // 确保起始地址在有效范围内
        if (start_addr > matrix_end_addr || start_addr < matrix_start_addr) {
            cout<<"Error transfer_matrixblock: "<<transfer_name<<endl;
            cout << "ERROR: Invalid start address: 0x" << hex << start_addr << endl;
            cout<<"matrix_start_addr:"<<matrix_start_addr<<",matrix_end_addr:"<<matrix_end_addr<<endl;
            sc_stop();
            return;
        }

        // 计算当前位置
        uint64_t offset = start_addr - matrix_start_addr;
        int start_row = (offset / sizeof(T)) / m_cols;
        int start_col = (offset / sizeof(T)) % m_cols;

        // 计算实际块大小
        real_block_rows = std::min(block_rows, m_rows - start_row);
        real_block_cols = std::min(block_cols, m_cols - start_col);

        // 验证计算结果
        if (real_block_rows <= 0 || real_block_cols <= 0) {
            cout <<sc_time_stamp()<< ":ERROR: Invalid block size calculated: [" << real_block_rows 
                << "," << real_block_cols << "]" << endl;
            cout<<"start_row:"<<start_row<<",start_col:"<<start_col<<endl;
            cout<<hex<<"start_addr:"<<start_addr<<",matrix_start_addr:"<<matrix_start_addr<<",matrix_end_addr:"<<matrix_end_addr<<endl;
            sc_stop();
            return;
        }

        vector<T> block_buffer(real_block_cols);
        for(int i = 0; i < real_block_rows; i++){
            read_data(start_addr + i * m_cols * sizeof(T), block_buffer, source_dmi, real_block_cols);
            //check_all_zero(block_buffer);
            write_data(target_start_addr + i * real_block_cols * sizeof(T), target_end_addr, block_buffer, target_dmi, real_block_cols);
        }
        end_addr = start_addr + (((real_block_rows-1) * m_cols+real_block_cols)) * sizeof(T) - 1;
        // 计算下一个块的起始地址
        if (traverse_by_row) {
            //先行后列循环
            next_block_start_addr = start_addr + (real_block_cols * sizeof(T));
            rowloop_complete = false;
            if ((start_col + real_block_cols) == m_cols) {
                next_block_start_addr = matrix_start_addr + 
                                    ((start_row + real_block_rows) * m_cols * sizeof(T));
                rowloop_complete = true;
            }
        } else {
            //先列后行循环
            next_block_start_addr = start_addr + (real_block_rows * m_cols * sizeof(T));
            rowloop_complete = false;
            if ((start_row + real_block_rows) == m_rows) {
                next_block_start_addr = matrix_start_addr + 
                                    (start_col + real_block_cols) * sizeof(T);
                rowloop_complete = true;
            }
        }

    }
    void transfer_back(
        uint64_t start_addr, 
        uint64_t target_start_addr,
        uint64_t target_end_addr,
        int am_rows,
        int am_cols,
        int ddr_rows,
        int ddr_cols,
        const tlm::tlm_dmi& source_dmi, 
        const tlm::tlm_dmi& target_dmi
    ){
        vector<T> block_buffer(am_cols);
        uint64_t end_addr;
        for(int i = 0; i<am_rows; i++){
            read_data(start_addr + i * am_cols * sizeof(T), block_buffer, source_dmi, am_cols);
            //check_all_zero(block_buffer);
            write_data(target_start_addr + i * ddr_cols * sizeof(T), end_addr, block_buffer, target_dmi, am_cols);
        }
        if(end_addr != target_end_addr){
            cout<<"Error transfer_back: "<<transfer_name<<endl;
            cout<<"start_addr:"<<start_addr<<",target_start_addr:"<<target_start_addr<<endl;
            cout<<"end_addr:"<<end_addr<<",target_end_addr:"<<target_end_addr<<endl;
            cout<<"am_rows:"<<am_rows<<",am_cols:"<<am_cols<<endl;
            cout<<"ddr_rows:"<<ddr_rows<<",ddr_cols:"<<ddr_cols<<endl;  
            sc_stop();
            return;
        }
        
    }
};

template <typename T>
struct Gemm : public BaseInitiatorModel<T> {
    // tlm_utils::multi_passthrough_initiator_socket<512,Gemm> socket;
    SC_CTOR(Gemm) : BaseInitiatorModel<T>("Gemm"){
        // socket.register_invalidate_direct_mem_ptr(this, &Gemm::invalidate_direct_mem_ptr);
        SC_THREAD(Gemm_top_thread);
        SC_THREAD(Gemm_init_process);
        SC_THREAD(Gemm_computing_process);
        SC_THREAD(Gemm_writeback_C_process);
    }
public:
    using BaseInitiatorModel<T>::socket;
    using BaseInitiatorModel<T>::am_dmi;
    using BaseInitiatorModel<T>::sm_dmi;
    using BaseInitiatorModel<T>::ddr_dmi;
    using BaseInitiatorModel<T>::gsm_dmi;
    using BaseInitiatorModel<T>::vector_mac;
    //这两个事件是与外界交互的，用于启动和结束GEMM计算
    sc_event start_gemm_event,gemm_done_event;
    //实际矩阵大小，这也是与外界交互的内容，用于初始化矩阵
    int A_rows = 0, A_cols = 0, B_rows = 0, B_cols = 0, C_rows = A_rows, C_cols = B_cols;
    //与子类交互的地址
    uint64_t Gemm_data_start_addr_ddr;
    uint64_t Gemm_data_start_addr_am;
    uint64_t Gemm_result_start_addr;

    sc_event Gemm_init_start_event,Gemm_init_done_event;
    sc_event Gemm_compute_start_event,Gemm_kernel_compute_done_event;
    sc_event Gemm_C_write_back_start_event,Gemm_C_write_back_done_event;  

    vector<T> MatrixA;
    vector<T> MatrixB;
    vector<T> MatrixC;
    // 将vector声明移到循环外，避免重复创建和销毁
    vector<T> A_block_1D;
    vector<T> B_block_1D;
    vector<T> C_block_1D;
    
    int GSM_row_cnt = 0;
    enum Matrix_flag{A=0,B=1,C=2};
    enum flag_A {DDR_A=0,GSM=1,GSMSM=2,SM=3};
    enum flag_BC{DDR_BC=0,AM=1};
    enum flag_start_end{start=0,end=1};
    enum flag_size{row=0,col=1};

    //初始化,实际矩阵块大小0_row,1_col
    array<int, 2> A_GSM_size = {0, 0};
    array<int, 2> A_SM_size = {0, 0};
    array<int, 2> B_AM_size = {0, 0};
    array<int, 2> C_AM_size = {0, 0};
    array<int, 2> C_DDR_size = {0, 0}; // 新增：C矩阵写回DDR的大小记录

    // 用于跟踪当前处理的块位置
    int current_M_block = 0;
    int current_K_block = 0;
    int current_N_block = 0;
    bool M_complete = false;
    bool K_complete = false;
    bool N_complete = false;
    bool SM_complete = false;

    uint64_t A_GSM_addr_flag;
    uint64_t temp_end_addr;  // 临时变量存储写入结束地址
    //实际DDR中矩阵起始结束地址
    array<array<uint64_t, 2>, 3> Matrix_addr = {{{0, 0}, {0, 0}, {0, 0}}};
    //分块矩阵的起始结束地址
    array<array<uint64_t, 2>, 4> A_addr = {{{0, 0}, {0, 0}, {0, 0}, {0, 0}}};
    array<array<uint64_t, 2>, 2> B_addr = {{{0, 0}, {0, 0}}};
    array<array<uint64_t, 2>, 2> C_addr = {{{0, 0}, {0, 0}}};   
    array<array<uint64_t, 2>, 3> A_next_addr = {{{0, 0}, {0, 0}, {0, 0}}};
    array<array<uint64_t, 2>, 2> B_next_addr = {{{0, 0}, {0, 0}}};
    array<array<uint64_t, 2>, 2> C_next_addr = {{{0, 0}, {0, 0}}}; 



    void read_data(uint64_t addr, vector<T>& values, const tlm::tlm_dmi& dmi, unsigned int data_num) {
        ins::read_from_dmi(addr, values, dmi, data_num, "Gemm");
    }
    
    void write_data(uint64_t start_addr, uint64_t& end_addr, const vector<T>& values, 
                   const tlm::tlm_dmi& dmi, unsigned int data_num) {
        ins::write_to_dmi(start_addr, end_addr, values, dmi, data_num, "Gemm");
    }
    
    //向量外积C_1D=A_1D*B_1D+C_1D
    void kernel_mul(
        vector<T>& A_1D,
        vector<T>& B_1D,
        vector<T>& C_1D,
        const int rows_a,
        const int cols_a,
        const int rows_b,
        const int cols_b,
        const int rows_c,
        const int cols_c
    ){
        // 定义事务和延迟
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        //向量外积
        // 遍历 A 的每一行M
        vector<T> vec1(cols_b);
        vector<T> vec2(cols_b);
        vector<T> vec3(cols_b);
        vector<T> merged_vec(cols_b*3);
        for(int m = 0; m < rows_a; m++){
            //遍历A的每一列K,B的每一行K，根据规定，B一行的元素不会超过macs_per_vpu
            for(int k = 0; k < cols_a; k++){
                //C[m][start:end] += A[m][k]*B[k][start:end]
                //A[m][k]重复cols_b次,构成vec1,模拟SVR广播
                //B[k][:]取出一整行，构成vec2
                //C[m][:]取出一整行，构成vec3
                for(int i = 0; i < cols_b; i++){
                    vec1[i] = A_1D[m*cols_a+k];
                    vec2[i] = B_1D[k*cols_b+i];
                    vec3[i] = C_1D[m*cols_c+i];
                }

                vector_mac(vec1, vec2, vec3, cols_b);
                //C的中间结果写回
                for(int i = 0; i < cols_b; i++){
                    C_1D[m*cols_c+i] = vec3[i];
                }
            }
        }
    }


    //GEMM C=A*B+C 外积循环逻辑：
        //已知：A：M*K,B:K*N,C:M*N
        // for m = M/m_gsm_max
        //     for k = K/k_gsm_max
        //         A:m_gsm_max*k_gsm_max(DDR->GSM)
        //         for n = N/cu_max
        //             B:k_gsm_max*cu_max(DDR->AM)
        //             C:k_gsm_max*cu_max(DDR->AM)
        //             for sm = m_gsm_max/sm_max
        //                 A_sm:sm_max*k_gsm_max(GSM->SM)
        //                 C += A*B
        //             C:k_gsm_max*cu_max(AM->DDR)
    void Gemm_top_thread() {
        while(true){
            wait(start_gemm_event);
            Gemm_init_start_event.notify();
            wait(Gemm_init_done_event);
            cout << sc_time_stamp() << "=====================GEMM初始化完成============================" << endl;
            int m,k,n,sm;
            
            // 初始化矩阵传输对象
            MatrixBlockTransfer<T> gsm_transfer("GSM_Transfer");
            MatrixBlockTransfer<T> sm_transfer("SM_Transfer");
            MatrixBlockTransfer<T> amB_transfer("AMB_Transfer");
            MatrixBlockTransfer<T> amC_transfer("AMC_Transfer");
            
            // 预计算循环次数
            int M_blocks = (A_rows + m_gsm_max - 1) / m_gsm_max;  // M方向的块数1
            int K_blocks = (A_cols + k_gsm_max - 1) / k_gsm_max;  // K方向的块数2
            int N_blocks = (B_cols + cu_max - 1) / cu_max;        // N方向的块数2
            cout<<"M_blocks: "<<M_blocks<<endl;
            cout<<"K_blocks: "<<K_blocks<<endl;
            cout<<"N_blocks: "<<N_blocks<<endl;


            // M方向循环 (处理MatrixA的行)
            for(m = 0; m < M_blocks && !M_complete; m++) {
                // 计算当前M块的实际大小
                int current_m_size = min(m_gsm_max, A_rows - m * m_gsm_max);
                for(k = 0; k < K_blocks && !K_complete; k++) {
                    gsm_transfer.transfer(
                        A_addr[DDR_A][start],
                        A_addr[DDR_A][end],
                        A_next_addr[DDR_A][start],
                        Matrix_addr[A][start],
                        Matrix_addr[A][end],
                        A_addr[GSM][start],
                        A_addr[GSM][end],
                        A_rows, A_cols,
                        m_gsm_max, k_gsm_max,
                        A_GSM_size[row], A_GSM_size[col],
                        ddr_dmi, gsm_dmi,
                        true,
                        K_complete
                    );


                    // N方向循环 (处理MatrixB的列)
                    for(n = 0; n < N_blocks && !N_complete; n++) {
                        // 计算当前N块的实际大小

                        // 从DDR加载B矩阵块到AM
                        amB_transfer.transfer(
                            B_addr[DDR_BC][start],
                            B_addr[DDR_BC][end],
                            B_next_addr[DDR_BC][start],
                            Matrix_addr[B][start],
                            Matrix_addr[B][end],
                            B_addr[AM][start],
                            B_addr[AM][end],
                            B_rows, B_cols,
                            k_gsm_max, cu_max,
                            B_AM_size[row], B_AM_size[col],
                            ddr_dmi, am_dmi,
                            true,
                            N_complete
                        );

                        // 从DDR加载C矩阵块到AM
                        C_addr[AM][start] =B_addr[AM][end] + 1;

                        amC_transfer.transfer(
                            C_addr[DDR_BC][start],
                            C_addr[DDR_BC][end],
                            C_next_addr[DDR_BC][start],
                            Matrix_addr[C][start],
                            Matrix_addr[C][end],
                            C_addr[AM][start],
                            C_addr[AM][end],
                            C_rows, C_cols,
                            m_gsm_max, cu_max,
                            C_AM_size[row], C_AM_size[col],
                            ddr_dmi, am_dmi,
                            true,
                            N_complete
                        );

                        // SM方向循环 (处理GSM中A矩阵的行分块)
                        int SM_blocks = (current_m_size + sm_max - 1) / sm_max;
                        // cout<<"SM_blocks: "<<SM_blocks<<endl;
                        A_addr[GSMSM][start] = A_addr[GSM][start];
                        for(sm = 0; sm < SM_blocks; sm++) {
                            sm_transfer.transfer(
                                A_addr[GSMSM][start],
                                A_addr[GSMSM][end],
                                A_next_addr[GSMSM][start],
                                A_addr[GSM][start],
                                A_addr[GSM][end],
                                A_addr[SM][start],
                                A_addr[SM][end],
                                A_GSM_size[row], A_GSM_size[col],
                                sm_max, k_gsm_max,
                                A_SM_size[row], A_SM_size[col],
                                gsm_dmi, sm_dmi,
                                false, //列循环
                                SM_complete
                            );

                            A_GSM_addr_flag = A_addr[GSMSM][start];
                            A_addr[GSMSM][start] = A_next_addr[GSMSM][start];
                            
                            // 执行计算
                            Gemm_compute_start_event.notify();
                            wait(Gemm_kernel_compute_done_event);

                        }
                        // 等待计算结果写回
                        Gemm_C_write_back_start_event.notify();
                        wait(Gemm_C_write_back_done_event);

                        B_addr[DDR_BC][start] = B_next_addr[DDR_BC][start];
                        C_addr[DDR_BC][start] = C_next_addr[DDR_BC][start];
                    }
                    if(!K_complete){
                            //K方向循环未完成，N方向重新开始循环
                            N_complete= false;
                            //C分块起始地址回到本行第一个分块，重新开始N放方向循环
                            //B_addr[DDR_BC][start] = B_addr[DDR_BC][start] - B_AM_size[row]*B_cols*sizeof(T);
                            C_addr[DDR_BC][start] = C_addr[DDR_BC][start] - C_AM_size[row]*C_cols*sizeof(T);
                    }
                    A_addr[DDR_A][start] = A_next_addr[DDR_A][start];
                    //B_addr[DDR_BC][start] = B_next_addr[DDR_BC][start];
                    
                }
                if(!M_complete){
                    //M方向循环未完成，K方向重新开始循环
                    K_complete = false;
                    N_complete = false;
                    //B分块起始地址回到整个矩阵的第一个分块
                    B_addr[DDR_BC][start] = Matrix_addr[B][start];
                    //C_addr[DDR_BC][start] = Matrix_addr[C][start];
                }

            }
            
            //MatrixA_DDR_empty = true;
            read_data(Matrix_addr[C][start], MatrixC, ddr_dmi, C_rows * C_cols);
            cout << sc_time_stamp()<< "=====================Gemm计算完成============================" << endl;
            gemm_done_event.notify();
        }
    }
    void Gemm_init_process(){
        while(true){
            wait(Gemm_init_start_event);

            // 初始化矩阵在DDR中的起始结束地址
            Matrix_addr[A][start] = Gemm_data_start_addr_ddr;
            Matrix_addr[A][end] = Matrix_addr[A][start] + A_rows * A_cols * sizeof(T)-1;
            Matrix_addr[B][start] =  Matrix_addr[A][end]+1;
            Matrix_addr[B][end] = Matrix_addr[B][start] + (B_rows * B_cols) * sizeof(T)-1;
            Matrix_addr[C][start] = Gemm_result_start_addr;
            Matrix_addr[C][end] = Matrix_addr[C][start] + (C_rows * C_cols) * sizeof(T)-1;
            A_addr[DDR_A][start] = Matrix_addr[A][start];
            B_addr[DDR_BC][start] = Matrix_addr[B][start];
            C_addr[DDR_BC][start] = Matrix_addr[C][start];
            A_addr[GSM][start] = GSM_BASE_ADDR;
            A_addr[GSMSM][start] = GSM_BASE_ADDR;
            A_addr[SM][start] = SM_BASE_ADDR;
            B_addr[AM][start] = Gemm_data_start_addr_am;
            
            
            // Write batch data to DDR
            //cout << "=== MatrixA Write to DDR ===" << endl;
            int MatrixA_num = A_rows * A_cols;
            // write_data(A_addr[DDR_A][start], A_addr[DDR_A][end], MatrixA, ddr_dmi, MatrixA_num);
            //cout << "=== MatrixB Write to DDR ===" << endl;
            int MatrixB_num = B_rows * B_cols;
            // write_data(B_addr[DDR_BC][start], B_addr[DDR_BC][end], MatrixB, ddr_dmi, MatrixB_num);
            //cout << "=== MatrixC Write to DDR ===" << endl;
            int MatrixC_num = C_rows * C_cols;
            // write_data(C_addr[DDR_BC][start], C_addr[DDR_BC][end], MatrixC, ddr_dmi, MatrixC_num);
            M_complete = false;
            K_complete = false;
            N_complete = false;
            SM_complete = false;
        
            Gemm_init_done_event.notify();
            //cout << sc_time_stamp() << "=====================初始化完成============================" << endl; 
        }
    }
    void Gemm_computing_process() {
        while(true) {
            // 等待新的计算任务
            wait(Gemm_compute_start_event);
            
            vector<T> C_block_1D(A_SM_size[row]*B_AM_size[col]);
            vector<T> A_block_1D(A_SM_size[row]*A_SM_size[col]);
            vector<T> B_block_1D(B_AM_size[row]*B_AM_size[col]);
            // 从SM读取A块
            read_data(A_addr[SM][start], A_block_1D, sm_dmi, A_SM_size[row]*A_SM_size[col]);
            //从AM读取B块
            read_data(B_addr[AM][start], B_block_1D, am_dmi, B_AM_size[row]*B_AM_size[col]);
            //从AM读取C块
            // 计算C块在AM中的偏移
            uint64_t C_offset = (A_GSM_addr_flag - A_addr[GSM][start]) / ( A_GSM_size[col]) * C_AM_size[col];
            read_data(C_addr[AM][start]+C_offset, C_block_1D, am_dmi, A_SM_size[row]*B_AM_size[col]);

        
            //cout << sc_time_stamp()<< "============准备开始一次kernel计算============="<<endl;
            // 执行矩阵乘法计算
            kernel_mul(
                A_block_1D, B_block_1D, C_block_1D,
                A_SM_size[row], A_SM_size[col],
                B_AM_size[row], B_AM_size[col],
                A_SM_size[row], B_AM_size[col]
            );

            // 将计算结果写回AM
            // 计算C块在AM中的偏移  
            uint64_t temp_end_addr;  // 临时变量存储写入结束地址
            write_data(C_addr[AM][start]+C_offset, temp_end_addr, C_block_1D, am_dmi, A_SM_size[row]*B_AM_size[col]);

            // 通知计算完成
            Gemm_kernel_compute_done_event.notify();
            //cout << "kernel计算完成" << endl;
            
        }
    }
    void Gemm_writeback_C_process() {
        MatrixBlockTransfer<T> amCback_transfer("AMCback_Transfer");
        while(true) {
            wait(Gemm_C_write_back_start_event);
            amCback_transfer.transfer_back(
                C_addr[AM][start],
                C_addr[DDR_BC][start],
                C_addr[DDR_BC][end],
                C_AM_size[row],
                C_AM_size[col],
                C_rows,
                C_cols,
                am_dmi,
                ddr_dmi
            );

            Gemm_C_write_back_done_event.notify();
        }
    }
};
#endif
