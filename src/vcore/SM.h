#ifndef SM_H
#define SM_H

#include "../../util/const.h"
#include "../../util/tools.h"

// 定义SM子模块（Scalar Memory）
template<typename T>
class SM : public sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<SM, 512> dma2sm_target_socket;
    
    SC_CTOR(SM) : dma2sm_target_socket("dma2sm_target_socket") {
        dma2sm_target_socket.register_b_transport(this, &SM::b_transport);
        dma2sm_target_socket.register_get_direct_mem_ptr(this, &SM::get_direct_mem_ptr);
        memory.resize(SM_SIZE / sizeof(T));
    }

    bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        dmi_data.set_start_address(SM_BASE_ADDR);
        dmi_data.set_end_address(SM_BASE_ADDR + SM_SIZE - 1);
        dmi_data.set_dmi_ptr(reinterpret_cast<unsigned char*>(memory.data()));
        dmi_data.set_read_latency(SM_LATENCY);
        dmi_data.set_write_latency(SM_LATENCY);
        dmi_data.allow_read_write();
        return true;
    }

    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay) {
        uint64_t addr = (trans.get_address() - SM_BASE_ADDR) / sizeof(T);
        
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            T* data = reinterpret_cast<T*>(trans.get_data_ptr());
            memory[addr] = *data;
        } else {
            T* data = reinterpret_cast<T*>(trans.get_data_ptr());
            *data = memory[addr];
        }
        
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        wait(SM_LATENCY);
    }

private:
    vector<T> memory;
};
#endif