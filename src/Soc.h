#ifndef SOC_H
#define SOC_H

#include "DDR.h"
#include "GSM.h"
#include "VCore.h"
#include "CAC.h"
#include "../util/const.h"
#include "../util/tools.h"
template <typename T>
SC_MODULE(Soc) {
    CAC<T>* cac;
    DDR<T>* ddr;
    GSM<T>* gsm;
    VCore<T>* vcore;
    //外部socket
    tlm_utils::multi_passthrough_target_socket<Soc, 512> ext2soc_target_socket;
    tlm_utils::multi_passthrough_initiator_socket<Soc, 512> soc2ext_initiator_socket;
    //内部socket
    tlm_utils::multi_passthrough_initiator_socket<Soc, 512> soc2vcore_initiator_socket;
    tlm_utils::multi_passthrough_target_socket<Soc, 512> vcore2soc_target_socket;

    SC_CTOR(Soc) {
        ext2soc_target_socket.register_get_direct_mem_ptr(this, &Soc::ext2soc_get_direct_mem_ptr);
        ext2soc_target_socket.register_b_transport(this, &Soc::ext2soc_b_transport);
        vcore2soc_target_socket.register_b_transport(this, &Soc::vcore2soc_b_transport);
        vcore2soc_target_socket.register_get_direct_mem_ptr(this, &Soc::vcore2soc_get_direct_mem_ptr);

        soc2ext_initiator_socket.register_invalidate_direct_mem_ptr(this,&Soc::invalidate_direct_mem_ptr);
        soc2vcore_initiator_socket.register_invalidate_direct_mem_ptr(this,&Soc::invalidate_direct_mem_ptr);
        
        cac = new CAC<T>("CAC");
        ddr = new DDR<T>("DDR");
        gsm = new GSM<T>("GSM");
        vcore = new VCore<T>("VCore");

        soc2vcore_initiator_socket.bind(vcore->soc2vcore_target_socket);
        vcore->vcore2cac_init_socket.bind(cac->vcore2cac_target_socket);
        vcore->vcore2soc_init_socket.bind(vcore2soc_target_socket);
        cac->cac2ddr_initiator_socket.bind(ddr->cac2ddr_target_socket);
        cac->cac2gsm_initiator_socket.bind(gsm->cac2gsm_target_socket);
    }
    bool ext2soc_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return soc2vcore_initiator_socket->get_direct_mem_ptr(trans, dmi_data);
    }

    void ext2soc_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        soc2vcore_initiator_socket->b_transport(trans, delay);
    }
    bool vcore2soc_get_direct_mem_ptr(int ID, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        return soc2ext_initiator_socket->get_direct_mem_ptr(trans, dmi_data);
    }
    void vcore2soc_b_transport(int ID, tlm::tlm_generic_payload& trans, sc_time& delay) {
        soc2ext_initiator_socket->b_transport(trans, delay);
    }
    virtual void invalidate_direct_mem_ptr(int ID, sc_dt::uint64 start_range, sc_dt::uint64 end_range) {
        cout << "DMI invalidated. Range: " << hex << start_range << " - " << end_range << endl;
    }
    //析构函数
    ~Soc() {
        delete cac;
        delete ddr;
        delete gsm;
        delete vcore;
    }

};

#endif
