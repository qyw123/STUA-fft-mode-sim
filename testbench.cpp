#include "./src/Soc.h"
#include "./FFT_initiator.h"
#include "./util/const.h"
using DataType = float;

SC_MODULE(Top){
    Soc<DataType>* soc;
    FFT_Initiator<DataType>* fft_initiator;
    SC_CTOR(Top){
        soc = new Soc<DataType>("soc");
        fft_initiator = new FFT_Initiator<DataType>("initiator");
        fft_initiator->socket.bind(soc->ext2soc_target_socket);
        soc->soc2ext_initiator_socket.bind(fft_initiator->soc2ext_target_socket);
    }
};

int sc_main(int argc, char* argv[])
{
    Top top("top");
    sc_start(sc_time(1000, SC_NS));  // Run for 10 seconds or until sc_stop() is called
    return 0;
}
