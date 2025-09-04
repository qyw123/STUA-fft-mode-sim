# Compiler and Flags
CXX = g++
SYSTEMC_DIR = /root/systemC/systemc-2.3.4
CXXFLAGS = -I. -I$(SYSTEMC_DIR)/include -I../GEMM_SA/include -I../include -I../utils -O0
LDFLAGS = -L$(SYSTEMC_DIR)/lib-linux64 -Wl,-rpath=$(SYSTEMC_DIR)/lib-linux64
LIBS = -lsystemc -lm -lpthread

# 🎯 Essential Test Targets  
TESTS = pe_dual_core_test fft_shuffle_dyn_test fft_multi_stage_test fft_multi_stage_test_2_half fft_multi_stage_test_multi_frame in_buf_vec_fft_test out_buf_vec_fft_test pea_fft_test pea_fft_multi_frame FFT_TLM_test

# Default target
all: $(TESTS)

# 🔥 PE_DUAL Core Test (with dependency)
pe_dual_core_test: pe_dual_core_test.cpp ../src/pe_dual.cpp
	@echo "=== Building PE_DUAL Core Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🔄 FFT Shuffle Dynamic Test (with dependency)
fft_shuffle_dyn_test: fft_shuffle_dyn_test.cpp ../src/fft_shuffle_dyn.cpp ../src/pe_dual.cpp
	@echo "=== Building FFT Shuffle Dynamic Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🚀 Think Ultra Multi-Stage FFT Test (with dependencies)
fft_multi_stage_test: fft_multi_stage_test.cpp ../src/fft_multi_stage.cpp ../src/pe_dual.cpp ../src/fft_shuffle_dyn.cpp
	@echo "=== Building Think Ultra Multi-Stage FFT Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🔥 Dual 4-Point FFT Bypass Test (Stage0/Shuffle0 Bypass Mode)
fft_multi_stage_test_2_half: fft_multi_stage_test_2_half.cpp ../src/fft_multi_stage.cpp ../src/pe_dual.cpp ../src/fft_shuffle_dyn.cpp
	@echo "=== Building Dual 4-Point FFT Bypass Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🚀 Multi-Frame FFT Test (8-Point FFT Multi-Frame Testing)
fft_multi_stage_test_multi_frame: fft_multi_stage_test_multi_frame.cpp ../src/fft_multi_stage.cpp ../src/pe_dual.cpp ../src/fft_shuffle_dyn.cpp
	@echo "=== Building Multi-Frame FFT Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🔥 IN_BUF_VEC_FFT Buffer Test (Grouped FIFO Buffer Testing)
in_buf_vec_fft_test: in_buf_vec_fft_test.cpp ../src/in_buf_vec_fft.cpp
	@echo "=== Building IN_BUF_VEC_FFT Buffer Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🔥 OUT_BUF_VEC_FFT Buffer Test (Real/Imaginary Separation Buffer Testing)
out_buf_vec_fft_test: out_buf_vec_fft_test.cpp ../src/out_buf_vec_fft.cpp
	@echo "=== Building OUT_BUF_VEC_FFT Buffer Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🎯 PEA_FFT Complete Pipeline Test 
pea_fft_test: pea_fft_test.cpp ../src/pea_fft.cpp ../src/in_buf_vec_fft.cpp ../src/out_buf_vec_fft.cpp ../src/fft_multi_stage.cpp ../src/pe_dual.cpp ../src/fft_shuffle_dyn.cpp
	@echo "=== Building PEA_FFT Complete Pipeline Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)



# Clean
clean:
	rm -f $(TESTS) *.vcd wave/*.vcd

# NO Help target


# === Run Targets ===
run_%: %
	@echo "=== Running $< ==="
	./$<

# 🎯 Essential Test Suites
# PE_DUAL Core Test
test_pe_dual: pe_dual_core_test
	@echo "🔥 PE_DUAL Core Functionality Test"
	@echo "--- FFT/GEMM/Bypass Three-Core Verification ---"
	./pe_dual_core_test
	@echo "✅ PE_DUAL Test Complete"

# FFT Shuffle Dynamic Test
test_fft_shuffle: fft_shuffle_dyn_test
	@echo "🔄 FFT Shuffle Dynamic Test"
	@echo "--- Perfect Shuffle & Pipeline Timing Verification ---"
	./fft_shuffle_dyn_test
	@echo "✅ FFT Shuffle Test Complete"

# 8-Point FFT Multi-Stage Test
test_fft_multi_stage: fft_multi_stage_test
	@echo "🚀 8-Point FFT Multi-Stage Test"
	@echo "--- Using fft_test_utils.h: Standard DFT Reference Validation ---"
	./fft_multi_stage_test
	@echo "✅ 8-Point FFT Multi-Stage Test Complete"

# FFT Multi-Stage Test with MKL verification
test_fft_multi_stage_mkl: fft_multi_stage_test_mkl
	@echo "🚀 FFT Multi-Stage Test with MKL Verification"
	@echo "--- 8-Point FFT with Intel MKL Reference Validation ---"
	./fft_multi_stage_test_mkl
	@echo "✅ FFT Multi-Stage MKL Test Complete"

# Dual 4-Point FFT Bypass Test
test_fft_dual_4pt: fft_multi_stage_test_2_half
	@echo "🔥 Dual 4-Point FFT Bypass Test"
	@echo "--- Using fft_test_utils.h: Enhanced Testing with Standard DFT Reference ---"
	./fft_multi_stage_test_2_half
	@echo "✅ Dual 4-Point FFT Bypass Test Complete"

# Multi-Frame FFT Test
test_fft_multi_frame: fft_multi_stage_test_multi_frame
	@echo "🚀 Multi-Frame FFT Test"
	@echo "--- 8-Point FFT Multi-Frame Testing with Different Input Data ---"
	./fft_multi_stage_test_multi_frame
	@echo "✅ Multi-Frame FFT Test Complete"

# IN_BUF_VEC_FFT Buffer Test
test_in_buf_vec_fft: in_buf_vec_fft_test
	@echo "🔥 IN_BUF_VEC_FFT Buffer Test"
	@echo "--- Grouped FIFO Buffer Write/Read and Control Logic Verification ---"
	./in_buf_vec_fft_test
	@echo "✅ IN_BUF_VEC_FFT Buffer Test Complete"

# OUT_BUF_VEC_FFT Buffer Test
test_out_buf_vec_fft: out_buf_vec_fft_test
	@echo "🔥 OUT_BUF_VEC_FFT Buffer Test"
	@echo "--- Real/Imaginary Separation Buffer and Control Logic Verification ---"
	./out_buf_vec_fft_test
	@echo "✅ OUT_BUF_VEC_FFT Buffer Test Complete"

# PEA_FFT Complete Pipeline Test
test_pea_fft: pea_fft_test
	@echo "🎯 PEA_FFT Complete Pipeline Test"
	@echo "--- Input Buffer → FFT Processing → Output Buffer Integration Test ---"
	./pea_fft_test
	@echo "✅ PEA_FFT Pipeline Test Complete"

# 🎯 PEA_FFT Multi-Frame Complete Pipeline Test 
pea_fft_multi_frame: pea_fft_multi_frame.cpp ../src/pea_fft.cpp ../src/in_buf_vec_fft.cpp ../src/out_buf_vec_fft.cpp ../src/fft_multi_stage.cpp ../src/pe_dual.cpp ../src/fft_shuffle_dyn.cpp
	@echo "=== Building PEA_FFT Multi-Frame Complete Pipeline Test ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# 🚀 FFT_TLM Test (TLM-2.0 Transaction Level Testing)
FFT_TLM_test: FFT_TLM_test.cpp ../src/FFT_TLM.cpp ../src/pea_fft.cpp ../src/in_buf_vec_fft.cpp ../src/out_buf_vec_fft.cpp ../src/fft_multi_stage.cpp ../src/pe_dual.cpp ../src/fft_shuffle_dyn.cpp
	@echo "=== Building FFT_TLM Test (TLM-2.0 Transaction Level Testing) ==="
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# PEA_FFT Multi-Frame Complete Pipeline Test
test_pea_fft_multi_frame: pea_fft_multi_frame
	@echo "🎯 PEA_FFT Multi-Frame Complete Pipeline Test"
	@echo "--- Multi-Frame Input Buffer → FFT Processing → Output Buffer Integration Test ---"
	./pea_fft_multi_frame
	@echo "✅ PEA_FFT Multi-Frame Pipeline Test Complete"

# FFT_TLM Test (TLM-2.0 Transaction Level Testing)
test_fft_tlm: FFT_TLM_test
	@echo "🚀 FFT_TLM Test (TLM-2.0 Transaction Level Testing)"
	@echo "--- Multi-Frame TLM Transaction Interface and SC_THREAD Verification ---"
	./FFT_TLM_test
	@echo "✅ FFT_TLM Test Complete"

