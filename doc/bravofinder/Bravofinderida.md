# BravoFinder v3 航路查找引擎：实现要点与数学推理

> **摘要** — BravoFinder 是一个面向真实航空运行的航路查找引擎，其 v3 版本从"几何最短路径"全面转向"合规航路生成"。本文系统梳理该引擎的实现架构与数学基础，涵盖空域图建模、A* 启发式搜索与 K-最短路径算法族、可插拔约束框架、终端程序建模与航路网衔接、地形安全双层模型、航路串压缩、跨平台二进制缓存、无锁并发查询及系统化性能工程。全文以定理—证明—实现三段式组织，力求严谨。

---

## 目录

1. [引言](#1-引言)
2. [空域图建模](#2-空域图建模)
3. [核心路由算法](#3-核心路由算法)
   - 3.1 A* 搜索与大圆距离启发式
   - 3.2 Yen K-最短路径算法
   - 3.3 Lawler 优化
   - 3.4 多源多汇扩展
4. [合规约束框架](#4-合规约束框架)
5. [终端程序建模与航路网衔接](#5-终端程序建模与航路网衔接)
6. [地形安全模型](#6-地形安全模型)
7. [航路串压缩](#7-航路串压缩)
8. [性能工程](#8-性能工程)
   - 8.1 启发式记忆化
   - 8.2 搜索工作区复用
   - 8.3 内存布局优化
   - 8.4 性能分解表
9. [二进制缓存格式](#9-二进制缓存格式)
10. [线程安全模型](#10-线程安全模型)
11. [HTTP 服务体系](#11-http-服务体系)
12. [性能评估](#12-性能评估)
13. [结论](#13-结论)
14. [参考文献](#14-参考文献)

---

## 1. 引言

航路查找问题在形式上可归约为带约束的图上 K-最短路径问题，但真实航空运行引入了图论教科书中少有关注的复杂性：航路方向性、高度层限制、最低安全高度约束、终端程序衔接、平行航路合并等。BravoFinder v3 的核心命题是：**如何在 27 万顶点、34 万边的有向约束图中，以毫秒级延迟返回 top-K 条合规航路候选。**

v1/v2 版本以 Dijkstra 算法求地理最短路径，忽视了航路方向、高度带等运行约束，产出的"最短线"往往不可飞。v2 的 `static` 局部变量跨实例共享状态也导致了多数据集场景下的隐蔽正确性缺陷。v3 以四条设计宪法重写全部核心：

1. **禁用 `static` / 全局可变状态**——每个 `NavDatabase` 实例完全自包含；
2. **禁用裸 `new`/`delete`、`goto`、按值捕获异常**——所有权采用 `unique_ptr` / 值类型；
3. **领域类型为不可变值类型**——无隐藏指针，无生命周期依赖；
4. **预期失败使用 `Result<T,E>`，异常保留给真正异常的场景。**

---

## 2. 空域图建模

### 2.1 图的基本定义

航路网络被建模为有向图 $G = (V, E)$，其中：

- **顶点 $V$**：导航台（VOR/DME/NDB）、航路点（waypoint），共约 270,821 个；
- **边 $E \subseteq V \times V$**：连接相邻两点的航路段，共约 345,801 条；
- **边权重 $w: E \to \mathbb{R}^+$**：大圆距离（nautical miles）。

### 2.2 CSR 存储格式

BravoFinder 采用 **CSR (Compressed Sparse Row)** 格式（`nav_graph.h`）存储图拓扑：

- 所有边存放在单一连续数组 `edges_` 中；
- 每个顶点记录其在边数组中的索引区间 `[begin, end)`；
- 顶点坐标、标识符等属性按顶点索引平行存储。

**设计理由：**
1. **内存连续性**：边遍历时缓存局部性最大化，A* 松弛操作的 cache miss 率最低；
2. **文件序列化友好**：连续内存块可直接映射为二进制缓存段；
3. **构建后不可变**：与只读查询语义一致，无需为并发写付出同步代价。

### 2.3 有向性编码

航路的方向性在**图构建阶段**编码，而非查询时检查。X-Plane `earth_awy.dat` 格式定义三种方向类型：

| 代码 | 语义 | 图构建动作 |
|------|------|-----------|
| `N` | 双向 | 创建 from→to 和 to→from 两条边 |
| `F` | 仅正向 | 仅创建 from→to 边 |
| `B` | 仅反向 | 仅创建 to→from 边 |

这意味着**逆向边在图中根本不存在**，搜索算法天然不可能穿越单向航路——零运行时开销的硬约束。

---

## 3. 核心路由算法

### 3.1 A* 搜索与大圆距离启发式

#### 3.1.1 算法原理

A* 在 Dijkstra 的基础上引入启发式函数 $h(v)$，估计顶点 $v$ 到目标的最小剩余代价。扩展时按 $f(v) = g(v) + h(v)$ 排序优先队列，其中 $g(v)$ 为已行驶距离。

#### 3.1.2 启发式选择

BravoFinder 采用**大圆距离**（great-circle distance）作为启发式：

$$h(v) = d_{\text{gc}}\big(\text{coord}(v), \text{coord}(goal)\big)$$

使用 Haversine 公式在球面模型上计算：

$$a = \sin^2\left(\frac{\Delta\phi}{2}\right) + \cos(\phi_1) \cos(\phi_2) \sin^2\left(\frac{\Delta\lambda}{2}\right)$$

$$c = 2 \cdot \text{atan2}\left(\sqrt{a}, \sqrt{1-a}\right)$$

$$d = R \cdot c$$

其中 $R$ 为地球半径，距离以海里为单位输出。

#### 3.1.3 可采纳性证明

**定理 1（可采纳性）** 大圆距离启发式 $h(v)$ 是 **可采纳的**（admissible），即对于任意顶点 $v$ 和目标 $t$，有 $h(v) \leq d^*(v, t)$，其中 $d^*(v, t)$ 为 $v$ 到 $t$ 的真实最短路径长度。

**证明** 大圆距离是地球表面两点间的最短测地线。沿实际航路飞行只能在此之上增加距离（绕行、折线），不可能短于大圆距离。因此 $h(v) \leq d^*(v, t)$ 恒成立。 $\square$

**推论 1（最优性保证）** 可采纳启发式保证 A* 找到的解为真实最短路径。

#### 3.1.4 软代价与启发式一致性

约束框架引入的软代价（见 §4）仅**增加**边的遍历成本而不缩短地理距离。大圆距离因此仍为有效下界，启发式的可采纳性在约束参与下不变。

### 3.2 Yen K-最短路径算法

#### 3.2.1 问题定义

给定有向图 $G = (V, E)$、源点 $s$、汇点 $t$、正整数 $K$，求 $s$ 到 $t$ 的前 $K$ 条无环最短路径，按路径总代价升序排列。

#### 3.2.2 Yen 算法流程

实现于 `yen_kshortest.cc`。记 $A = \{A^1, A^2, \ldots\}$ 为已接受路径集合，$B$ 为候选路径优先队列（按代价排序）。

```
Yen-KSP(G, s, t, K):
  A[1] ← A*(G, s, t)                    // 第 1 条最短路径
  A ← {A[1]}
  B ← ∅

  for k ← 2 to K:
    for i ← 1 to |A[k-1]| - 1:           // 沿上一路径每个节点做 spur
      spur_node ← A[k-1][i]
      root_path ← A[k-1][0..i]           // 根路径

      // 删除边：对所有已接受路径 p ∈ A 中 root_path 为前缀的 p，
      // 删除 p 从 spur_node 出发的下一段边
      for each p ∈ A:
        if p[0..i] == root_path:
          临时删除边 (p[i], p[i+1])

      // 删除节点：root_path 上除 spur_node 外的所有节点
      临时删除 root_path[0..i-1] 中的所有节点

      spur_path ← A*(G, spur_node, t)    // 从 spur_node 出发的最短 spur
      if spur_path 存在:
        candidate ← root_path + spur_path[1..]
        B ← B ∪ {candidate}

      恢复所有临时删除

    if B == ∅: break
    A[k] ← B 中代价最小的候选
    B ← B \ {A[k]}
```

**关键洞察**：每条新路径都是在某条已接受路径的某个节点处"拐弯"的结果。候选池 $B$ 持有所有可能的拐弯选项，每次选择代价最小者进入 $A$，确保严格按代价升序输出前 $K$ 条路径。

#### 3.2.3 性能瓶颈

在真实数据集（$V \approx 270\text{k}$）上，Yen 的性能随 $k$ 急剧恶化：

| $k$ | 优化前 (ms) |
|-----|------------|
| 1   | 0.93       |
| 3   | 23.5       |
| 10  | 103        |

$k=1 \to k=3$ 出现约 25× 的性能断崖。在 $k=10$、路径长约 30 个节点时，约发生 **270 次 spur 操作**，每次足图搜索约 1–5ms。瓶颈在于："这些 spur 包含大量冗余计算。"

### 3.3 Lawler 优化

#### 3.3.1 核心洞察

在第 $k$ 轮，前一路径 $A^{k-1}$ 的**每个**节点都执行 spur 操作。但 $A^{k-1}$ 与更早路径共享前缀，那些共享前缀上的 spur **在更早轮次已经计算过，其结果已在 $B$ 中**。

#### 3.3.2 Lawler 修正

**定义 1（偏离节点索引）** 候选路径 $c$ 的偏离节点索引 $\text{dev}(c)$ 为其父路径上生成它的 spur 节点的位置。

**Lawler 规则**：当候选路径被接受为 $A^k$ 后，下一轮仅从 $\text{dev}(A^k)$ 开始执行 spur，跳过之前的位置。

```
Yen-with-Lawler(G, s, t, K):
  A[1] ← A*(G, s, t)
  A ← {A[1]}
  B ← ∅
  last_deviation ← 0                    // 首条路径从节点 0 开始 spur

  for k ← 2 to K:
    for i ← last_deviation to |A[k-1]| - 1:   // 仅从偏离索引开始
      ... (spur 逻辑同 Yen)

    if B == ∅: break
    best ← B 中代价最小的候选
    A[k] ← best
    B ← B \ {best}
    last_deviation ← best.deviation     // 记录偏离索引
```

#### 3.3.3 正确性证明

**定理 2（Lawler 优化的正确性）** 在 Yen 算法中，设第 $k$ 轮接受的路径为 $A^k$，其偏离索引为 $d = \text{dev}(A^k)$。则对所有 $i < d$，在 $A^k$ 的位置 $i$ 处执行 spur 不会产生任何不在 $B$ 中的新候选路径。

**证明** 考虑任意 $i < d$。根路径 $\text{root} = A^k[0..i]$ 等于某条更早接受路径的对应前缀（否则 $d \leq i$）。在更早轮次中，该根路径下所有可能的偏离都已被穷尽探索并加入 $B$。因此以同一 root 重新 spur 只会重新生成已在 $B$ 中的候选。跳过这些位置不会遗漏任何新候选。 $\square$

**推论 2** Lawler 优化是纯去冗余变换——接受路径集合与排序保持不变，仅消除冗余计算。

#### 3.3.4 Lawler 的三项正确性前提

1. **$B$ 跨轮次持久存在，不清空**：`candidates` 容器定义在 `k` 循环之外。
2. **边禁止语义跨轮次一致**：`std::equal(root..., p.vertices...)` 仅禁止与当前 root 共享前缀的已接受路径的对应边——非全局禁止。
3. **每个候选记录偏离索引**：`Candidate` 结构体增加 `int deviation` 字段，仅作元数据不参与排序关键字。

### 3.4 多源多汇扩展

#### 3.4.1 问题建模

实际航路搜索并非单源单汇。机场通过 SID/STAR 的多个过渡定位点（transition fixes）连接航路网，出发和到达端各有**候选点集合**，每点携带**种子代价**（沿程序多边形的累计距离）。

#### 3.4.2 虚拟超源/超汇

`FindKShortestPathsMulti` 使用**概念上的超源节点**：

- 循环索引 $i = -1$ 表示"在超源处偏离"——重新运行多源搜索、禁止已用起始定位点，切换到不同 SID 入口；
- $i \geq 0$ 表示标准单源→多汇 spur。

#### 3.4.3 超源下的 Lawler 有效性

**定理 3（多源 Lawler 的同构性）** 将"超源 → 第一个定位点"段视为路径的虚拟 $-1 \to 0$ 段后，多源多汇 Yen 与标准 Yen **同构**。

**证明思路** 偏离节点概念直接转移，唯一差异是偏离索引域须包含 $-1$。超源无物理节点，不纳入 `banned_nodes`；当 $i < 0$ 时代码进入"重新运行多源搜索 + 禁止起始定位点"分支——这与标准 Lawler 的"从最小偏离索引开始"完全一致。 $\square$

**实现细节**：
- 首条路径的初始搜索视为在超源处偏离，`last_deviation` 初始化为 $-1$；
- 在 $i = -1$ 处生成的候选收到 `deviation = -1`；
- 下一轮循环 `for (int i = last_deviation; ...)` 从该值开始。

#### 3.4.4 代价计算

`CostOfPathMulti` 执行端到端代价重新计算，**包含两端种子代价**，确保不同定位点对的候选在统一尺度下正确排序。

---

## 4. 合规约束框架

### 4.1 设计哲学

v1/v2 求地理最短路径，忽视了航路方向性、高度带限制、最低安全高度等运行约束。v3 引入**可插拔约束框架**，将合规检查从图结构层贯穿至搜索层。

### 4.2 四层合规架构

#### 第 1 层：图结构层——有向性

航路方向在图构建时编码为有向边的存在性（§2.3），搜索算法无需感知。

#### 第 2 层：可插拔约束框架——硬过滤 + 软代价

**核心数据结构：**

```
struct EdgeVerdict {
    bool   allowed;      // false ⇒ 硬过滤：边不可用
    double extra_cost;   // 软代价：加到遍历代价上
};
```

搜索时（`astar.cc` 的 `EdgeAllowed`），每条被松弛的边由所有活跃约束逐一评估：

- **硬过滤**（`allowed = false`）：边完全禁止，搜索必须绕行；
- **软代价**（`extra_cost > 0`）：边仍可用但更"昂贵"，搜索倾向于回避——用于偏好而非禁止；
- **任一约束 block 则边不可用；软代价跨约束累加**。

约束之间无状态、相互独立。新增限制（如 Eurocontrol RAD/CDR）仅需添加新 `Constraint` 子类，不触动图或算法。

#### 第 3 层：三个内置约束

**AltitudeBandConstraint（硬约束）**：
巡航高度层必须落在航路段的 $[\text{base\_fl}, \text{top\_fl}]$ 区间内。若不在区间内，该段禁用。DCT（直飞）合成边的 $\text{base} = \text{top} = 0$ 视为无高度限制。

**MoraConstraint（硬约束）**：
巡航高度不得低于所在位置的 MORA（Grid Minimum Off-Route Altitude，见 §6）。当 MORA 数据缺失（格网值为 0）时，不施加下限。

**LevelPreferenceConstraint（软约束）**：
表达高空航路（Jet）与低空航路（Victor）的偏好。不匹配的航路类型产生额外代价而非直接禁止——"跨层衔接是常见的"。

#### 第 4 层：Yen K-最短——候选集用于选择

返回 top-K 候选而非单一路径，每个候选本身合规（约束在每条路径的搜索中参与）。K 个候选给用户或上层逻辑提供选择空间。

### 4.3 数学形式化

**定义 2（合规路径）** 路径 $P = v_0 \to v_1 \to \cdots \to v_m$ 是**合规的**，当且仅当对每条边 $e_j = (v_j, v_{j+1})$ 和每个活跃约束 $\mathcal{C}_i$：

$$\mathcal{C}_i.\text{allowed}(e_j, Q) = \text{true}$$

其中 $Q$ 为查询参数（巡航高度等）。路径总代价为：

$$\text{cost}(P) = \sum_{j=0}^{m-1} \left( w(e_j) + \sum_i \mathcal{C}_i.\text{extra\_cost}(e_j, Q) \right)$$

---

## 5. 终端程序建模与航路网衔接

### 5.1 问题：最近航路点陷阱

直观方案是从机场坐标画直线连接最近航路点。这在真实数据中完全失败——机场周边最近航路点几乎全是终端进近点，仅存在于 SID/STAR 程序内部而非航路网中。以 KJFK 为例：**周边 96 个最近点中仅 1 个真正在航路网中**。机场将被困在终端"死胡同"中。

**结论**：机场**必须**通过已发布的真实程序连接，程序建模是航路计算的前提而非可选项。

### 5.2 ARINC 424 / CIFP 与路径终结码

X-Plane `CIFP/<ICAO>.dat` 文件每行格式为 `记录类型:序号,逗号分隔字段;`，记录类型包括 SID、STAR、APPCH、RWY、PRDAT。每条命名程序（如 `DEEZZ5`）由多个**航段（legs）**组成，每个航段携带**路径终结码（path terminator）**，定义飞机如何飞越该段以及由什么终止。

在全部 14,838 个机场、192,283 条程序的语料库中，共使用 **23 种路径终结码**，分为两类：

| 类别 | 终结码 | 特征 |
|------|--------|------|
| 定点型 | TF, IF, DF, CF | 终点为可解析为图顶点的定义航路点，占绝大多数 |
| 航向/弧/高度/等待型 | VA, VM, CA, VI, VR, FM, RF, HM 等 | 终点非定点——可能是高度、航向截获或等待航线 |

### 5.3 非定点航段的重评估

初始设计假设非定点航段会阻塞程序连通性，需要将其"折叠"为等效边。但全量语料分析推翻了这一假设：

- **STAR：0%** 完全由非定点航段构成；
- **SID：仅 2.43%** 完全由非定点航段构成。

这 2.43% 为雷达引导离场（如 KPHL 的 PHL4 使用 VA→VM），其设计上即无固定过渡定位点。"不该伪造一个出来"——项目拒绝虚构过渡点。定点航段已覆盖全部 STAR 和 97.6% 的 SID 连通性。

### 5.4 过渡定位点选择

**核心决策**：SID 可将其途经的**每个**航路网定位点作为移交点（不限于最后一个定位点），STAR 对称处理。`ProcedureConnector` 暴露程序的每个航路网定位点为候选 `Connection`。

**设计要点**：

1. **按顶点去重，聚合 ProcedureRefs**：多个程序共享同一固定位点时，所有可互换的 SID/STAR 列在一起；
2. **种子 = 沿发布折线的累计距离**（`FixHit.cumulative_nm`），仅机场与记录端点之间的未测量段以直线填充。绕远才到达的定位点获得更大（更诚实）的种子，自然偏向更近的航路网定位点；
3. **方向敏感的航路网判定**：SID 要求过渡定位点有**出边**（`HasOutbound`），STAR 要求有**入边**（`HasInbound`）。

**实例**：VHHH 的 `ABEY` 系列 STAR 入口 `ABBEY` 仅通过单向段 `FISHA→ABBEY` 可达。仅用出边检查会漏掉它，迫使 RJTT→VHHH 绕道西南经 SIKOU 走 `SIER7C`。切换入边检查后走 `FISHA→ABBEY` + `ABEY` STAR，节省约 **330 NM**。

### 5.5 语义诚实：三种连接模式

当程序无法连通时，系统退回到直飞（DCT），但原因不同，混为一谈会产生误导。`ConnectionKind` 区分三种情况：

| 枚举值 | 含义 |
|--------|------|
| `kProcedure` | 通过程序成功连接 |
| `kRadarVectors` | 机场有 SID/STAR 但均无法到达航路网定位点（雷达引导离场） |
| `kDirect` | 机场无任何程序数据，纯 DCT 回退 |

CLI 和 JSON 输出均明确标注 "RADAR VECTORS"，区分雷达引导与数据缺失。

---

## 6. 地形安全模型

### 6.1 两种尺度的最低安全高度

BravoFinder 建模两种不同的最低安全高度概念：

| 维度 | MORA（Grid MORA） | MSA（最低扇区高度） |
|------|-------------------|---------------------|
| 尺度 | 航路级，全球连续 | 终端级，稀疏 |
| 覆盖 | $1° \times 1°$ 全球格网 | 以特定定位点为中心 |
| 查询键 | 任意坐标 | 机场 ICAO + 磁方位/距离 |
| 结构 | 二维数组 $O(1)$ 索引 | 线性扇区列表 |
| 航路角色 | 硬过滤约束 | 查询输出信息 |

### 6.2 MORA：稠密 1° 格网

#### 6.2.1 数据结构

180 纬度行（$-90°$ 到 $+89°$）× 360 经度列（$-180°$ 到 $+179°$），行主序 flat array。总计 64,800 个 `int16` 值，约 127 KB，常驻内存。

#### 6.2.2 关键实现细节

**向负无穷取整**：坐标 $-12.3°$ 属于 $[-13°, -12°)$ 格网单元。标准 C 风格 `(int)(-12.3) = -12` 得到错误单元，须使用 `FloorToInt` 正确处理负分支。

$$\text{cell\_lat} = \lfloor \phi \rfloor, \quad \text{cell\_lon} = \lfloor \lambda \rfloor$$

其中 $\lfloor \cdot \rfloor$ 为向负无穷取整。

**零值语义**：MORA 值 0 表示"无数据"而非"海平面"。稀疏区域（如开阔海洋）缺乏 MORA 值，保留为零。航路计算时，零值视为"无下限"而非"最低 0 英尺"。

#### 6.2.3 高度单位

以飞行高度层（FL，百英尺）存储。二进制文件格式简单地将 64,800 个 `int16` 数存储为连续块。

### 6.3 MSA：按机场索引的稀疏扇区

MSA 以特定定位点为中心，按磁方位划分为扇区。每个弧记录起始方位（顺时针到下一弧的起点）、最低安全高度（百英尺）和半径（海里）。扇区按机场 ICAO 线性过滤——数据量小到不需要额外索引。

### 6.4 MORA 参与航路计算的机制

`MoraConstraint` 作为硬过滤约束：

$$\text{allowed}(e, Q) = \begin{cases} \text{true} & \text{若未指定高度} \\ \text{true} & \text{若目标格网 MORA = 0 (无数据)} \\ \text{false} & \text{若 } \text{FL}_{\max}(Q) < \text{MORA}(\text{coord}(e)) \\ \text{true} & \text{否则} \end{cases}$$

**语义诚实声明**：MORA 是 MSL（平均海平面）高度，而飞行高度层是气压高度。两者严格意义上不可直接比较。当前代码直接将两者比较，作为"合理的安全下限近似"——刻意保守。这是"有文档记录的近似，绝不伪装为精确"。

---

## 7. 航路串压缩

### 7.1 问题定义

内部每个 `RouteLeg` 携带 `to`（航路点）和 `via`（航路）。ICAO Doc 4444 格式省略同一航路上的中间点，仅保留出入点：

- **未压缩**：`KLAX GARDY V210 HESPE V210 APLES V442 HEC J64 GLACO J64 PGS J64 ...`
- **压缩后**：`KLAX SID GARDY V210 APLES V442 HEC J64 RSK J110 ALS ...`

### 7.2 字符串相等方法的陷阱

平行（并发）航路被编码为拼接字符串如 `V28-Y28`，意味该段同时属于 V28 和 Y28。一条全程沿 Y28 的路径，内部航路段可能显示为 `… Y28 … V28-Y28 … Y28 …`。基于 `via` 字符串相等的合并会将这三个视为不同航路，**制造虚假的转接点**。

### 7.3 累积交集算法

**核心洞察**：每段的 `via` 不是单个航路名而是**集合**（如 `V28-Y28` = {V28, Y28}）。相邻航段可被折叠仅当它们的集合有共同幸存元素——即累积交集非空。

**算法**：

```
Running ← SplitDesignators(legs[i].via)     // 初始运行集
j ← i
while 下一段存在且非 "DCT":
    inter ← Intersect(Running, SplitDesignators(legs[j+1].via))
    if inter.empty(): break
    Running ← inter
    j ← j + 1
chosen ← Running.front()                     // 第一个幸存者
```

**正确性论证**：选交集的首个元素是有效的，因为所选标识符属于累积交集，后者是每个独立航段标识符列表的子集。因此该名称确实存在于组内每个航段。

**实例**：`Y28` ∩ `{V28, Y28}` = {Y28}（非空），然后 {Y28} ∩ `Y28` = {Y28}（非空）。三段折叠为单一 `Y28`，虚假转接点消失。

### 7.4 并发信息保留

每段折叠后的 `concurrent_airways` 字段仅在原始标识符列表超过一个条目的航段上填充：

- **JSON 输出**：`concurrent_airways` 键仅在有真正并发时出现；
- **文本表**：via 列包含 `(concurrent: V210, V394)` 标注。

### 7.5 DCT、SID、STAR 边界处理

- **DCT** 作为硬边界，不参与折叠，输出为独立段；
- **SID/STAR** 是字面量连接词，非程序名。程序真实名称储存在 `Route::sid`/`Route::star` 中。

---

## 8. 性能工程

### 8.1 启发式记忆化

**问题**：在多源 Yen 搜索中，目标集在整个 Yen 轮次中不变，但 $h(v)$ 在数百次 spur 中每次都被重新计算 $O(|\text{goals}|)$ 次。

**方案**：跨 spur 缓存已计算的启发式值。由于同一轮次内目标集恒定，$h(v) = \min_{g \in \text{goals}} d_{\text{gc}}(v, g)$ 的结果可以复用。

**实测效果（k=10）**：103.9 ms → 44.4 ms（约 2.3×）。

### 8.2 搜索工作区复用

**问题**：每次 A* spur 调用需要分配 + 零初始化 5 个 size-V 的数组（`g`, `geo`, `prev`, `closed`, `seed_table`），分配 + `std::fill_n` 占 k=10 场景约 23% 的自有时间（perf 采样）。

gprof 曾将这一成本大部分内联入主循环的 ~93.7%，使只有 `SeedTable` 的 ~0.9% 独立可见，导致团队最初判断"省不掉"而拒绝优化。

**方案**：`SearchWorkspace` 使用**代数戳（generation stamp）**技术——数组在栈上分配一次，用代数标记替代 $O(V)$ 的清零。逻辑清零为 $O(1)$。

**实测效果（k=10，在 +Lawler 基础上）**：30.2 ms → 13.2 ms（约 2.3×）。

### 8.3 内存布局优化

#### 8.3.1 GraphEdge：15 字节磁盘 / 16 字节内存

```
int32  to              // 目标顶点
float  distance_nm     // 存储为 float；A* g-cost 使用 double 累加
uint16 airway_id       // 航路标识（~12k 值，远低于 65535 限制）
int16  base_fl, top_fl // 高度带
uint8  flags           // bit0 = is_high
```

15 字节磁盘编码（无对齐填充）和 16 字节内存表示，相比早期 32 字节结构将边数组体积减半，A* 遍历时每条 cache line 的边数量翻倍。

#### 8.3.2 FixedIdent：12 字节内联标识符

顶点级（27 万）和 CIFP 级（76 万）的航路点标识符使用 `FixedIdent`（12 字节、长度前缀内联）代替 `std::string`，零堆分配，分别节省约 14 MB 和 40 MB。

#### 8.3.3 查找表压缩

`ident_all_` 从 `unordered_map` 替换为"排序数组 + 二分查找"：查找延迟差异约 49 ns/op，内存从 ~26 MB 降至 ~4 MB。A* 热路径上不涉及此查找。

### 8.4 性能分解表

**测试条件**：AMD EPYC 9K65, 16C/32T, 64 GB RAM, gcc 12.3 -O2, AIRAC cycle 2601, $V=270{,}821$, $E=345{,}801$。工作负载：10 个真实城市对 × 30 轮 = 300 次查询。

| $k$ | baseline | +memoize | +Lawler | +workspace | 累积加速比 |
|-----|----------|----------|---------|------------|-----------|
| 1   | 9.87 ms  | 9.93 ms  | 9.90 ms | 9.94 ms    | 1.0×      |
| 3   | 30.76 ms | 17.69 ms | 16.23 ms | 11.48 ms  | **2.68×** |
| 5   | 51.40 ms | 24.82 ms | 20.54 ms | 11.96 ms  | **4.30×** |
| 10  | 103.92 ms| 44.42 ms | 30.21 ms | 13.20 ms  | **7.87×** |

**解读**：
- $k=1$ 在所有四个变体下性能几乎不变（~9.9 ms）——三项优化**零退化**；
- **Memoization** 消灭了跨 spur 重复的启发式计算（k=10：103.9→44.4 ms）；
- **Lawler** 剪枝了冗余 spur 搜索本身（k=10：44.4→30.2 ms）；
- **Workspace** 消灭了每次 spur 的数组分配/清零固定成本（k=10：30.2→13.2 ms）；
- **收益随 k 增长**：更多 spur 意味着三项优化都有更多发挥空间。k=10 累积加速 **7.87×**。

---

## 9. 二进制缓存格式

### 9.1 设计原则

`.bfdb` 格式以**显式定宽小端序**编码所有整数，与主机字节序解耦。浮点数按 IEEE-754 位模式经 `memcpy` 转换为同宽无符号整数存储，确保精确往返。

**拒绝 mmap**：mmap 的零拷贝需求会将结构体填充、字节序和 `sizeof` 焊入文件格式，破坏 x86/ARM 可移植性。

### 9.2 统一容器结构

```
[file header]    magic "BFDB", format_version, section_count, cycle,
                 program_version, source_loader, data_dir, pool_len
[section table]  3 固定条目: (type U32, offset U64, length U64)
[global pool]    pool_len 字节，三区段共享
[graph section] [cifp section] [detail section]  — 连续排列
```

**全局字符串池**位于头部之后而非文件末尾，使得读取器无需 seek 到文件末尾即可立即解析字符串。通过内部 `unordered_map` 追踪已驻留字符串——航路点名、标识符、航路名和 ICAO 代码在三段之间大量重复，去重将三个本地池总计 ~8.7 MB 压缩为单个 ~1.5 MB 全局池（**83% 缩减**）。

### 9.3 按需加载 vs. 预加载

| 模式 | 冷启动 | 常驻内存（峰值 RSS） |
|------|--------|---------------------|
| 按需（默认） | ~0.20s | ~101 MB |
| 预加载（`--cifp-load eager`） | ~0.29s | ~169 MB |

按需模式下 `Open` 仅读取头部 + 段表 + 全局池 + CIFP 目录（~3 MB）。CIFP 段内含 `ICAO → (offset, length)` 目录；`Fetch(icao)` 使用定位读（`pread` / `ReadFile` + `OVERLAPPED`）按绝对文件偏移获取单个机场的程序段。

### 9.4 三层版本控制

1. **程序版本**（CMake `project VERSION`）
2. **容器 `format_version`**：单一机器强制版本号，控制所有布局；不匹配返回 `kFormatMismatch`
3. **来源信息**：程序版本 + source_loader + AIRAC cycle 写入容器头部

规则："修改缓存磁盘布局 → 递增容器 `format_version`"；读取侧或内部函数变更不需要递增。

### 9.5 鲁棒性保证

- 所有从文件头部读取的计数字段在 `resize` 前以上界检查（剩余字节 ÷ 每元素最小磁盘字节数）；
- 段偏移和长度在 `Open` 时与实际文件大小交叉验证；
- CIFP 内部相对偏移以 CIFP 段长度验证；
- 所有损坏路径返回 `Result::Err`，不通过 `bad_alloc` 或 `length_error` 崩溃。

---

## 10. 线程安全模型

### 10.1 合同 B：一个数据库，多线程并发查询

`NavDatabase::Open()` / `OpenCached()` 成功后，实例除一个内部同步的程序缓存外**完全只读**。`FindRoutes()` 和 `MsaForAirport()` 为 `const`，可被多线程并发调用。

合同**不**保证 `NavDatabase` 的并发移动赋值或析构——生命周期操作须保持单线程。

### 10.2 状态分类与同步策略

| 组件 | 策略 |
|------|------|
| Graph/MSA/MORA 数据 | `Open` 后不可变，零同步 |
| 程序缓存（按需） | 双重检查锁定：仅锁 map 操作，磁盘解析在锁外，`try_emplace` 保证插入一次性，指针跨 rehash 稳定 |
| 程序缓存（预加载） | `Open` 时全量预加载后**冻结**——无锁读，无插入 |
| CIFP 段获取 | 共享只读句柄 + 定位读（`pread`/`OVERLAPPED`），无共享文件游标 |
| 搜索（A*/Yen） | 全部状态为函数局部；禁止集合按值捕获；记忆化表每次调用独立构建 |

### 10.3 双重检查锁定的三阶段

`ProceduresFor`（按需模式）：

1. **加锁，检查缓存**：若键存在（包括缓存的 `nullptr` 表示"无程序"），立即返回；
2. **解锁，磁盘解析 CIFP**：I/O 在锁外执行——不同机场的并发查询可并行解析、互不阻塞；
3. **重新加锁，`try_emplace` 插入**：保留先到者的条目，后到者的解析结果直接丢弃（无害的重复工作）。

**指针稳定性**：缓存"只增不删"，值为 `unique_ptr<CifpData>`。map rehash 时仅节点指针移动，堆上 `CifpData` 对象地址不变。`ProceduresFor` 返回的 `const CifpData*` 在实例生命周期内有效。

### 10.4 ThreadSanitizer 验证

专用 `tsan` 预设 + 8 线程并发集成测试套件：
- 混合同键加载（压力测试缓存竞争）；
- 不同键加载（压力测试并行解析与 rehash）；
- 预加载模式：8 线程并发计算航路（验证无锁读路径零竞争）；
- CIFP `Fetch`：8 线程并发获取不同机场。

**测试有效性已证实**："早期去掉锁后 TSan 立即报 `procedure_cache_` 的 emplace/rehash 竞争"。

---

## 11. HTTP 服务体系

### 11.1 面向服务而非进程内嵌

上层业务逻辑（Go 网关）通过 HTTP+JSON 调用 `bf-http`，而非通过 cgo/pybind 嵌入 C++ 库：

- 航路查询为 10–30ms 的纯计算，JSON 序列化（微秒级）和 localhost 往返（亚毫秒级）开销可忽略；
- 进程隔离：C++ 崩溃不拖垮 Go 网关；
- 消费者通过 `encoding/json` 反序列化获得强类型对象。

### 11.2 共享中性服务层

`bf::service` 命名空间下的 9 个 handler 和 `NavDatabaseRegistry` 是传输无关的查询逻辑。MCP 和 HTTP 作为对等消费者共享此层。

### 11.3 传输层：libuv + llhttp

**线程拓扑**：

```
libuv loop thread (epoll) — accept / read / llhttp parse / write-back, 全部非阻塞
        │  uv_queue_work: 分发到线程池
        ▼
libuv 内置线程池 — handler (10–30ms CPU)
        │  after_work 回调（自动回到 loop 线程）
        ▼
loop thread: 若连接存活则写响应，否则丢弃
```

**铁律**：10–30ms 的航路计算永不在 loop 线程上执行。

**连接活性守卫**：`Connection` 使用 `std::shared_ptr` 持有 `self_` 强引用。每个在途工作项持有额外强引用，即使客户端断开、loop 线程已 `uv_close` 其句柄，`Connection` 对象及其句柄内存仍存活到工作完成。`after_work` 回调检查 `IsAlive()`——若连接已关闭，丢弃响应。

### 11.4 MCP-over-HTTP

`bf-mcp --transport http` 支持 Streamable HTTP（2025-03-26），单一端点 `/mcp`：

- `POST /mcp`：JSON-RPC 请求，根据 `Accept` 头返回 JSON 或 SSE；
- `GET /mcp`：SSE 长连接；
- `DELETE /mcp`：无状态 ACK。

`Dispatcher` 处理批量 JSON-RPC（stdio 和 HTTP 均支持）。错误码集中于 `jsonrpc.h` 的 `constexpr` 值。

---

## 12. 性能评估

### 12.1 测试环境

| 项目 | 规格 |
|------|------|
| CPU | AMD EPYC 9K65, 16C/32T |
| RAM | 64 GB |
| OS | Linux 6.6 |
| 编译器 | gcc 12.3, -O2 |
| 导航数据 | X-Plane 12, AIRAC cycle 2601 |

### 12.2 冷启动 vs. 缓存加载

| 路径 | 耗时 | 备注 |
|------|------|------|
| 冷启动（`--data navdata`） | ~2.27 s | 完整解析 + 图构建 |
| 缓存加载（`--db nav.bfdb`） | ~0.20 s | 仅反序列化 |

端到端加速约 **11×**。一次性 `bf build`（~3.2 s）仅在 AIRAC cycle 变更时需执行。

### 12.3 查询性能总结

| $k$ | 最终延迟 | 优化前延迟 | 总加速比 |
|-----|---------|-----------|---------|
| 1   | 9.94 ms | 9.87 ms  | 1.0×    |
| 3   | 11.48 ms| 30.76 ms | 2.68×   |
| 5   | 11.96 ms| 51.40 ms | 4.30×   |
| 10  | 13.20 ms| 103.92 ms| **7.87×** |

### 12.4 内存占用

| 模式 | 峰值 RSS | 说明 |
|------|----------|------|
| 按需（默认） | ~101 MB | |
| 预加载（eager） | ~169 MB | 全量 CIFP 常驻 |
| 统一 `.bfdb` 文件 | 60.9 MB | graph + CIFP + detail + 全局字符串池 |

---

## 13. 结论

BravoFinder v3 通过以下层次化设计实现了从"最短几何线"到"合规候选航路集"的范式转换：

1. **图建模层**：CSR 格式 + 构建时有向性编码，为上层提供高效、只读、缓存友好的图表示；
2. **算法层**：A* 搜索（大圆距离可采纳启发式）→ Yen K-最短（多源多汇 + Lawler 剪枝）→ 候选路径集按代价升序输出；
3. **约束层**：可插拔硬过滤 + 软代价框架，使航路方向、高度带、MORA 安全下限、高低空偏好以统一机制参与搜索；
4. **衔接层**：全量 ARINC 424 路径终结码解析 + 每定位点暴露 + 方向敏感航路网判定 + 三种诚实回退模式；
5. **工程层**：启发式记忆化 + Lawler 优化 + 代数戳工作区复用 → k=10 加速 7.87×；FixedIdent 值类型 → 内存压缩；`.bfdb` 统一容器 → 11× 启动加速；双重检查锁定 + 冻结无锁读 → 多线程并发查询零竞争。

最终系统在 27 万顶点图上以 9.9 ms（k=1）至 13.2 ms（k=10）的延迟返回合规候选航路，同时保持约 101 MB 的按需常驻内存。所有优化均有理论正确性保证（可采纳性证明、Lawler 同构性证明）和实验验证（差分随机测试、ThreadSanitizer、gprof/perf 交叉确认）。

---

## 14. 参考文献

1. Hart, P. E., Nilsson, N. J., & Raphael, B. (1968). A Formal Basis for the Heuristic Determination of Minimum Cost Paths. *IEEE Transactions on Systems Science and Cybernetics*, 4(2), 100–107.
2. Yen, J. Y. (1971). Finding the K Shortest Loopless Paths in a Network. *Management Science*, 17(11), 712–716.
3. Lawler, E. L. (1972). A Procedure for Computing the K Best Solutions to Discrete Optimization Problems and Its Application to the Shortest Path Problem. *Management Science*, 18(7), 401–405.
4. Dijkstra, E. W. (1959). A Note on Two Problems in Connexion with Graphs. *Numerische Mathematik*, 1, 269–271.
5. ARINC 424-22. Navigation System Database Standard. Aeronautical Radio, Inc.
6. ICAO Doc 4444. Procedures for Air Navigation Services — Air Traffic Management (PANS-ATM).
7. BravoFinder v3 技术文档. https://github.com/Bokjan/BravoFinder/tree/v3/docs
8. X-Plane 12 NAV DATA. https://developer.x-plane.com/docs/data-development-documentation/
