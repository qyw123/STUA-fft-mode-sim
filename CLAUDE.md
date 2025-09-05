# CLAUDE.md
Always reponding in Chinese


This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STUA-fft-mode-sim is a SystemC-based FFT (Fast Fourier Transform) accelerator simulation platform using TLM-2.0. It models a System-on-Chip (SoC) architecture with specialized vector processing units for FFT computation.

## Build and Test Commands

### Main Build Commands
```bash
# Build the main simulation
make

# Run the main simulation (2000ns runtime)
make run
# or directly
./main

# Clean build artifacts
make clean
```

### FFT Systolic Array Testing
The FFT_SA module has comprehensive test suites in `src/vcore/FFT_SA/testbench/`:
```bash
cd src/vcore/FFT_SA/testbench/

# Build all test modules
make

# Run specific tests
make test_pe_dual           # PE_DUAL core functionality
make test_fft_shuffle       # FFT shuffle dynamic test
make test_fft_multi_stage   # 8-point FFT multi-stage test
make test_fft_dual_4pt      # Dual 4-point FFT bypass test
make test_fft_multi_frame   # Multi-frame FFT test
make test_pea_fft           # Complete PEA_FFT pipeline test
make test_fft_tlm           # TLM-2.0 transaction level test

# Clean test artifacts
make clean
```

## Architecture Overview

### System Hierarchy
- **Top** (`testbench.cpp`): SystemC simulation top-level containing SoC and FFT_Initiator
- **SoC** (`src/Soc.h`): System-on-Chip integrating VCore, DDR, GSM, and CAC components
- **VCore** (`src/VCore.h`): Vector processing core containing SPU, DMA, AM, SM, VPU, and FFT_TLM
- **FFT_Initiator** (`FFT_initiator.h/.cpp`): Test stimulus generator inheriting from BaseInitiatorModel

### Key Components
- **FFT_TLM** (`src/vcore/FFT_SA/include/FFT_TLM.h`): TLM wrapper for FFT accelerator
- **PEA_FFT** (`src/vcore/FFT_SA/include/pea_fft.h`): Systolic array FFT core implementation
- **PE_DUAL** (`src/vcore/FFT_SA/include/pe_dual.h`): Dual-function processing elements (FFT/GEMM/Bypass)
- **DMA** (`src/vcore/DMA.h`): Direct memory access controller
- **CAC** (`src/CAC.h`): Cache Coherent Agent
- **DDR/GSM** (`src/DDR.h`, `src/GSM.h`): Memory system components

### Configuration Files
- **Global constants**: `util/const.h` (FFT_TLM_N=16, TEST_FFT_SIZE=16)
- **FFT timing config**: `src/vcore/FFT_SA/utils/config.h` (operation cycles, pipeline delays)
- **Complex number types**: `src/vcore/FFT_SA/utils/complex_types.h`
- **Test utilities**: `src/vcore/FFT_SA/utils/fft_test_utils.h`

## Development Notes

### SystemC Dependencies
- Requires SystemC library installed (default path: `/opt/systemc/`)
- Main Makefile points to `/opt/systemc/` - update INCLUDE_DIR and LIB_DIR as needed
- FFT testbench uses `/root/systemC/systemc-2.3.4` - update SYSTEMC_DIR in testbench Makefile
- Uses C++17 standard with `-DSC_ALLOW_DEPRECATED_IEEE_API` flag

### FFT Processing Pipeline
1. Input buffer (`in_buf_vec_fft`) - grouped FIFO with write/read control
2. Multi-stage FFT (`fft_multi_stage`) - butterfly operations with PE_DUAL cores
3. Shuffle operations (`fft_shuffle_dyn`) - perfect shuffle with pipeline timing
4. Output buffer (`out_buf_vec_fft`) - real/imaginary separation buffer

### TLM Communication
- Uses TLM-2.0 b_transport interface
- Multi-passthrough sockets for component interconnection
- Transaction routing through SoC between VCore, DDR, GSM via CAC
- FFT operations controlled via TLM transactions to FFT_TLM module

### Test Data Flow
FFT_Initiator → SoC → VCore → FFT_TLM → PEA_FFT → results back through same path
- Supports multi-frame processing (configurable frame count)
- Automatic data generation, computation, and verification
- Event-driven control using SystemC events