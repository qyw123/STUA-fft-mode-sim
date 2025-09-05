#ifndef SPU_H
#define SPU_H

#include "../../util/const.h"
#include "../../util/tools.h"

template<typename T>
class SPU : public sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<SPU, 512> vcore2spu_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<SPU, 512> spu2cac_init_socket;
    tlm_utils::multi_passthrough_initiator_socket<SPU, 512> spu2vpu_init_socket;
    tlm_utils::multi_passthrough_initiator_socket<SPU, 512> spu2dma_init_socket;
    tlm_utils::multi_passthrough_initiator_socket<SPU, 512> spu2fft_init_socket;
    SC_CTOR(SPU) : vcore2spu_target_socket("vcore2spu_target_socket"), 
                spu2cac_init_socket("spu2cac_init_socket"), 
                spu2vpu_init_socket("spu2vpu_init_socket"), 
                spu2dma_init_socket("spu2dma_init_socket") 
    {
        vcore2spu_target_socket.register_b_transport(this, &SPU::b_transport);
        vcore2spu_target_socket.register_get_direct_mem_ptr(this, &SPU::get_direct_mem_ptr);
        spu2cac_init_socket.register_invalidate_direct_mem_ptr(this, &SPU::invalidate_direct_mem_ptr);
        spu2vpu_init_socket.register_invalidate_direct_mem_ptr(this, &SPU::invalidate_direct_mem_ptr);
        spu2dma_init_socket.register_invalidate_direct_mem_ptr(this, &SPU::invalidate_direct_mem_ptr);
        spu2fft_init_socket.register_invalidate_direct_mem_ptr(this, &SPU::invalidate_direct_mem_ptr);

    }
    //阻塞传输方法
    void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay )
    {
        sc_dt::uint64 address = trans.get_address();
        if (address >= GSM_BASE_ADDR && address < DDR_BASE_ADDR + DDR_SIZE) {
            spu2cac_init_socket->b_transport(trans, delay);
            // CAC方向
        } else if (address >= VPU_BASE_ADDR && address < VPU_BASE_ADDR + VPU_REGISTER_SIZE) {
            spu2vpu_init_socket->b_transport(trans, delay);
            // VPU方向
        } else if (address >= SM_BASE_ADDR && address < SM_BASE_ADDR + SM_SIZE ||
                   address >= AM_BASE_ADDR && address < AM_BASE_ADDR + AM_SIZE ||
                   address >= DMA_BASE_ADDR && address < DMA_BASE_ADDR + DMA_SIZE) {
            spu2dma_init_socket->b_transport(trans, delay);
            // DMA方向

            
        }else if (address >= FFT_BASE_ADDR && address < FFT_BASE_ADDR + FFT_SIZE) {
            spu2fft_init_socket->b_transport(trans, delay);
            // FFT_TLM方向
        }
        else{
            SC_REPORT_ERROR("SPU", "b_transport:Address out of range");
            sc_stop(); // 
            return;
        }
    }
    //DMI请求方法
    bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        sc_dt::uint64 address = trans.get_address();
        if (address >= GSM_BASE_ADDR && address < DDR_BASE_ADDR + DDR_SIZE) {
            return spu2cac_init_socket->get_direct_mem_ptr(trans, dmi_data);
            // CAC方向
        } else if (address >= VPU_BASE_ADDR && address < VPU_BASE_ADDR + VPU_REGISTER_SIZE) {
            return spu2vpu_init_socket->get_direct_mem_ptr(trans, dmi_data);
            // VPU方向
        } else if (address >= SM_BASE_ADDR && address < SM_BASE_ADDR + SM_SIZE ||
                   address >= AM_BASE_ADDR && address < AM_BASE_ADDR + AM_SIZE ||
                   address >= DMA_BASE_ADDR && address < DMA_BASE_ADDR + DMA_SIZE) {
            return spu2dma_init_socket->get_direct_mem_ptr(trans, dmi_data);
            // DMA方向

        } else if (address >= FFT_BASE_ADDR && address < FFT_BASE_ADDR + FFT_SIZE) {
            return spu2fft_init_socket->get_direct_mem_ptr(trans, dmi_data);
            // FFT_TLM方向
        }
        else{
            SC_REPORT_ERROR("SPU", "get_direct_mem_ptr:Address out of range");
            sc_stop(); // 或者 throw std::runtime_error("Address out of range");
            return false;
        }
    }
    virtual void invalidate_direct_mem_ptr(int id, sc_dt::uint64 start_range, 
                                         sc_dt::uint64 end_range) 
    {
        // 收到 DMI 失效通知时，清除对应的 DMI 指针
        dmi_ptr_valid = false;
        
        // 如果需要，可以记录日志
        SC_REPORT_INFO("SPU", "invalidate_direct_mem_ptr:DMI access invalidated");
    }

private:
    bool dmi_ptr_valid;  // 标记 DMI 指针是否有效
};

#endif
