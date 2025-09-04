#ifndef AM_H
#define AM_H

#include "../../util/const.h"
#include "../../util/tools.h"


// 定义AM子模块（Array Memory）
template<typename T>
class AM : public sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<AM, 512> dma2am_target_socket;
    
    SC_CTOR(AM) : dma2am_target_socket("dma2am_target_socket") {
        dma2am_target_socket.register_b_transport(this, &AM::b_transport);
        dma2am_target_socket.register_get_direct_mem_ptr(this, &AM::get_direct_mem_ptr);
        memory.resize(AM_SIZE / sizeof(T));
    }

    bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        dmi_data.set_start_address(AM_BASE_ADDR);
        dmi_data.set_end_address(AM_BASE_ADDR + AM_SIZE - 1);
        dmi_data.set_dmi_ptr(reinterpret_cast<unsigned char*>(memory.data()));
        dmi_data.set_read_latency(AM_LATENCY);
        dmi_data.set_write_latency(AM_LATENCY);
        dmi_data.allow_read_write();
        return true;
    }

    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay) {
        uint64_t addr = (trans.get_address() - AM_BASE_ADDR) / sizeof(T);
        
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            T* data = reinterpret_cast<T*>(trans.get_data_ptr());
            memory[addr] = *data;
        } else {
            T* data = reinterpret_cast<T*>(trans.get_data_ptr());
            *data = memory[addr];
        }
        
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        wait(AM_LATENCY);
    }

private:
    vector<T> memory;
};
#endif