# Compiler and Flags
CXX = g++
SYSTEMC_DIR = /root/systemC/systemc-2.3.4
CXXFLAGS = -I. -I$(SYSTEMC_DIR)/include -I../GEMM_SA/include -I../include -I../utils -O0
LDFLAGS = -L$(SYSTEMC_DIR)/lib-linux64 -Wl,-rpath=$(SYSTEMC_DIR)/lib-linux64
LIBS = -lsystemc -lm -lpthread

# 🎯 Essential Test Targets  
TESTS = test_fft_tlm

# Default target
all: $(TESTS)

# FFT TLM Test Sources
FFT_TLM_SOURCES = test_fft_tlm.cpp \
                  ../src/fft_tlm.cpp \
                  ../src/fft_multi_stage.cpp \
                  ../src/fft_shuffle_dyn.cpp \
                  ../src/pe_dual.cpp

# FFT TLM Test Headers
FFT_TLM_HEADERS = ../include/fft_tlm.h \
                  ../include/fft_multi_stage.h \
                  ../include/fft_shuffle_dyn.h \
                  ../include/pe_dual.h \
                  ../utils/complex_types.h \
                  ../utils/config.h

# Build FFT TLM Test
test_fft_tlm: $(FFT_TLM_SOURCES) $(FFT_TLM_HEADERS)
	@echo "=== Building FFT TLM Test ==="
	$(CXX) $(CXXFLAGS) -o $@ $(FFT_TLM_SOURCES) $(LDFLAGS) $(LIBS)
	@echo "FFT TLM Test built successfully"

# Clean
clean:
	rm -f $(TESTS) *.vcd wave/*.vcd *.o
	@echo "Cleaned build files"

# Help target
help:
	@echo "Available targets:"
	@echo "  all           - Build all tests"
	@echo "  test_fft_tlm  - Build FFT TLM test"
	@echo "  run_test_fft_tlm - Run FFT TLM test (4-point default)"
	@echo "  run_fft_4     - Run 4-point FFT test"
	@echo "  run_fft_8     - Run 8-point FFT test"
	@echo "  run_fft_16    - Run 16-point FFT test"
	@echo "  run_fft_32    - Run 32-point FFT test"
	@echo "  run_fft_64    - Run 64-point FFT test"
	@echo "  run_fft_all   - Run all FFT point tests"
	@echo "  run_bypass    - Run Bypass mode tests (16->8/4/2 points)"
	@echo "  clean         - Clean build files"
	@echo "  help          - Show this help"

# === Run Targets ===
run_%: %
	@echo "=== Running $< ==="
	./$<

# === Multi-Point FFT Test Targets ===
run_fft_4: test_fft_tlm
	@echo "=== Running 4-point FFT Test ==="
	./test_fft_tlm 4

run_fft_8: test_fft_tlm
	@echo "=== Running 8-point FFT Test ==="
	./test_fft_tlm 8

run_fft_16: test_fft_tlm
	@echo "=== Running 16-point FFT Test ==="
	./test_fft_tlm 16

run_fft_32: test_fft_tlm
	@echo "=== Running 32-point FFT Test ==="
	./test_fft_tlm 32

run_fft_64: test_fft_tlm
	@echo "=== Running 64-point FFT Test ==="
	./test_fft_tlm 64

run_bypass: test_fft_tlm
	@echo "=== Running Bypass Mode Tests ==="
	./test_fft_tlm bypass

run_fft_all: test_fft_tlm
	@echo "=== Running All FFT Point Tests ==="
	@echo "Testing 4-point FFT:"
	@./test_fft_tlm 4 | tee fft_4pt_results.log
	@echo "\n========================================\n"
	@echo "Testing 8-point FFT:"
	@./test_fft_tlm 8 | tee fft_8pt_results.log
	@echo "\n========================================\n"
	@echo "Testing 16-point FFT:"
	@./test_fft_tlm 16 | tee fft_16pt_results.log
	@echo "\n========================================\n"
	@echo "Testing 32-point FFT:"
	@./test_fft_tlm 32 | tee fft_32pt_results.log
	@echo "\n========================================\n"
	@echo "Testing 64-point FFT:"
	@./test_fft_tlm 64 | tee fft_64pt_results.log
	@echo "All FFT tests completed. Check *.log files for detailed results."


