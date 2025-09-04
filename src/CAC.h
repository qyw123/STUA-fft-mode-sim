#ifndef CAC_H
#define CAC_H

#include "../util/const.h"
#include "../util/tools.h"

template<typename T>
class CAC : public sc_module
{
public:
    //外部socket
    tlm_utils::multi_passthrough_target_socket<CAC, 512> vcore2cac_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<CAC, 512> cac2ddr_initiator_socket;
    tlm_utils::multi_passthrough_initiator_socket<CAC, 512> cac2gsm_initiator_socket;
    SC_CTOR(CAC) : vcore2cac_target_socket("vcore2cac_target_socket"), 
                cac2ddr_initiator_socket("cac2ddr_initiator_socket"), 
                cac2gsm_initiator_socket("cac2gsm_initiator_socket")
    {
        vcore2cac_target_socket.register_b_transport(this, &CAC::b_transport);
        vcore2cac_target_socket.register_get_direct_mem_ptr(this, &CAC::get_direct_mem_ptr);

        cac2ddr_initiator_socket.register_invalidate_direct_mem_ptr(this, &CAC::invalidate_direct_mem_ptr);
        cac2gsm_initiator_socket.register_invalidate_direct_mem_ptr(this, &CAC::invalidate_direct_mem_ptr);
    }
    //阻塞传输方法
    virtual void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay )
    {
        sc_dt::uint64 addr = trans.get_address();
        if (addr >= DDR_BASE_ADDR && addr < DDR_BASE_ADDR + DDR_SIZE) {
            cac2ddr_initiator_socket->b_transport(trans, delay);
        } else if (addr >= GSM_BASE_ADDR && addr < GSM_BASE_ADDR + GSM_SIZE) {
            cac2gsm_initiator_socket->b_transport(trans, delay);
        }

        else{
            SC_REPORT_ERROR("CAC", "b_transport:Address out of range");
        }
    }
    //DMI请求方法
    virtual bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        sc_dt::uint64 addr = trans.get_address();
        if (addr >= DDR_BASE_ADDR && addr < DDR_BASE_ADDR + DDR_SIZE) {
            return cac2ddr_initiator_socket->get_direct_mem_ptr(trans, dmi_data);
        } else if (addr >= GSM_BASE_ADDR && addr < GSM_BASE_ADDR + GSM_SIZE) {
            return cac2gsm_initiator_socket->get_direct_mem_ptr(trans, dmi_data);
        }
        // } else if (addr >= VCORE_BASE_ADDR && addr < VCORE_BASE_ADDR + VCORE_SIZE) {
        //     return (*initiator_socket[VCore_id])->get_direct_mem_ptr(trans, dmi_data);
        // }
        else{
            SC_REPORT_ERROR("CAC", "get_direct_mem_ptr:Address out of range");
        }
    }

    virtual void invalidate_direct_mem_ptr(int id,  // id 参数移到第一位
                                        sc_dt::uint64 start_range, 
                                        sc_dt::uint64 end_range)
    {
        // 收到 DMI 失效通知时，清除对应的 DMI 指针
        dmi_ptr_valid = false;
        
        // 如果需要，可以记录日志
        SC_REPORT_INFO("CAC", ("DMI access invalidated for id " + std::to_string(id)).c_str());
    }

private:
    bool dmi_ptr_valid;  // 标记 DMI 指针是否有效

};

#endif
