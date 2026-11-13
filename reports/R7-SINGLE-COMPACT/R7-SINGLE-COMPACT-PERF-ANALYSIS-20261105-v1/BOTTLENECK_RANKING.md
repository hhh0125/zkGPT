# 瓶颈排名（按证据层级）

| Full12 P阶段 | wall秒 | 解释 |
|---|---|---|
| protocol1-unified-input-sumcheck-and-opening | 2391.090000 | 阶段inclusive；只在此层排名 |
| native-Range-Exp-Lasso-Memory-Product | 1890.690000 | 阶段inclusive；只在此层排名 |
| frozen-witness-and-prechallenge-lookup | 628.766000 | 阶段inclusive；只在此层排名 |
| all-classified-roots-prechallenge-seal | 549.470000 | 阶段inclusive；只在此层排名 |
| all-native-GKR | 373.293000 | 阶段inclusive；只在此层排名 |
| all-source-original-handles | 92.997700 | 阶段inclusive；只在此层排名 |
| classified-public-Y-constants-and-input-definition | 0.038549 | 阶段inclusive；只在此层排名 |
| classified-physical-zero-tails | 0.000923 | 阶段inclusive；只在此层排名 |

Full12 V：Protocol1485.077 > Native431.892 > GKR53.5301 > source32.4829。阶段外110.486506另列，内部未知，不强塞给最高阶段。

Protocol1内部可比较的互斥P子区：U扫描652.3055；A3 core513.015；claim prepare358.118；K build355.983；U-K products200.387181；K fold129.443；guard65.3161；mapped fold prep63.682。它们处于不同父区域但不相互包含；其余plumbing、query生成、序列化、校验及残差另列，不将这些行当作完整process partition。V为prepare293.653、K kernel79.196865、A3 core33.8359、guard31.9587，其他终点等差额另列。

Native整体大于上述单函数，但子函数没有timer，暂不发布“Range最慢”或“Product占x%”。A3内部direct generator folding计数巨大，尚未测独占秒，列为高优先验证假设。MSM wall和memory bandwidth未测，不把它们列为已证实的主因。

Oneblock结构不同：Native658.724占P全程约62.68%，Protocol1200.716约19.10%；Oneblock无K回退，不能用其各项百分比线性外推Full12。
