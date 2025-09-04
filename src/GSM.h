#ifndef GSM_H
#define GSM_H

#include "../util/const.h"  // 引入公用常量和头文件
#include "../util/tools.h"

template<typename T>
class GSM : public sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<GSM, 512> cac2gsm_target_socket;

    GSM(sc_module_name name) : sc_module(name), cac2gsm_target_socket("cac2gsm_target_socket") {
        cac2gsm_target_socket.register_get_direct_mem_ptr(this, &GSM::get_direct_mem_ptr);
        cac2gsm_target_socket.register_b_transport(this, &GSM::b_transport);
        mem = new T[GSM_SIZE / sizeof(T)];
        for (uint64_t i = 0; i < GSM_SIZE / sizeof(T); i++) {
            mem[i] = T();  // Initialize memory
        }
    }
        //阻塞传输方法
    virtual void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay) {
        uint64_t data_length = trans.get_data_length();
        //更新延时信息
        delay += DDR_LATENCY*calculate_clock_cycles(data_length, DDR_DATA_WIDTH);
        wait(delay);
    }
    virtual bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        dmi_data.set_start_address(GSM_BASE_ADDR);
        dmi_data.set_end_address(GSM_BASE_ADDR + GSM_SIZE - 1);
        dmi_data.set_dmi_ptr(reinterpret_cast<unsigned char*>(mem));
        dmi_data.set_read_latency(GSM_LATENCY);
        dmi_data.set_write_latency(GSM_LATENCY);
        dmi_data.allow_read_write();
        return true;
    }

    ~GSM() {
        delete[] mem;
    }

private:
    T* mem;
};  

#endif
