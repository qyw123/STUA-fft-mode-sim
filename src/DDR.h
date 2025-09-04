#ifndef DDR_H
#define DDR_H

#include "../util/const.h"  // 引入公用常量和头文件
#include "../util/tools.h"

template<typename T>
class DDR : public sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<DDR, 512> cac2ddr_target_socket;

    DDR(sc_module_name name) : sc_module(name), cac2ddr_target_socket("cac2ddr_target_socket") {
        cac2ddr_target_socket.register_b_transport(this, &DDR::b_transport);
        cac2ddr_target_socket.register_get_direct_mem_ptr(this, &DDR::get_direct_mem_ptr);

        mem = new T[DDR_SIZE / sizeof(T)];
        for (uint64_t i = 0; i < DDR_SIZE / sizeof(T); i++) {
            mem[i] = T();  // 初始化内存
        }
    }
    //阻塞传输方法
    virtual void b_transport(int id, tlm::tlm_generic_payload& trans, sc_time& delay) {
        uint64_t data_length = trans.get_data_length();
        //更新延时信息
        delay += DDR_LATENCY*calculate_clock_cycles(data_length, DDR_DATA_WIDTH);
        wait(delay);
    }
    //获取DMI指针
    virtual bool get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi_data) {
        dmi_data.set_start_address(DDR_BASE_ADDR);
        dmi_data.set_end_address(DDR_BASE_ADDR + DDR_SIZE - 1);
        dmi_data.set_dmi_ptr(reinterpret_cast<unsigned char*>(mem));
        dmi_data.set_read_latency(DDR_LATENCY);
        dmi_data.set_write_latency(DDR_LATENCY);
        dmi_data.allow_read_write();
        return true;
    }


    ~DDR() {
        delete[] mem;
    }

private:
    T* mem;
};

#endif
