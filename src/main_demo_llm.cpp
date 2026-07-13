
今天 11:15
重新搜索一下，现在公开了

今天 15:22
现在更新了代码，https://github.com/hhh0125/zkGPT/tree/main/src，重新扫描一下文件，如果要实现zkGPT还有哪些不足

思考了 1m 1s
我重新扫描了最新提交 916bc31a。与上一版相比，代码有两个实质进展：

已经恢复 verifier v(&p, p.C) 和 v.prove(32)，GKR、Lasso 和承诺开口会真正执行。

readFconWeight() 已经能够从 fc_0.bin～fc_47.bin 加载量化后的 GPT‑2 FC 权重，不再默认使用随机权重。

但如果目标是“证明指定 GPT‑2 对指定文本生成了指定输出”，目前仍有较大缺口。最准确的定位仍然是：

当前实现是“使用真实部分权重的 GPT 核心算子证明原型”，还不是完整 zkGPT 推理系统。

一、最新版本已改进的地方
1. 主证明流程恢复了
main_demo_llm.cpp 现在执行：

prover p;
LLM nn(...);
nn.create(p, 1);
verifier v(&p, p.C);
v.prove(32);
而 v.prove() 会继续调用：

verifyGKR();
verifyLasso();
openCommit();
因此上一版“只构建电路、不真正运行验证器”的问题已经解决。

2. FC 权重开始使用真实文件
readFconWeight() 现在读取：

data/gpt2_int/fc_0.bin
...
data/gpt2_int/fc_47.bin
并且检查：

输出维度；

输入维度；

文件头；

权重数据是否完整。

这比上一版 rand()%1024 明显前进了一步。

不过，真实权重只覆盖了 48 个 FC 矩阵，GPT‑2 的其他参数还没有加载。

二、当前最严重的问题：范围证明被关闭，而且没有绑定真实 witness
最新主程序中，范围证明整体被注释：

// range_prover.init();
// range_prover.build();
// double range_prover_time = range_prover.prove();
这不是单纯少统计一段时间，而会影响非线性算子的可靠性。

当前以下正确性都依赖“某些有限域元素是非负小整数”：

LayerNorm 的平方根与舍入；

FC 后的定点舍入；

Softmax 最大值；

Softmax 查表索引；

Softmax 除法舍入；

GELU 的区间判断和分段选择。

例如 roundLayer() 中：

assert(!val[0][s].isNegative());
Softmax 中：

assert(!val[layer_id][i].isNegative());
这些只是证明者本地执行的 C++ 断言，验证者无法通过 GKR 自动确认“非负”。

更重要的是，即使重新打开当前 range_prover，它证明的仍然是随机数据：

constraint.prepare();  // 生成随机数
range_prove(constraint.inputs, ...);
所以现在的情况是：

GKR/Lasso 主证明已经恢复；

但真正需要范围检查的 witness 没有进入范围证明；

范围证明与 val[0] 的 commitment 也没有建立一致性。

这是当前密码学完整性上的第一优先级问题。

应该怎么修改
需要将实际产生的这些值收集起来：

LayerNorm:
sigma相关区间项、delta1、delta2

Round:
term1、term2

Softmax:
pmax-pj、查表索引t、除法舍入term1/term2

GELU:
符号、分段条件、舍入区间
然后：

对这些真实值做范围证明；

使用与 GKR 输入一致的 commitment；

证明范围证明中的值就是 GKR 使用的那些 val[0] 位置；

不能只证明另一份复制出来的数组。

三、输入现在变成了随机隐藏向量
上一版从文件读取 30×768 隐藏向量；新版在 calcInputLayer() 中直接生成：

num = rand() / RAND_MAX * ...;
默认范围由：

ZKGPT_RANDOM_INPUT_ABS=350
控制。

这说明当前输入甚至不是一个真实 GPT‑2 prompt 对应的 embedding，而是一组随机隐藏状态。

因此当前证明的语句近似是：

对随机生成的隐藏矩阵，执行后续近似 Transformer 算子得到的中间结果是正确的。

而不是：

对用户输入的文本/token，指定 GPT‑2 得到了这个输出。

仍需补充
完整输入链应该是：

文本
→ tokenizer
→ token IDs
→ token embedding
→ position embedding
→ 初始隐藏状态
Tokenizer 不一定必须在 ZK 电路里，但至少要做到：

token IDs 是公共输入或被 commitment 绑定；

token ID 范围为 [0, vocab_size)；

embedding lookup 正确；

position ID 正确；

token embedding 与 position embedding 正确相加；

得到的隐藏状态与第一个 Transformer block 相连。

否则证明者可以自由选择任意隐藏向量。

四、仍然缺少残差连接
标准 GPT‑2 block 的核心结构是：

u
=
x
+
Attention
⁡
(
LN
⁡
1
(
x
)
)
u=x+Attention(LN 
1
​
 (x))
y
=
u
+
MLP
⁡
(
LN
⁡
2
(
u
)
)
y=u+MLP(LN 
2
​
 (u))
当前 create() 的流程是依靠 q_offset 顺序传递：

LN → FC → Round → Attention → FC → Round
→ LN → FC → Round → GELU → FC → Round
代码中没有发现明确保存 block 输入并与 Attention 输出相加，也没有发现 MLP 输出与 Attention 残差结果相加。

这意味着当前实现更接近：

u
=
Attention
⁡
(
LN
⁡
1
(
x
)
)
u=Attention(LN 
1
​
 (x))
y
=
MLP
⁡
(
LN
⁡
2
(
u
)
)
y=MLP(LN 
2
​
 (u))
缺少残差后，网络已经不是 GPT‑2。即使加载真实 GPT‑2 权重，也无法与 Hugging Face GPT‑2 对齐。

这是模型语义完整性上的第一优先级问题。

五、FC bias 仍然没有使用
虽然代码中保留了 readBias()，但参数分配仍被注释：

// fc.bias_start_id = pos;
// pos += channel_out;
fullyConnLayer() 中 bias 加法也被注释：

// our protocol doesn't support adding bias for simplicity
GPT‑2 的四类线性层都带 bias：

attn.c_attn.bias；

attn.c_proj.bias；

mlp.c_fc.bias；

mlp.c_proj.bias。

因此虽然当前已经加载真实 FC 权重，但得到的仍不是该 checkpoint 的真实结果。

六、LayerNorm 仍未加载真实参数
read_layer_norm() 仍然把所有参数设置成：

weight = 1;
bias = 1;
而实际 GPT‑2 每个 block 有两个不同的 LayerNorm：

h.0.ln_1
h.0.ln_2
...
h.11.ln_1
h.11.ln_2
最后还有：

transformer.ln_f
这些 LayerNorm 分别拥有自己的：

weight/
γ
γ；

bias/
β
β。

当前代码循环中一直调用：

read_layer_norm(0);
这会反复覆盖 ln_id=0 的参数位置，并没有按 24 个 block LayerNorm 分别管理参数。

代码中仍保留：

// TODO we need to fix all layer_norm value's read
// TODO: s1 is wrong
因此 LayerNorm 还没有达到真实 GPT‑2 级别。

七、模型维度可能与 GPT‑2 small 不匹配
主程序仍然构造：

LLM nn(12, 12, 64, 768, 2304);
也就是 FFN 中间维度为 2304。

标准 GPT‑2 small 的典型配置是：

n_layer = 12
n_head  = 12
n_embd  = 768
n_inner = 3072
如果导出的 fc_24 等 MLP 权重来自标准 GPT‑2，那么文件形状应包含：

3072 × 768
768 × 3072
但 C++ 当前期望的是 2304 维。新版增加了严格形状检查，因此很可能直接报：

GPT-2 FC weight shape mismatch
除非导出的模型确实是将 MLP 裁剪为 2304 的特殊 GPT‑2。

因此需要统一以下三者：

checkpoint 配置；

导出脚本；

LLM 构造参数。

如果目标是标准 gpt2，这里应重点检查是否应改成 3072。

八、当前只加载了 FC 权重，并没有加载完整模型
现在加载的 48 个 FC 矩阵对应每层：

attn.c_attn
attn.c_proj
mlp.c_fc
mlp.c_proj
但是完整 GPT‑2 还需要：

参数	当前状态
token embedding wte	缺失
position embedding wpe	缺失
24组 block LayerNorm 参数	缺失
final LayerNorm ln_f	缺失
48组 FC bias	缺失
LM Head	缺失
embedding/LM Head 权重共享	未处理
所以“加载真实 FC 权重”不能等同于“加载真实 GPT‑2”。

九、缺少最终 LayerNorm 和 LM Head
12 个 block 结束后，标准 GPT‑2 还要计算：

h
f
=
LN
⁡
f
(
h
12
)
h 
f
​
 =LN 
f
​
 (h 
12
​
 )
logits
=
h
f
W
v
o
c
a
b
⊤
logits=h 
f
​
 W 
vocab
⊤
​
 
当前代码没有：

final LayerNorm；

768→50257 的 LM Head；

vocabulary logits；

公开输出 token。

因此当前证明没有真正的 GPT 输出。它证明到最后一个 Transformer block 的某个量化隐藏状态就结束了。

十、没有证明 argmax 或采样
如果使用贪心解码，需要证明：

y
=
arg
⁡
max
⁡
v
∈
[
0
,
50257
)
logits
[
v
]
y=arg 
v∈[0,50257)
max
​
 logits[v]
如果使用采样，还要处理：

temperature；

top-k；

top-p；

随机数；

抽样规则。

当前没有 LM Head，自然也没有这些输出选择约束。即使未来只把 logits 输出到电路外部，验证者也只能确认 logits，而不能确认服务端返回的 token 是正确选择出来的。

建议第一版先做 greedy argmax，不要直接做 top-p 采样。

十一、没有完整自回归生成
当前只构造一次固定长度前向传播，序列长度固定为 30。真实 GPT 生成需要：

prompt → token₁
prompt+token₁ → token₂
prompt+token₁+token₂ → token₃
...
当前缺少：

生成 token 回接下一步输入；

相邻证明之间的状态绑定；

KV cache；

KV cache 正确更新；

EOS；

生成长度；

多 token 输出的整体声明。

因此，即使补齐 LM Head，第一阶段也只能称为：

单步 next-token inference proof。

还不能称为完整回答生成证明。

十二、Softmax 查表存在未证明的越界降级
新版 Softmax 对查表索引采用：

int exp_value = tj < EXP_TABLE_SIZE ? table[tj] : 1;
也就是说，当索引超过表范围时，直接令指数结果为 1。

这比崩溃更容易运行，但会带来两个问题：

tj >= EXP_TABLE_SIZE 没有被证明为异常；

超界结果被静默替换为 1，可能显著改变 Softmax。

正确做法应当是：

范围证明 
0
≤
t
j
<
EXP_TABLE_SIZE
0≤t 
j
​
 <EXP_TABLE_SIZE；

lookup proof 证明 
E
j
=
table
[
t
j
]
E 
j
​
 =table[t 
j
​
 ]；

如果希望对大索引饱和，应把“饱和 Softmax”明确定义为模型算法，并证明分支条件。

不能只在 witness 生成代码中写一个条件表达式。

十三、Softmax、GELU 和 LayerNorm 都是近似版本
这本身不一定是错误，因为 zkML 通常必须采用定点近似。但需要明确证明目标：

验证量化 zkGPT 模型，而不是逐位复现 PyTorch 浮点 GPT‑2。

目前还缺少完整的数值一致性测试：

每层最大绝对误差；

每层平均误差；

logits 排名一致率；

next-token 一致率；

不同 prompt 下累计误差；

多 token 生成分叉率；

量化前后 perplexity；

Softmax/GELU lookup 误差界。

尤其在残差、bias、真实 LayerNorm 参数都未加入之前，暂时没有必要先讨论很小的近似误差，因为当前结构误差远大于量化误差。

十四、公开声明尚未定义清楚
一个真正的 zkGPT 证明应明确表达：

∃
W
,
 witness
:
Commit
⁡
(
W
)
=
C
W
∧
GPT
⁡
(
W
,
x
)
=
y
∃W, witness:Commit(W)=C 
W
​
 ∧GPT(W,x)=y
其中：

C
W
C 
W
​
 ：指定模型的公开 commitment；

x
x：用户的 prompt/token IDs；

y
y：模型输出 token；

W
W：私有或公开模型权重；

中间激活和辅助值：私有 witness。

当前代码虽然对整个 val[0] 调用了 commitInput()，但 val[0] 混合了：

随机输入隐藏状态；

模型 FC 权重；

LayerNorm 假参数；

Round 辅助值；

GELU 辅助值；

Softmax 辅助值。

目前没有独立、稳定的“模型 commitment”，也没有公开的 input/output statement。这意味着验证者仍不能清楚回答：

验证的是哪个 GPT‑2 checkpoint？

输入是哪段文本？

输出是哪一个 token？

同一模型不同请求是否使用了相同模型参数？

十五、代码的可配置性仍较弱
以下参数仍然硬编码：

const int HEAD = 12;
const int HSIZE = 64;
len = 30;
hidden = 768;
虽然构造函数接收 headnum、headdim，注意力和 Softmax 内部仍直接写死 12 和 64。

这会导致：

构造参数与内部计算不一致；

无法支持 GPT‑2 medium/large；

难以测试小模型；

修改序列长度容易引发 offset 错误。

建议所有算子统一使用成员变量：

headnum
headdim
attn_dim
linear_dim
len
并增加维度断言。

最新版完成度总结
功能	当前状态
GKR/Lasso 主验证器	已恢复
输入 commitment/opening	已执行
真实 FC 权重加载	已实现
因果 QK 注意力	已实现
近似 Softmax×V	已实现
近似 GELU	已实现
近似 LayerNorm	算子存在，但参数和公式仍有 TODO
真实 token 输入	未实现，当前为随机隐藏向量
token/position embedding	未实现
FC bias	未实现
残差连接	未实现
真实 LayerNorm 参数	未实现
final LayerNorm	未实现
LM Head	未实现
argmax/采样	未实现
范围证明	主程序已关闭
范围证明绑定真实 witness	未实现
单步 next-token 完整证明	未实现
多 token 自回归证明	未实现
推荐实施优先级
P0：先保证证明在密码学上闭环
将范围证明连接到真实 witness。

与 GKR 的 val[0] commitment 建立一致性。

为 Softmax lookup 建立完整的索引范围和查表证明。

增加恶意 witness 测试：修改任一辅助值后验证必须失败。

明确定义公共输入、模型 commitment 和公共输出。

P1：还原一个真实 GPT‑2 block
修正 FFN 维度 2304/3072 的一致性。

加入 FC bias。

加入两次残差连接。

加载每层真实 LayerNorm 参数。

与 PyTorch 量化参考实现逐层比对。

P2：完成单步 GPT 推理
token embedding；

position embedding；

final LayerNorm；

LM Head；

logits；

greedy argmax；

证明公开 token IDs 得到公开 next token。

P3：扩展为完整文本生成
多步生成；

KV cache；

相邻步骤状态绑定；

EOS；

再考虑 top-k/top-p 和随机采样。

所以，与上一版相比，这次更新确实迈过了“真实 FC 权重”和“实际运行 verifier”两道门槛；但目前最急迫的并不是继续优化 MSM，而是先解决范围证明未绑定、残差缺失、bias 缺失和输入输出链缺失。否则 MSM 即使大幅加速，最终加速的仍不是一个语义完整的 GPT‑2 推理证明。



### 0：先保证证明在密码学上闭环 1. 将范围证明连接到真实 witness。 2. 与 GKR 的 `val[0]` commitment 建立一致性。 3. 为 Softmax lookup 建立完整的索引范围和查表证明。 4. 增加恶意 witness 测试：修改任一辅助值后验证必须失败。 5. 明确定义公共输入、模型 commitment 和公共输出。

给出修改代码

可以修改，但上述不足涉及多个独立阶段，无法一次性安全补齐。你希望我先实现哪一部分？

完整 Transformer Block（推荐）：加入真实 FC bias、真实 LayerNorm 参数、两次残差连接，并修正 FFN 维度。这是当前最关键的模型语义修复。

证明安全闭环：让 range proof 使用 LayerNorm、Softmax、GELU、Round 的真实 witness，并与 GKR commitment 绑定。

完整 GPT 输入输出：实现 token/position embedding、final LayerNorm、LM Head 和 argmax。

先做最小可运行补丁：读取真实隐藏状态和权重，修复 2304/3072 维度、错误检查与配置硬编码。

建议先选第 1 项，完成后用 PyTorch 量化参考实现逐层对齐，再处理范围证明。



20.zkGPT.pdf
PDF
这是对应的文档，参考他文章的内容，补全第一部分吧

思考了 6m 19s
已参考论文补全第一部分“完整 Transformer Block”，代码位于：

neuralNetwork.cpp

neuralNetwork.hpp

models.cpp

main_demo_llm.cpp

export_gpt2_complete_params.py

README.md

主要修改如下：

将 GPT‑2 small 的 MLP 维度修正为 3072。

将 QKV 输出维度单独设为 
3
×
768
=
2304
3×768=2304，不再与 MLP 维度混用。

加入 Attention 后和 MLP 后的两次残差连接。

残差结果作为 advice 放入输入层，并增加：

y
−
x
−
r
e
s
i
d
u
a
l
=
0
y−x−residual=0
的电路约束。

统一 block 内激活量化尺度，使残差可以在整数域直接相加。

增加 48 组真实 FC bias 的读取。

将 bias 融合到 FC 后的舍入约束中，不会只在 witness 计算中加入。

加载每层独立的 24 组 LayerNorm weight/bias，不再全部使用常数 1。

参数文件缺失或维度错误时直接报错，不再悄悄使用假参数。

暂时关闭 circuit squeeze，避免新增残差层被旧的层合并逻辑错误处理。

参数导出方式：

python scripts/export_gpt2_complete_params.py \
    --model gpt2 \
    --output data/gpt2_int

export ZKGPT_PARAMETER_DIR=data/gpt2_int
./llm.sh
导出脚本会生成：

fc_0.bin ... fc_47.bin
fc_0_bias.bin ... fc_47_bias.bin
ln_0.bin ... ln_23.bin
已经完成：

C++17 语法检查；

Python 导出脚本语法检查；

git diff --check。

当前环境没有安装 cmake 和完整 cnpy 构建依赖，因此没有完成链接和全规模运行。还有一点需要注意：论文没有提供每层的最终量化 scale manifest，目前代码沿用了仓库的统一激活 scale。下一步应使用 PyTorch 量化参考程序逐层对齐，并根据真实统计确定每层 scale；之后再重新适配 circuit squeeze。


main_demo_llm.cpp



#undef NDEBUG
#include "circuit.h"
#include "neuralNetwork.hpp"
#include "verifier.hpp"
#include "models.hpp"
#include "global_var.hpp"
#include <iostream>

#include "range_prover.hpp"
#include "hyrax_rp.hpp"
#include "stats.hpp"
using namespace mcl::bn;
using namespace std;




int main(int argc, char **argv) 
{
    initPairing(mcl::BN254);
    
    // range prover
    printf("Start range proving...\n");
    // range_prover range_prover(12, 12, 64, 768, 2304, 30, 32, 1); // 12 layer, 12 head, 64 channel, 768 head dim, 2304 linear dim, 30 seq len, 32 threads
    // range_prover.init();
    // range_prover.build();
    // // 证明者运行，证明输入值的范围
    // double range_prover_time = range_prover.prove();


    // gkr
    prover p;
    // GPT-2 small: 12 blocks, 12 heads, hidden size 768 and MLP size 3072.
    LLM nn(12, 12, 64, 768, 3072);
    // Keep the unsqueezed circuit until residual layers are included in the
    // squeeze scheduler. This preserves the exact GPT-2 data dependencies.
    nn.create(p, 0);
    verifier v(&p, p.C);
    // v.range_prove(range_prover_time);
    v.prove(32); // prove with 32 threads
    stats::print_stats();

}

main_demo_llm.cpp



#undef NDEBUG
#include "circuit.h"
#include "neuralNetwork.hpp"
#include "verifier.hpp"
#include "models.hpp"
#include "global_var.hpp"
#include <iostream>

#include "range_prover.hpp"
#include "hyrax_rp.hpp"
#include "stats.hpp"
using namespace mcl::bn;
using namespace std;




int main(int argc, char **argv) 
{
    initPairing(mcl::BN254);
    
    // range prover
    printf("Start range proving...\n");
    range_prover range_prover(12, 12, 64, 768, 2304, 30, 32, 1); // 12 layer, 12 head, 64 channel, 768 head dim, 2304 linear dim, 30 seq len, 32 threads
    range_prover.init();
    range_prover.build();
    // 证明者运行，证明输入值的范围
    double range_prover_time = range_prover.prove();


    // gkr
    prover p;
    // GPT-2 small: 12 blocks, 12 heads, hidden size 768 and MLP size 3072.
    LLM nn(12, 12, 64, 768, 3072);
    // Keep the unsqueezed circuit until residual layers are included in the
    // squeeze scheduler. This preserves the exact GPT-2 data dependencies.
    nn.create(p, 0);
    verifier v(&p, p.C);
    // v.range_prove(range_prover_time);
    v.prove(32); // prove with 32 threads
    stats::print_stats();

}
