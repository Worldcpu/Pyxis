# Simple Flight Planner 竞品调研

> 调研日期：2026-08-04。目标：为 Pyxis（航路研究 + 飞行计划工具）做竞品分析，全部结论基于第一手来源（官网公开页面、作者在官方论坛的发布/更新帖、社区论坛帖）。每条事实后标注来源 URL；无法确认的内容标注"推断"。

## 概述

Simple Flight Planner（常被中文社区简称 "simpleflightplanne"）是一个**闭源、免费、浏览器端**的 VFR/IFR 飞行计划工具，官方定位是 "The easiest way to plan scenic VFR and IFR flights for MSFS, FSX, Prepar3D, and X-Plane"（https://www.simpleflightplanner.com ）。它是个人开发者（MSFS 官方论坛用户名为 BrittleBridge06，AVSIM 用户名为 Lnsnifty，https://www.avsim.com/profile/540899-lnsnifty/content/ ）的 passion project，2026 年 2 月 15 日在 Microsoft Flight Simulator 官方论坛发布（https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ），并在 X-Plane.org（https://forums.x-plane.org/forums/topic/343329-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/ ）与 AVSIM（https://www.avsim.com/forums/topic/690764-simplified-flight-planning-free-for-all-major-sims/ ）同步宣传。

核心卖点是"免费 + 零门槛"：官网与支持页反复强调 "100% Free • No account required • Works offline as a PWA" 以及 "The tool is 100% free, with no ads, no login walls, and no premium tiers"（https://www.simpleflightplanner.com 、https://simpleflightplanner.com/support 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ）。截至调研日，产品已从发布初期的"轻量 VFR/GA 观光规划器"演进为覆盖 IFR 程序、空域警告、燃油签派、重量平衡的准航司级工具，并附带一个 WebSocket 遥测配套应用（Simple Flight Tracker）。

## A. 基本盘

### 功能清单

官网称共有 70 项功能（https://www.simpleflightplanner.com ），下表为主要功能（名字 + 一句话描述），细节见下一节。

| 功能 | 一句话描述 |
|---|---|
| 八种规划方式 | Create Your Own / 预置观光航线 / 现实航线 / Waypoints First / Shape Route（GPS 艺术形状）/ Surprise Me! / SimBrief 导入 / Plan by Time（按时长自动生成）（https://simpleflightplanner.com/guide ） |
| 交互式地图规划 | 拖拽编辑航线、航段中点 "+" 插入航点、移动端长按落点（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ） |
| 机场数据库 | 71,500+ 机场（跑道数据、海拔），支持 ICAO/IATA/名称/城市/地图搜索，收藏、最近使用、自定义机场（https://www.simpleflightplanner.com 、https://simpleflightplanner.com/guide ） |
| 导航台数据库 | 260,000+ 导航台与 AIRAC 航路点（VOR、NDB、ILS、DME、FIX）+ 250,000+ AIRAC 五字码 fixes（https://www.simpleflightplanner.com 、https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 ） |
| POI 与观光航线 | 11,000+ 精选 POI、MSFS World Update 1–20 的 150+ 地标、100+ 跨 6 大洲预置观光航线（https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ） |
| 机型数据库 | 1,000+ 机型性能档案（速度、油耗、升限），支持自定义机型（https://www.simpleflightplanner.com ） |
| 燃油与性能计算器 | 密度高度、TAS、起降滑跑距离、航司燃油政策分解（https://simpleflightplanner.com/guide ） |
| 地形感知 | 全程高程剖面图、CFIT 风险分析、建议最低安全高度（https://simpleflightplanner.com/guide ） |
| 程序支持 | 美国 FAA CIFP 真实 SID/STAR/进近 + 20 个精选国际机场程序 + Auto-Procedure Builder 全球自动生成（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 ） |
| 智能建议引擎 | 实时分析计划，最多给出 3 条情境化建议（风/高度/航路匹配/IFR 规则/燃油）（https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 、https://simpleflightplanner.com/guide ） |
| 航路搜索 | V 航路（FL180 以下）/ J 航路（FL180 以上），"吸附到航路"（https://simpleflightplanner.com/guide ） |
| 风估计 | 每航段高空风分量，"derived from surface METAR data"（由地面 METAR 推导）（https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 ） |
| 成本指数 | 0–999，0 表示最高燃油效率，高值优先省时（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ） |
| ETOPS | 距合适备降机场最远距离规划，地图显示 ETOPS 圆（https://simpleflightplanner.com/guide ） |
| 时间规划 | ETD/ETA（NOW 实时模式）、检查点 UTC 时刻表、Plan by Time、多航段行程（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ） |
| 检查点时刻表 | 每段航向、距离、ETE、累计总计 + UTC 预计到达时刻列（https://simpleflightplanner.com/guide ） |
| 日出日落 | 每航路点日出/日落（NOAA 太阳算法），白天/暮光/黑夜徽章（https://simpleflightplanner.com/guide ） |
| 航路检查 | 空域警告：美国 Class B、欧洲 TMA/CTR、禁/限区（DC P-56/SFRA、Camp David、卡纳维拉尔角、白金汉宫），5 NM 缓冲，critical/warning/advisory 分级（https://simpleflightplanner.com/guide ） |
| 重量与平衡 | 乘客/行李/燃油输入 → 实时 CG 包线图，超限视觉反馈（https://simpleflightplanner.com/guide ） |
| 飞行员日志 | 本地记录起降、机型、距离、预计时间、备注，累计统计（https://simpleflightplanner.com/guide ） |
| 天气与航图 | 实时 METAR/TAF、伪 ATIS 生成器、SIGMET/AIRMET 叠加、FAA VFR/IFR 航图叠加、FAA d-TPP 24,000+ 仪表航图内嵌 PDF 预览（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/update-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/201 ） |
| NOTAM | AutoRouter 合作的实时 NOTAM 面板（严重度着色、原始 ICAO 文本 + 解码摘要）（https://www.simpleflightplanner.com 、https://simpleflightplanner.com/guide ） |
| 在线网络 | VATSIM/IVAO 实时飞行员与管制员叠加（https://simpleflightplanner.com/guide ） |
| 签派报告 | 可打印 Flight Release：航线概览、逐项燃油、METAR 简报、自动化安全清单，"DISPATCH APPROVED / REVIEW REQUIRED" 横幅（https://simpleflightplanner.com/guide ） |
| 导出 | MSFS 2020/2024 .PLN（含 2024 SU3+ STAR/ATC & EFB 兼容）、FSX/P3D .PLN、X-Plane 11/12 .FMS v11（https://simpleflightplanner.com/guide 、https://www.simpleflightplanner.com ） |
| 分享链接 | 生成永久有效的唯一链接，任何人有链接即可查看路线/统计/地图并下载 .PLN（https://simpleflightplanner.com/guide ） |
| SimBrief 导入 | 使用 Pilot ID 直接导入航线、机场、高度层、飞行类型（https://simpleflightplanner.com/guide ） |
| 配套应用 | Simple Flight Tracker（Windows，SimConnect → 本地 WebSocket 实时遥测 + 一键发送计划进模拟器）（https://simpleflightplanner.com/companion ） |

### 支持的模拟平台

MSFS 2020 与 2024（.PLN，含 MSFS 2024 Sim Update 3+ 的 STAR/ATC 与 EFB 路由兼容）、FSX 与 Prepar3D（.PLN）、X-Plane 11 与 12（.FMS v11）（https://www.simpleflightplanner.com 、https://simpleflightplanner.com/guide ）。

### 定价与商业模式

完全免费、无广告、无账户、无付费墙，官方承诺 "Simple Flight Planner is — and will always be — completely free. There are no ads, no accounts and no paywalls"（https://simpleflightplanner.com/support ）。收入来源是一次性 PayPal 捐赠（任意金额），捐赠者解锁 4 份礼物：电子书《Beginner's Guide to Creating Flight Plans》、Simple Flight Dashboard Pro（SimConnect 玻璃座舱仪表盘，v1.7）、Simple Dispatch（MSFS 插件管理器）、Simple Profile Manager（图形设置档切换）（https://simpleflightplanner.com/support 、https://simpleflightplanner.com/downloads ）。支持页明言成本所在："Map tiles, weather, NOTAMs and the airport database — all run on paid services"，且 "AIRAC cycles, charts and worldwide airport data are refreshed every month"（https://simpleflightplanner.com/support ）——这是免费产品依靠捐赠维持服务器与数据成本的说明。

### 数据来源

| 数据 | 来源 | 来源 URL |
|---|---|---|
| 机场（71,500+） | OurAirports 免费 CSV（Dashboard Pro v1.7 更新日志明言 "全球 70,000+ 机场（免费 OurAirports CSV）"）；2026-02-16 起机场库迁移云端（71,570 个，含跑道数据、ISO 国家名解析） | https://simpleflightplanner.com/support 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 |
| 导航台/航路点 | AIRAC 周期数据（官网现显示 "AIRAC Cycle 2607"）；VOR 条目 2026-02-16 起全部替换为 OurAirports 验证数据（"Replaced all VOR entries with OurAirports-verified data"）；导航台云库 11,010 个（6,748 NDB、3,653 VOR、442 TACAN、167 DME） | https://www.simpleflightplanner.com 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 |
| AIRAC 周期 | 每月刷新（"AIRAC cycles, charts and worldwide airport data are refreshed every month"） | https://simpleflightplanner.com/support |
| 程序数据（SID/STAR/进近） | 美国：FAA 免费发布的 CIFP 数据（"The FAA publishes its CIFP data for free"）；国际：20 个枢纽人工精选（约 70 个程序）；全球其余：算法自动生成（APB） | https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/180 、https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 |
| METAR/TAF | 实时，通过公共航空天气 API（隐私政策："We fetch real-time weather from public aviation weather APIs"） | https://simpleflightplanner.com/privacy |
| 高空风估计 | "derived from surface METAR data"（基于地面 METAR 推导） | https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 |
| NOTAM | AutoRouter 合作方实时 NOTAM（FAQ 有专门条目 "Where does the NOTAM data come from?" 与 "Are NOTAMs available worldwide?"） | https://www.simpleflightplanner.com 、https://simpleflightplanner.com/faq |
| VATSIM / IVAO | 实时在线网络数据（飞行员与管制员位置） | https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 |
| 空域多边形 | OpenAIP：全球约 27,000 个空域多边形（CTR/TMA/Class A–G/禁限区）+ 约 6,000 个 VFR 报告点 | https://forums.flightsimulator.com/t/update-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/201 |
| 航图 | 美国：FAA d-TPP（24,000+ 仪表进近图，每 28 天自动刷新对齐 AIRAC）；其他国家：按地区链到官方源（NavCanada、UK NATS、德国 DFS、法国 SIA、澳洲 Airservices、Eurocontrol EAD）；地图叠加为 FAA VFR/IFR 航图 | https://forums.flightsimulator.com/t/update-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/201 |
| 地形高程 | Open-Meteo 高程 API（APB 每段采样 5 个点） | https://simpleflightplanner.com/guide |
| 跑道几何/地面设施 | OpenStreetMap（高分辨率跑道几何、滑行道、停机位） | https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 、https://www.simpleflightplanner.com |
| POI | 自建 11,000+ 精选 POI + MSFS World Update 地标（150+，WU1–WU20、City Updates、40 周年纪念） | https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 |

注意：官网"下载"页将 Navigraph 与 NavDataPro（付费订阅）和 FAA CIFP（免费、仅美国）列为**外部**第三方数据源链接，即内置数据库与 AIRAC 周期相互独立更新（"The built-in database has 71,500+ airports and is updated independently of AIRAC cycles"）（https://simpleflightplanner.com/downloads ）——这印证了机场库的 OurAirports 来源与 AIRAC 导航台数据是两套体系。

## B. 功能细节

### 燃油与性能计算器

官方用户指南（https://simpleflightplanner.com/guide ）**未披露任何底层公式或模型**，只给出了参数与规则，这是闭源产品的公开信息上限：密度高度"基于机场海拔和温度"（"在出发机场有可用数据时，使用实时 METAR 天气数据来提供准确的密度高度计算"）；起降距离计算并与可用跑道长度对比；基础燃油 = 机型耗油率 × 飞行时间（机型档案含巡航速度、升限、燃油消耗率），显示航程油/储备油/总油量比例条；单位换算支持 GAL/LBS/KG 三态循环，密度系数 "Jet-A1（6.7 磅/加仑，3.04 千克/加仑）适用于喷气式飞机、客机和涡桨飞机；Avgas 100LL（6.0 磅/加仑，2.72 千克/加仑）适用于活塞式飞机"（该单位切换是 2026-03-24 应社区建议加入的，https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/150 ）；航司燃油政策分解为航程油、应急油（"航程燃油的 5%（ICAO 标准）"）、备降油、最终储备（"30 分钟等待燃油"）、滑行油、总轮挡油；风的影响仅有定性描述（"逆风增加燃油需求，顺风减少燃油需求"），无风修正公式；成本指数 "为 0 意味着最高燃油效率（较慢），而较高的值则优先考虑节省时间而非燃油成本"。结论：**公开证据无法确认其模型来源（推断为经验公式/简化模型，非 BADA 等专业数据库）**，Pyxis 若以 BADA/OpenAP 建模则具备差异化空间。

### 航路规划能力

- **SID/STAR/进近三层支持**（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 、https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/180 ）：(1) 美国机场真实 FAA CIFP 程序（含高度/速度限制的完整航路点序列，点击 "Load SIDs"/"Load STARs" 按需导入并缓存）；(2) 20 个精选国际枢纽（EGLL、LFPG、EDDF、RJTT、VHHH、WSSS、OMDB、EHAM、LEMD、LIRF、LEBL、EDDM、LTFM、ZBAA、ZSPD、RKSI、VTBS、WMKK、CYYZ、YSSY），约 70 个"真实航路点序列、高度限制和速度限制"的程序；(3) Auto-Procedure Builder（APB™，2026-04-01 上线）为全球任何机场算法生成 4–6 段进离场程序，按机型意图分三档（Airliner 5–6 段、GA/涡桨 4–5 段、Bush/VFR 3–4 段），每段用 Open-Meteo 高程采样 5 个点、应用 "FAA 标准缓冲区（5,000 英尺以下地形为 1,000 英尺，2,000 英尺以上）"建立 MSA 下限，程序带 "为什么选择这条路线？" 透明性解释面板，并明示 "不保证障碍物净空……仅供模拟器使用"。
- **航路搜索**：V 航路（FL180 以下）/ J 航路（FL180 以上），选择"吸附到航路"以通过已发布的 V/J 航路路由（https://simpleflightplanner.com/guide ）。
- **风估计**：每航段高空风分量，来源于地面 METAR 推导（https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 ）。
- **成本指数**：0–999（https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 ）。
- **ETOPS**："延程双发运行性能标准规划显示距合适备降机场的最远距离"，地图显示 ETOPS 圆；另有备降规划器（150 NM 内按机型跑道长度过滤）（https://simpleflightplanner.com/guide ）。
- **时间规划**：Plan by Time（指定 45 分钟/2 小时等时长自动生成航线）；2026-03-12 加入 ETD（默认当前 UTC，NOW 实时模式）与自动 ETA，检查点时刻表新增 UTC 时钟列（https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/120 、https://simpleflightplanner.com/guide ）。
- **多航段**：行程（Itinerary）把多个计划串成一条航线，累计距离与时间（https://simpleflightplanner.com/guide ）。
- **航线优化器**：nearest-neighbor + 2-opt 算法重排航路点（https://simpleflightplanner.com/guide ）。
- **IFR 高度规则**：半球规则东单西双，智能建议中自动检查（https://simpleflightplanner.com/guide ）。

### 智能建议引擎

2026-03-26 上线（https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 、https://simpleflightplanner.com/guide ）："Smart Suggestions Engine 实时分析您的飞行计划，并呈现情境化建议"，出现在起降机场设置之后（Step 1 与 Step 2 之间），最多 3 条：风优化（逆风时建议反向航线或升高/降低高度）、基于高空风的高度效率、真实世界航路匹配（起降对与精选现实航线吻合时提示）、IFR 单双飞行高度层合规、燃油储备下限警告（长航线）。每条建议带一键应用按钮，可关闭；高优先级警告（如地形）以非阻塞 toast 呈现在地图上。

### 检查点时刻表、日出日落、地形剖面、航路检查

- 检查点时刻表："一个可折叠的表格，显示每个航段的航向、距离、预计航路时间（ETE）和累计总计"，含 UTC 列（基于出发时间估算到达各航路点的时钟时间），可打印作纸质航迹记录（https://simpleflightplanner.com/guide ）。
- 日出日落："每个航路点显示 UTC 的日出和日落时间，使用 NOAA 太阳算法计算"，显示白天/暮光/黑夜徽章（https://simpleflightplanner.com/guide ）。
- 地形剖面：起降之间全程地面高程横截面，巡航高度叠加，识别地形冲突（https://simpleflightplanner.com/guide ）。
- 航路检查：空域警告覆盖美国 Class B（"JFK、LAX、ORD、ATL、DEN、SFO 等"）、欧洲 TMA/CTR（伦敦、巴黎、法兰克福、阿姆斯特丹、马德里、罗马）、禁/限区（DC P-56 与 SFRA、Camp David、卡纳维拉尔角、白金汉宫），分级 critical/warning/advisory，关键区 5 NM 缓冲；OpenAIP 空域数据接入后自动扫描航线巡航高度是否突破 CTR/限制区/TMA 多边形；页面明示"空域边界是简化近似值"（https://simpleflightplanner.com/guide 、https://forums.flightsimulator.com/t/update-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/201 ）。

### 重量与平衡、飞行员日志

- 重量与平衡："输入乘客、行李和燃油，即可在实时 CG 包线图上查看您的重心位置"，支持单发活塞到客机各类别，超限有视觉反馈（https://simpleflightplanner.com/guide ）。
- 飞行员日志：记录起降、机型、距离、预计时间、备注，累计飞行数/距离/小时，全部本地存储（https://simpleflightplanner.com/guide 、https://simpleflightplanner.com/privacy ）。

### 导出、分享、导入

- 导出：MSFS 2020/2024 .PLN（完整 VFR/IFR；2024 输出 SU3+ 兼容，Tracker 另写 `_2024.pln` 变体）、FSX/P3D 旧版 .PLN（直飞）、X-Plane 11/12 .FMS v11；.PLN 上限 99 航路点，超限时导出器"智能简化路径同时保留关键转向点"；剪刀工具可把计划从任一机场拆成两个连续 .PLN（https://simpleflightplanner.com/guide 、https://simpleflightplanner.com/companion ）。
- 分享：唯一链接永久有效，"任何拥有链接的人都可以在交互式地图上查看路线、查看航班摘要并下载 .PLN 文件"，无需账户（https://simpleflightplanner.com/guide ）。
- 导入：SimBrief 通过 Pilot ID 导入航线、机场、飞行高度层、飞行类型（https://simpleflightplanner.com/guide ）；自定义机场/机型可导出导入 JSON 备份（https://simpleflightplanner.com/downloads ）。

## C. 架构线索（推断）

产品是纯浏览器 Web 应用（React 类 SPA 的技术细节未公开），以下基于可观察行为推断：

1. **PWA 离线**：官网明示 "Works offline as a PWA"、"no account required"（https://www.simpleflightplanner.com ），指南建议"PWA 安装、数据备份"并称加载后离线可用（https://simpleflightplanner.com/ecosystem 、https://simpleflightplanner.com/guide ）。**推断**：通过 Service Worker 缓存静态资源与已加载数据实现离线。
2. **本地存储为 LocalStorage**：隐私政策逐项列出已存计划、自定义机型/机场、偏好、日志、收藏均 "stored in your browser's local storage and never leaves your device"（https://simpleflightplanner.com/privacy ），服务条款同述 "stores flight plans locally in your browser"（https://simpleflightplanner.com/terms ）。**推断**：核心用户数据用 LocalStorage 而非 IndexedDB（政策用词是 "local storage"，且指南警告"清除浏览器数据会丢失所有数据"）。10 万+ 航路点不落本地。
3. **后端服务必然存在**（推断，证据链）：(a) 分享链接永久有效、云端可存取，指南明示"分享链接是云端备份途径"；(b) 2026-02-16 官方将机场库（71,570 个）与导航台库（11,010 个）"迁移至云端"，前端"设置起降机场后自动获取相关导航台"、机场浏览器 7 个标签页数据"按标签懒加载"（https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 、https://simpleflightplanner.com/guide ）——260K+ 导航台/fixes 的体量不可能全量进浏览器本地存储，主库在服务端、前端按需拉取是合理架构；(c) METAR 走"公共航空天气 API"（https://simpleflightplanner.com/privacy ），浏览器直连公共 API 存在 CORS/密钥问题，**推断**由自有后端代理；同理 VATSIM/IVAO 数据、AutoRouter NOTAM、Open-Meteo 高程也**推断**经后端或直连第三方完成。
4. **配套应用是本地 WebSocket 桥**：Simple Flight Tracker 是 Windows/.NET 8 桌面应用，架构为 MSFS ↔（SimConnect）↔ Tracker ↔（本地 WebSocket `ws://localhost:29112`）↔ 浏览器中的 Planner，官方保证 "no cloud servers involved, everything stays on your local machine"（https://simpleflightplanner.com/companion 、https://simpleflightplanner.com/ecosystem ）。WebSocket 消息示例为 JSON 命令，如 `{"type":"simrate:set","rate":2.0}`（sim rate 可选 0.25/0.5/1/2/4/8/16）（https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/202 ）。Tracker 遥测字段包括位置/高度/航向/IAS/GS/垂直速度/On Ground，以及自动驾驶模式与目标、分油箱油量、天气（风/OAT/QNH）、操纵面、逐发动机 N1/N2/EGT。
5. **前端数据管理策略**（推断）：主数据（机场、导航台）云端存储 + 按需/懒加载 + 本地缓存（CIFP 程序"再次访问直接从缓存加载"、FAA 航图"每 28 天自动刷新"），浏览器端不持有全量导航数据。这解释了"71,500 机场 + 260K 航路点"规模下仍能秒开的原因。

## D. 版本演进（来源：https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 与 https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 的作者更新帖，日期为论坛时间）

| 日期 | 里程碑 |
|---|---|
| 2026-02-15 | 首发公告，完整功能清单（数据规模、燃油/性能计算器、VFR/IFR、导出、PWA 离线） |
| 2026-02-16 | 数据质量冲刺：移除不可靠 VOR、新增约 85 个经核验 VOR、随后全部 VOR 替换为 OurAirports 验证数据；新增 210+ VOR/VORTAC、140+ NDB；机场库（71,570）与导航台库（11,010）迁移云端；/downloads 提供 4 个机场包（加勒比/非洲/欧洲 GA/东南亚大洋洲）；在线查找支持 IATA 码 |
| 2026-02-17 | 移动端长按（约 600ms）添加自定义航点；Print Route 打印；云端机场搜索排序优化；机场彩色标记与类型子筛选；新增机型（TL Ultralight Sirius TL-3000） |
| 2026-03-04 前后 | 修复某问题（社区回复 "Is it on line?" 确认已上线修复） |
| 2026-03-08/09 | 修复 MSFS 2020 .PLN 导出元数据（错误写入 AppVersionMajor=12）；本地保存 0KB 问题上报 |
| 2026-03-12 | 加入 ETD/ETA 时间跟踪（NOW 模式、检查点表 UTC 列） |
| 2026-03-22 | 修复 0KB 下载（保存方式变更），定位根因为浏览器扩展/杀毒软件拦截 blob 下载 |
| 2026-03-24 | 新更新帖（#761277）：FAA CIFP 导入管道 + 20 个精选国际机场程序（约 70 个） |
| 2026-03-25 | 内置航图系统上线：FAA d-TPP 24,000+ 仪表进近图（内嵌 PDF 预览，28 天自动刷新），国际机场链官方航图源（搜索确认，https://forums.flightsimulator.com/t/update-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959/201 ）；燃油单位 GAL/LBS/KG 切换 |
| 2026-03-26 | Smart Suggestions Engine 上线 |
| 2026-04-01 | Auto-Procedure Builder（APB™）：全球任意机场自动生成 SID/STAR（三层：真实 FAA CIFP / 20 国际精选 / 算法生成） |
| 2026-04 前后 | OpenAIP 全球空域（约 27,000 多边形 + 6,000 VFR 报告点）接入（搜索确认，同上） |
| 2026-05-10 | Simple Flight Tracker v2.1 大版本：一键安装、后台托盘运行、大幅扩展遥测、sim rate 控制 |
| 未标注日期（首页 "New" 徽标） | MSFS World Update POIs、Real-World Routes、Custom Aircraft Profiles、Custom & Online Airports、IFR Alternate Airports、Quick vs Full 双模式、Advanced Airport Explorer、Smart Route Editing（拖拽/中点插入/拆分导出）、AutoRouter NOTAM 集成（https://www.simpleflightplanner.com ） |

**演进优先级观察**：先打底数据规模与 VFR/GA 观光体验（发布期）→ 集中火力修数据质量与云端化（2026-02）→ 转向 IFR 程序能力（CIFP/国际/APB，2026-03 下旬至 04）→ 智能建议与签派类"航司味"功能（03 下旬）→ 航图/空域合规类（04）→ 配套应用与生态（05）。与 Pyxis 的"航路研究 + 合规航路 + 飞行计划"定位相比，Simple Flight Planner 的 IFR 程序（FAA CIFP 免费数据 + 算法生成）与零成本数据策略（OurAirports + OpenAIP + FAA 公开数据）是最值得对照的两点。

## 开源情况

未开源。搜索 "simpleflightplanner github"、"simple flight planner github source" 均无任何仓库结果；产品定位为免费闭源 Web 服务，官网与论坛均无源码发布声明。**结论：闭源，无法复用其代码；但数据层全部依赖免费公开数据集（OurAirports、OpenAIP、FAA CIFP/d-TPP、Open-Meteo），数据链路可复现。**

## 来源列表

官网（均抓取成功，除注明外）：
1. https://www.simpleflightplanner.com — 首页（功能、数据规模、AIRAC 2607、PWA、捐赠、免责声明）
2. https://simpleflightplanner.com/guide — 用户指南（21 章，功能与模型细节最全）
3. https://simpleflightplanner.com/faq — FAQ（35 个问题标题，答案折叠未能抓取，内容有取舍）
4. https://simpleflightplanner.com/support — 捐赠/支持（作者声明、礼物、月度数据刷新、服务器成本）
5. https://simpleflightplanner.com/privacy — 隐私政策（LocalStorage、第三方天气 API）
6. https://simpleflightplanner.com/terms — 服务条款（本地存储、真实导航禁用）
7. https://simpleflightplanner.com/downloads — 下载页（Tracker v2.1、机场包、外部数据源链接）
8. https://simpleflightplanner.com/companion — Simple Flight Tracker 配套页（ws://localhost:29112）
9. https://simpleflightplanner.com/ecosystem — 产品生态页（四工具集成、本地优先架构）
10. https://simpleflightplanner.com/auto-procedure-builder — APB 宣传页（未单独抓取，细节来自指南与论坛）

论坛（作者更新日志主来源）：
11. https://forums.flightsimulator.com/t/release-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/757959 — 发布帖（2026-02-15 起，200+ 楼，作者 BrittleBridge06 持续更新；含 /30、/90、/95、/120、/140、/150、/180、/197、/201、/202 各页）
12. https://forums.flightsimulator.com/t/update-simple-flight-planner/761277 — 独立更新帖（2026-03-24 起：CIFP、20 国际机场、Smart Suggestions）

社区论坛（直接抓取均返回 403 Forbidden，内容经由搜索结果摘要获取，已在正文对应处标注）：
13. https://forums.x-plane.org/forums/topic/343329-free-flight-planning-tool-for-the-flight-sim-community-simple-flight-planner/ — X-Plane.org 发布帖
14. https://www.avsim.com/forums/topic/690764-simplified-flight-planning-free-for-all-major-sims/ — AVSIM 发布帖（标题经版主 Ray Proudfoot 修改为 "Simplified flight planning - free for all major sims."，作者为 Lnsnifty）
15. https://www.avsim.com/profile/540899-lnsnifty/content/ — AVSIM 作者 Lnsnifty 档案页（确认 Lnsnifty = 开发者）

搜索佐证：
16. "Simple Flight Planner" site:forums.flightsimulator.com 与 "simpleflightplanner github" 等搜索（开源确认、d-TPP/OpenAIP 细节）

未抓取成功、已跳过并注明的页面：X-Plane.org 与 AVSIM 论坛帖正文（403）；FAQ 答案正文（页面折叠，仅得问题标题）；Simple Flight Academy 子站（https://simpleflightacademy.netlify.app/ ，未抓取，不影响核心结论）。
