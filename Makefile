# Makefile for building hello.cpp with SystemC

# Compiler
CXX = g++

# Directories
INCLUDE_DIR = /opt/systemc/include
LIB_DIR = /opt/systemc/lib

# Flags
DEBUG_FLAGS = -O0 -Wall -Wno-unused-variable -fsanitize=address,undefined -g -O0
CXXFLAGS = -std=c++17 -DSC_ALLOW_DEPRECATED_IEEE_API -I. -I$(INCLUDE_DIR) -Isrc -Isrc/vcore/FFT_SA/include -Isrc/vcore/FFT_SA/utils -Isrc/vcore/GEMM_SA/include #$(DEBUG_FLAGS)
LDFLAGS = -L. -L$(LIB_DIR) -Wl,-rpath=$(LIB_DIR)
LIBS = -lsystemc -lm

# Target and source files
TARGET = main
SRC = testbench.cpp FFT_initiator.cpp FFT_initiator_utils.cpp \
      src/vcore/FFT_SA/src/fft_multi_stage.cpp \
      src/vcore/FFT_SA/src/FFT_TLM.cpp \
      src/vcore/FFT_SA/src/pea_fft.cpp \
      src/vcore/FFT_SA/src/out_buf_vec_fft.cpp \
      src/vcore/FFT_SA/src/in_buf_vec_fft.cpp \
      src/vcore/FFT_SA/src/pe_dual.cpp \
      src/vcore/FFT_SA/src/fft_shuffle_dyn.cpp 

# Build target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Clean up
clean:
	rm -f $(TARGET)

run:
	./$(TARGET)
