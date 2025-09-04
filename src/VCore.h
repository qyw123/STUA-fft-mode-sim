#ifndef VCORE_H
#define VCORE_H

#include "../util/const.h"
#include "../util/tools.h"

#include "vcore/DMA.h"
#include "vcore/SPU.h"
#include "vcore/VPU.h"
#include "vcore/AM.h"
#include "vcore/SM.h"
// #include "vcore/PEA/systolic_array_top_tlm.h"
#include "vcore/FFT_SA/include/FFT_TLM.h"
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
    // tlm_utils::multi_passthrough_target_socket<VCore, 512> pea2vcore_target_socket;
    tlm_utils::multi_passthrough_target_socket<VCore, 512> fft2vcore_target_socket;


    VPU<T>* vpu;
    SM<T>* sm;
    AM<T>* am;
    SPU<T>* spu;
    DMA<T>* dma;
    // static const int pe_array_size = 16;
    // SYSTOLIC_ARRAY_TOP_TLM<T,pe_array_size,pe_array_size,pe_array_size,pe_array_size>* pea;
    
    // FFT加速器模块
    FFT_TLM<T, FFT_TLM_N, 8>* fft_tlm;

    VCore(sc_module_name name) : sc_module(name), 
                                soc2vcore_target_socket("soc2vcore_target_socket"),
                                vcore2cac_init_socket("vcore2cac_init_socket"),
                                spu2vcore_target_socket("spu2vcore_target_socket"),
                                dma2vcore_target_socket("dma2vcore_target_socket"),
                                vcore2spu_init_socket("vcore2spu_init_socket"),
                                // pea2vcore_target_socket("pea2vcore_target_socket") ,
                                fft2vcore_target_socket("fft2vcore_target_socket"),
                                vcore2soc_init_socket("vcore2soc_init_socket") {
        // 注册所有回调函数
        soc2vcore_target_socket.register_b_transport(this, &VCore::soc2vcore_b_transport);
        soc2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::soc2vcore_get_direct_mem_ptr);
        
        spu2vcore_target_socket.register_b_transport(this, &VCore::spu2vcore_b_transport);
        spu2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::spu2vcore_get_direct_mem_ptr);
        
        dma2vcore_target_socket.register_b_transport(this, &VCore::dma2vcore_b_transport);
        dma2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::dma2vcore_get_direct_mem_ptr);

        // pea2vcore_target_socket.register_b_transport(this, &VCore::pea2vcore_b_transport);
        // pea2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::pea2vcore_get_direct_mem_ptr);
        
        fft2vcore_target_socket.register_b_transport(this, &VCore::fft2vcore_b_transport);
        fft2vcore_target_socket.register_get_direct_mem_ptr(this, &VCore::fft2vcore_get_direct_mem_ptr);

        // 创建子模块
        vpu = new VPU<T>("vpu");
        sm = new SM<T>("sm");
        am = new AM<T>("am");
        dma = new DMA<T>("dma");
        spu = new SPU<T>("spu");
        // pea = new SYSTOLIC_ARRAY_TOP_TLM<T,pe_array_size,pe_array_size,pe_array_size,pe_array_size>("systolic_array_tlm");
        
        // 创建FFT加速器模块
        fft_tlm = new FFT_TLM<T, FFT_TLM_N, FFT_TLM_buf_depth>("fft_tlm");
        //gemm-sa = new GEMM_TLM
        
        // spu->spu2pea_init_socket.bind(pea->spu2pea_target_socket);
        // pea->pea2vcore_init_socket.bind(pea2vcore_target_socket);
        
        // 连接FFT模块
        spu->spu2fft_init_socket.bind(fft_tlm->spu2fft_target_socket);
        fft_tlm->fft2vcore_init_socket.bind(fft2vcore_target_socket);

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
    // void pea2vcore_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
    //     this->vcore2soc_init_socket->b_transport(trans, delay);
    // }
    void fft2vcore_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
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
    // virtual bool pea2vcore_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
    //     return vcore2soc_init_socket->get_direct_mem_ptr(trans, dmi_data);
    // }
    virtual bool fft2vcore_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return vcore2soc_init_socket->get_direct_mem_ptr(trans, dmi_data);
    }

    ~VCore() {
        delete vpu;
        delete sm;
        delete am;
        delete spu;
        delete dma;
        // delete pea;
        delete fft_tlm;
    }
};

#endif
