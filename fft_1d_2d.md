# Cooley–Tukey FFT 二维分解（Markdown 版）

Cooley–Tukey FFT 算法的核心思想是将一维 FFT 分解为二维 FFT。输入与输出的索引被重映射为二维索引：
$$
\begin{aligned}
n &= N_1 n_2 + n_1 \quad (0\le n_1 < N_1,\; 0\le n_2 < N_2),\\
k &= N_2 k_1 + k_2 \quad (0\le k_1 < N_1,\; 0\le k_2 < N_2).
\end{aligned}\tag{14}
$$

其中 $N=N_1 N_2$，并且 $0\le n_1,k_1 < N_1,\;0\le n_2,k_2 < N_2$。  
输入矩阵：$x[n_1,n_2]$；输出矩阵：$X[k_1,k_2]$。

---

## 一维 DFT 公式
$$
X(k)=\sum_{n=0}^{N-1} x(n)\,e^{-j\frac{2\pi}{N}kn},\qquad k=0,1,2,\dots,N-1.
\tag{15}
$$

将一维索引替换为二维索引得到
$$
X[k_1,k_2]
= \sum_{n_1=0}^{N_1-1}\sum_{n_2=0}^{N_2-1}
x[n_1,n_2]\,
e^{-j\frac{2\pi}{N}\,(N_2 k_1 + k_2)(N_1 n_2 + n_1)}.
\tag{16}
$$

展开指数项中的乘法：
$$
e^{-j\frac{2\pi}{N}\,\big(N_1N_2 k_1 n_2 + N_2 k_1 n_1 + N_1 k_2 n_2 + k_2 n_1\big)}.
\tag{17}
$$

由 $N=N_1N_2$ 得
$$
e^{-j\frac{2\pi}{N}\,\big(N_1N_2 k_1 n_2 + N_2 k_1 n_1 + N_1 k_2 n_2 + k_2 n_1\big)}
= e^{-j2\pi\left(k_1 n_2 + \frac{k_1 n_1}{N_1} + \frac{k_2 n_2}{N_2} + \frac{k_2 n_1}{N}\right)}.
\tag{18}
$$

在上述指数中，$e^{-j2\pi k_1 n_2}=1$（因其为整周相位），故剩余项为
$$
e^{-j2\pi\left(\frac{k_1 n_1}{N_1} + \frac{k_2 n_2}{N_2} + \frac{k_2 n_1}{N}\right)}.
\tag{19}
$$

代回到求和式：
$$
X[k_1,k_2]
= \sum_{n_1=0}^{N_1-1}\sum_{n_2=0}^{N_2-1}
x[n_1,n_2]\,
e^{-j2\pi\left(\frac{k_1 n_1}{N_1} + \frac{k_2 n_2}{N_2} + \frac{k_2 n_1}{N}\right)}.
\tag{20}
$$

将三个因子拆分：
$$
X[k_1,k_2]
= \sum_{n_1=0}^{N_1-1}\sum_{n_2=0}^{N_2-1}
x[n_1,n_2]\,
e^{-j\frac{2\pi}{N_2}k_2 n_2}\,
e^{-j\frac{2\pi}{N}k_2 n_1}\,
e^{-j\frac{2\pi}{N_1}k_1 n_1}.
\tag{21}
$$

记旋转因子 $W_M \equiv e^{-j\frac{2\pi}{M}}$，其中 $e^{-j\frac{2\pi}{N}k_2 n_1}$ 为**补偿旋转因子**（twiddle）。于是
$$
X[k_1,k_2]
= \sum_{n_1=0}^{N_1-1}
\Bigg(
W_N^{k_2 n_1}
\sum_{n_2=0}^{N_2-1} x[n_1,n_2]\,W_{N_2}^{k_2 n_2}
\Bigg)
W_{N_1}^{k_1 n_1}.
\tag{22}
$$

---

## 三步 Cooley–Tukey 实现（由式 (22)）

- **列 FFT（长度 $N_2$）**：对每一列做 $N_2$ 点 FFT
  $$
  G(k_2;n_1)=\sum_{n_2=0}^{N_2-1} x[n_1,n_2]\,W_{N_2}^{k_2 n_2}.
  \tag{23}
  $$

- **补偿旋转因子**：修正交叉项引入的相位
  $$
  H(k_2;n_1)=W_N^{k_2 n_1}\,G(k_2;n_1).
  \tag{24}
  $$

- **行 FFT（长度 $N_1$）**：对每一行做 $N_1$ 点 FFT
  $$
  X[k_1,k_2]=\sum_{n_1=0}^{N_1-1} H(k_2;n_1)\,W_{N_1}^{k_1 n_1}.
  \tag{25}
  $$
