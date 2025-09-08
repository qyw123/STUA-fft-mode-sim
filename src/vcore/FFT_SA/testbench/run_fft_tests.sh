#!/bin/bash
# FFT TLM测试运行脚本
# 用法: ./run_fft_tests.sh [点数] 或 ./run_fft_tests.sh all

echo "=========================================="
echo "         FFT TLM2.0 测试套件"
echo "=========================================="

# 检查是否编译了测试程序
if [ ! -f "test_fft_tlm" ]; then
    echo "⚠️  测试程序不存在，正在编译..."
    make test_fft_tlm
    if [ $? -ne 0 ]; then
        echo "❌ 编译失败，退出"
        exit 1
    fi
    echo "✅ 编译完成"
fi

# 创建结果目录
mkdir -p results

# 函数：运行单个FFT测试
run_fft_test() {
    local points=$1
    echo ""
    echo "🔍 运行${points}点FFT测试..."
    echo "=========================================="
    
    ./test_fft_tlm $points | tee results/fft_${points}pt_results.log
    
    # 检查测试结果
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        echo "✅ ${points}点FFT测试完成"
    else
        echo "❌ ${points}点FFT测试失败"
    fi
}

# 主逻辑
if [ $# -eq 0 ]; then
    echo "使用默认4点FFT测试"
    run_fft_test 4
elif [ "$1" = "bypass" ]; then
    echo "🔄 运行Bypass模式测试 (16点硬件 -> 8/4/2点FFT)..."
    echo "=========================================="
    
    ./test_fft_tlm bypass | tee results/fft_bypass_results.log
    
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        echo "✅ Bypass模式测试完成"
    else
        echo "❌ Bypass模式测试失败"
    fi
elif [ "$1" = "all" ]; then
    echo "🚀 运行所有FFT点数测试..."
    
    for points in 4 8 16 32 64; do
        run_fft_test $points
        echo ""
    done
    
    echo "=========================================="
    echo "📊 测试总结"
    echo "=========================================="
    echo "所有测试日志已保存到 results/ 目录"
    ls -la results/fft_*pt_results.log
    
    echo ""
    echo "📈 快速结果概览:"
    for points in 4 8 16 32 64; do
        if grep -q "通过" results/fft_${points}pt_results.log 2>/dev/null; then
            passed=$(grep -c "通过" results/fft_${points}pt_results.log)
            failed=$(grep -c "失败" results/fft_${points}pt_results.log)
            echo "  ${points}点FFT: ✅ ${passed}个测试通过, ❌ ${failed}个测试失败"
        else
            echo "  ${points}点FFT: ❓ 结果未知或测试未运行"
        fi
    done
    
elif [[ "$1" =~ ^[0-9]+$ ]]; then
    # 检查点数是否支持
    if [[ "$1" == "4" || "$1" == "8" || "$1" == "16" || "$1" == "32" || "$1" == "64" ]]; then
        run_fft_test $1
    else
        echo "❌ 不支持的FFT点数: $1"
        echo "支持的点数: 4, 8, 16, 32, 64"
        echo "或者使用 'all' 运行所有测试, 'bypass' 运行Bypass模式测试"
        exit 1
    fi
else
    echo "❓ 用法错误"
    echo "用法: $0 [点数|all|bypass]"
    echo "  点数: 4, 8, 16, 32, 64"
    echo "  all : 运行所有点数测试"
    echo "  bypass : 运行Bypass模式测试 (16点硬件实现小点数FFT)"
    echo ""
    echo "示例:"
    echo "  $0        # 运行默认4点测试"
    echo "  $0 8      # 运行8点测试"
    echo "  $0 all    # 运行所有测试"
    echo "  $0 bypass # 运行Bypass模式测试"
    exit 1
fi

echo ""
echo "=========================================="
echo "🎯 FFT TLM2.0 测试完成"
echo "=========================================="