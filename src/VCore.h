#ifndef VCORE_H
#define VCORE_H

#include "../util/const.h"
#include "../util/tools.h"

#include "vcore/DMA.h"
#include "vcore/SPU.h"
#include "vcore/VPU.h"
#include "vcore/AM.h"
#include "vcore/SM.h"
#include "vcore/GEMM_SA/include/GEMM_TLM.h"
using namespace sc_core;
using namespace sc_dt;
using namespace std;



// 修改VCore以包含子模块
template<typename T>
class VCore : public sc_module {
public:
    //外部socket
    tlm_utils::multi_passthrough_target_socket<VCore, 512> soc2vcore_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<VCore, 512> vcore2cac_init_socket;
    tlm_utils::multi_passthrough_initiator_socket<VCore, 512> vcore2soc_init_socket;
    //内部socket
    tlm_utils::multi_passthrough_target_socket<VCore, 512> spu2vcore_target_socket;
    tlm_utils::multi_passthrough_target_socket<VCore, 512> dma2vcore_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<VCore, 512> vcore2spu_init_socket;
    tlm_utils::multi_passthrough_target_socket<VCore, 512> gemm2vcore_target_socket;


    VPU<T>* vpu;
    SM<T>* sm;
    AM<T>* am;
    SPU<T>* spu;
    DMA<T>* dma;
    
    // GEMM模式加速器模块
    GEMM_TLM<T, GEMM_TLM_N, GEMM_TLM_buf_depth>* gemm_tlm;

    VCore(sc_module_name name) : sc_module(name), 
                                soc2vcore_target_socket("soc2vcore_target_socket"),
                                vcore2cac_init_socket("vcore2cac_init_socket"),
                                spu2vcore_target_socket("spu2vcore_target_socket"),
                                dma2vcore_target_socket("dma2vcore_target_socket"),
                                vcore2spu_init_socket("vcore2spu_init_socket"),
                                gemm2vcore_target_socket("gemm2vcore_target_socket"),
                                vcore2soc_init_socket("vcore2soc_init_socket") {
        // 注册所有回调函数
        soc2vcore_target_socket.register_b_transport(this, &VCore::soc2vcore_b_transport);
        soc2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::soc2vcore_get_direct_mem_ptr);
        
        spu2vcore_target_socket.register_b_transport(this, &VCore::spu2vcore_b_transport);
        spu2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::spu2vcore_get_direct_mem_ptr);
        
        dma2vcore_target_socket.register_b_transport(this, &VCore::dma2vcore_b_transport);
        dma2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::dma2vcore_get_direct_mem_ptr);
        gemm2vcore_target_socket.register_b_transport(this, &VCore::gemm2vcore_b_transport);
        gemm2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::gemm2vcore_get_direct_mem_ptr);

        // 创建子模块
        vpu = new VPU<T>("vpu");
        sm = new SM<T>("sm");
        am = new AM<T>("am");
        dma = new DMA<T>("dma");
        spu = new SPU<T>("spu");

        // 创建GEMM加速器模块
        gemm_tlm = new GEMM_TLM<T, GEMM_TLM_N, GEMM_TLM_buf_depth>("gemm_tlm");
        //gemm-sa = new GEMM_TLM

        // 连接GEMM模块
        spu->spu2gemm_init_socket.bind(gemm_tlm->spu2gemm_target_socket);
        gemm_tlm->gemm2vcore_init_socket.bind(gemm2vcore_target_socket);

        // 绑定所有socket
        this->vcore2spu_init_socket.bind(spu->vcore2spu_target_socket);
        spu->spu2cac_init_socket.bind(spu2vcore_target_socket);
        spu->spu2dma_init_socket.bind(dma->spu2dma_target_socket);
        spu->spu2vpu_init_socket.bind(vpu->spu2vpu_target_socket);
        dma->dma2sm_init_socket.bind(sm->dma2sm_target_socket);
        dma->dma2am_init_socket.bind(am->dma2am_target_socket);
        dma->dma2vcore_init_socket.bind(dma2vcore_target_socket);
    }
    void soc2vcore_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        this->vcore2spu_init_socket->b_transport(trans, delay);
    }
    void spu2vcore_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        this->vcore2cac_init_socket->b_transport(trans, delay);
    }
    void dma2vcore_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        this->vcore2cac_init_socket->b_transport(trans, delay);
    }

    void gemm2vcore_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        this->vcore2soc_init_socket->b_transport(trans, delay);
    }
    virtual bool soc2vcore_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return vcore2spu_init_socket->get_direct_mem_ptr(trans, dmi_data);
    }
    virtual bool spu2vcore_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return vcore2cac_init_socket->get_direct_mem_ptr(trans, dmi_data);
    }
    virtual bool dma2vcore_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return vcore2cac_init_socket->get_direct_mem_ptr(trans, dmi_data);
    }

    virtual bool gemm2vcore_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return vcore2soc_init_socket->get_direct_mem_ptr(trans, dmi_data);
    }

    ~VCore() {
        delete vpu;
        delete sm;
        delete am;
        delete spu;
        delete dma;
        delete gemm_tlm;
    }
};

#endif
