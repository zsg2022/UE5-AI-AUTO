# GU5 AI Plugin — 需求规格说明书

> 版本：v1.1 | 日期：2026-05-08 | 作者：Claude 基于用户输入 + plan-review 修复后生成

---

## §1 目标与范围

### 问题定义

UE5 编辑器的核心功能（蓝图节点图编辑、关卡 Actor 摆放、布光、骨骼动画、材质编辑等）全部依赖手动 GUI 操作。AI 无法直接操控编辑器，导致开发者无法通过自然语言驱动 UE5 完成重复性/复杂性编辑任务。

### 业务级成功度量

开发者能在 Claude Code 中**纯自然语言**完成以下流程，全程不碰 UE5 编辑器 UI：

1. "在场景里创建一个 3m×3m 的蓝色方块，放在 (0,0,100) 位置" → 编辑器自动执行
2. "给这个方块加一个旋转组件，每秒转 90 度" → 蓝图节点自动生成并连线
3. "截图让我看看现在场景的样子" → 截图回传 Claude Code
4. "在方块上方加一个点光源，暖色" → 光源自动创建并调参

### 目标用户

| 属性 | 值 |
|------|-----|
| 角色 | UE5 独立开发者 |
| 技术水平 | C++/蓝图均熟练，UE5 日常用户 |
| 使用频率 | 每日多次 |
| 工作环境 | 本地 Win10，i9-9900/64GB/3090Ti |

### 干系人

- 直接用户（开发者本人）— 最终使用者与决策者
- 后续维护者（可能是开发者本人或接手的其他开发者）

### 交付物形态

| 交付物 | 形式 | 说明 |
|--------|------|------|
| GU5AI UE5 插件 | C++ 源码（放入项目 `Plugins/GU5AI/`） | 需随项目编译 |
| Python MCP Server | Python 源码 + `requirements.txt` | `pip install` 后通过 Claude Code `mcp add` 配置 |
| MCP 配置 | JSON 文件 | 填入 `.claude/mcp.json`，一行命令注册 |

### Non-Goals

| # | 不做 | 原因 |
|---|------|------|
| 1 | 运行时（游戏运行态）操控 | 首阶段仅编辑器模式；运行时模式架构预留，二期考虑 |
| 2 | AI 直接生成 C++ 源码文件 | AI 可以调用反射 API，不直接写 `.h/.cpp` 文件 |
| 3 | 独立 GUI 配置面板 | 插件在 UE5 中为后台通信模块，无自定义编辑面板 |
| 4 | 多用户并发控制 | 仅个人本地使用 |
| 5 | 移动端/Web 端远程操控 | 仅本地 Claude Code ↔ 本地 UE5 |
| 6 | 自动化测试框架 | 二期考虑 |
| 7 | 第三方插件签名认证与沙箱隔离 | 仅个人本地使用 |

⚠️ **默认假设声明**：本 SRS 有 **6/20 维度**基于合理默认值填补，未经用户逐项确认。涉及：N3（功能边界）、N9（兼容性范围）、N12（平台限定）、N15（安全性策略）、N16（可靠性期望）、N20（商业化——免费内部工具）。用户可在评审后调整。

### 前提条件

| 项目 | 约束 |
|------|------|
| UE 引擎 | UE 5.7（编辑器和编译工具链） |
| Python | 3.12+，含 `asyncio`/`websockets`/`fastmcp` |
| 操作系统 | Windows 10 x64 |
| Claude Code | 已安装可运行 |

---

## §2 用户故事与使用场景

### US01 — Actor 操控（P0，每天多次）

> 作为 UE5 开发者，我想要通过自然语言创建/修改/删除场景中的 Actor 和 Component，以便快速搭建关卡而不必手动从 Place Actors 面板拖拽。

**前置条件**：UE5 编辑器已打开 DSPlugin 工程，Python MCP Server 已启动（WebSocket Server 端），UE5 插件作为 WebSocket 客户端已连接。
**后置条件**：指定 Actor 出现在关卡中，或属性已修改，或已被删除。
**用户感知**：Claude Code 收到确认消息 + 可选截图验证。

- **主路径**：用户说 "创建一个 StaticMeshActor，用 Cube 模型，放在 (100, 200, 50)" → 自动执行 → 返回 "Actor 'Cube_01' 已创建，位置 (100,200,50)"
- **异常路径 A**：用户引用了不存在的资源（如 "用 MyMesh 模型" 但不存在）→ 返回 "错误：未找到资源 'MyMesh'。当前可用 StaticMesh 资源：Cube, Sphere, Plane, Cylinder"
- **异常路径 B**：用户指定的位置与其他 Actor 碰撞重叠 → 返回 "警告：指定位置与 'Wall_03' 重叠，已创建于最近安全位置 (100, 200, 80)"（或问用户）
- **错误恢复 A**：WebSocket 断连 → UE5 插件作为客户端自动重连（5s 内 3 次），恢复后重新握手，状态不丢失
- **错误恢复 B**：UE5 编辑器意外崩溃 → 下次启动后 UE5 插件自动重连；Python MCP Server 检测到连接恢复，状态重置

### US02 — 蓝图节点图编辑（P0，每天多次）

> 作为 UE5 开发者，我想要通过自然语言创建蓝图节点、设置引脚值、连线，以便快速构建游戏逻辑。

**前置条件**：目标蓝图资产已存在或 AI 可创建新蓝图。
**后置条件**：蓝图图中有新节点和/或连线，可编译通过。
**用户感知**：Claude Code 收到节点创建/连线确认 + 编译结果。

- **主路径**：用户说 "打开 BP_MyActor 蓝图，在 BeginPlay 事件后添加 PrintString 节点，打印 'Hello'" → 蓝图节点图出现节点和连线 → 编译通过
- **异常路径**：引脚类型不兼容（如把 float 连到 int 但无自动转换）→ 返回错误描述 + 建议修正方案
- **错误恢复**：蓝图编译失败 → 返回具体编译错误 + 由 AI 修正后重试

### US03 — 关卡操控（P0，每天多次）

> 作为 UE5 开发者，我想要控制关卡流送、World Outliner 操作、视口相机控制，以便管理大规模场景。

**前置条件**：关卡已加载。
**后置条件**：子关卡加载/卸载，Actor 在 Outliner 中位置改变，或视口切换到指定视角。
**用户感知**：Claude Code 收到确认 + 可选新视角截图。

- **主路径**：用户说 "把相机移到正上方俯视" → 视口相机移到 (0,0,1000) 俯视 → 返回截图
- **异常路径**：用户说 "加载 SubLevel_X" 但该关卡不存在 → 返回错误 + 列出可用子关卡

### US04 — 视觉反馈（SceneCapture）（P0，每天多次）

> 作为 UE5 开发者，我想要 AI 能"看到"编辑器视口的内容，以便 AI 做出基于视觉的判断。

**前置条件**：UE5 编辑器视口存在，SceneCapture2D 组件可用。
**后置条件**：视口截图编码为 JPEG/PNG 并通过 WebSocket 回传。
**用户感知**：Claude Code 显示截图（作为 MCP 工具返回的图片内容）。

- **主路径**：用户说 "截图" → 插件从编辑器活动视口捕获 → JPEG（质量 85）→ Base64 → 通过 MCP 返回 Claude Code
- **异常路径**：视口被遮挡或最小化 → 返回最后有效帧 + 警告

### US05 — 材质编辑（P1，每周）

> 作为 UE5 开发者，我想要通过自然语言创建和编辑材质、材质实例、材质函数，以便快速调整场景视觉。

**前置条件**：目标材质资产存在或可创建。
**后置条件**：材质节点图已修改，材质已编译。
**用户感知**：Claude Code 收到确认。

- **主路径**：用户说 "创建一个材质，BaseColor 用红色，Metallic=0.8，Roughness=0.3" → 材质资产创建 + 节点图配置完成

### US06 — 动画操控（P1，每周）

> 作为 UE5 开发者，我想要操控动画蓝图、状态机、Montage，以便设置角色动画逻辑。

**前置条件**：目标动画蓝图或骨架存在。
**后置条件**：状态机节点/转换已修改，动画蓝图已编译。
**用户感知**：Claude Code 收到确认。

- **主路径**：用户说 "在 ABP_Character 里加一个 Idle→Run 的过渡，条件是 Speed>0" → 状态机节点和过渡规则创建完成

### US07 — 反射 API 自动发现（P0，每天多次）

> 作为 UE5 开发者，我希望插件自动扫描并暴露 UE5 所有反射系统可用的类、函数和属性，而不是手工逐一封装每个 API。

**前置条件**：插件已启动，UE5 反射系统可用。
**后置条件**：所有 UCLASS/UEnum/UProperty/UFunction 被扫描并注册为 MCP 工具。
**用户感知**：Claude Code 的 MCP 工具列表包含数千个 UE5 API 函数，无需等待人工适配新功能。

- **主路径**：插件启动 → 反射扫描 → 生成 JSON schema → MCP Server 注册工具 → 可用
- **异常路径**：某个模块未加载（如 Niagara 未启用）→ 对应反射数据不出现，不影响其他模块

### US08 — 连接可靠性（P1，每周多次）

> 作为 UE5 开发者，我希望插件与 AI 之间的连接在异常断开后能自动恢复，避免已执行操作的上下文丢失。

**前置条件**：WebSocket/MCP 连接已建立。
**后置条件**：断连后自动重连恢复，状态同步。
**用户感知**：Claude Code 可能短暂显示 "重连中……"，恢复后继续使用。

- **主路径**：WebSocket 断开 → UE5 插件客户端检测 → 自动重连 Python MCP Server（指数退避 1s/2s/4s）→ 握手恢复
- **异常路径**：UE5 编辑器关闭（预期关闭）→ Python MCP Server 检测连接断开 → 标记状态为 DISCONNECTED，不无限等待重连

---

## §3 功能需求

| REQ-ID | 功能 | 来源 | P | fit criteria | 验证方法 |
|--------|------|:---:|:---:|------|------|
| REQ-COM-001 | WebSocket 通信：Python MCP Server 启动 WebSocket Server（`ws://127.0.0.1:{port}`），UE5 插件作为客户端连接 | US01,US08 | P0 | 连接 10s 内建立；支持 JSON 文本帧和二进制帧双通道；心跳 10s 间隔；单次命令超时 30s；读空闲 60s 断连 | 用 wscat 连接 Python Server 验证 |
| REQ-COM-002 | MCP Server：Python 侧实现 FastMCP，通过 stdio 与 Claude Code 通信，通过 WebSocket 与插件通信 | US01 | P0 | `claude mcp add` 后工具列表出现的函数数 ≥ 500 | 在 Claude Code 中验证工具列表 |
| REQ-COM-003 | 双向消息：UE5 插件→Python Server（事件通知/截图/响应），Python Server→UE5 插件（命令/查询），每条消息含唯一 request_id | US01,US04 | P0 | 请求-响应配对率 100%（无孤儿响应）；30s 超时未响应则标记 TIMEOUT | 100 次请求/响应对验证 |
| REQ-COM-004 | 自动重连：WebSocket 断开后 5s 内启动重连，3 次指数退避（1s/2s/4s），超过 3 次失败则停 | US08 | P1 | 断开到首次重连尝试 ≤5s；3 次失败后状态变为 DISCONNECTED | 手动断开网卡验证 |
| REQ-ACT-001 | 创建 Actor：通过 class_name 和 spawn_transform 创建任意 UClass Actor | US01 | P0 | 从接收命令到 Actor 在关卡中出现 ≤80ms（不含引擎加载类时间） | Profile+目视验证 |
| REQ-ACT-002 | 修改属性：读取/写入任意 UPROPERTY，含嵌套结构体和数组 | US01 | P0 | 修改单个 float 属性 ≤30ms | 批量 100 属性修改验证 |
| REQ-ACT-003 | 删除 Actor：通过名称或路径删除，不可恢复操作须确认 | US01 | P0 | 删除前返回确认请求（混合模式规则可配置） | 验证确认交互流程 |
| REQ-ACT-004 | 查询场景：列出当前关卡所有 Actor（名/类型/变换/组件列表） | US01,US03 | P0 | 100 Actor 场景查询 ≤100ms | 大场景压力测试 |
| REQ-BLU-001 | 创建蓝图节点：在指定蓝图的指定图中创建 UK2Node 节点并设置引脚默认值 | US02 | P0 | 从命令到节点出现在图中 ≤100ms | 自动化测试 |
| REQ-BLU-002 | 节点连线：在源引脚和目标引脚之间创建连线，验证类型兼容 | US02 | P0 | 类型不兼容时返回错误而非崩溃 | 类型不匹配输入验证 |
| REQ-BLU-003 | 编译蓝图：编译指定蓝图，返回成功/失败及错误列表 | US02 | P0 | 返回的错误信息含文件路径、行号、错误描述 | 注入语法错误验证 |
| REQ-LVL-001 | 关卡流送：加载/卸载子关卡 | US03 | P1 | 加载 ≤ 取决于关卡大小；卸载 ≤500ms | 大型子关卡验证 |
| REQ-LVL-002 | 视口相机：设置编辑器活动视口的相机位置/旋转/透视类型 | US03 | P0 | 从命令到视口刷新 ≤50ms | 目视+计时验证 |
| REQ-VIS-001 | 视口截图：从活动视口捕获 JPEG 截图 | US04 | P0 | 默认 1920×1080 JPEG Q=85；捕获+编码+传输总 ≤200ms | 计时验证 |
| REQ-VIS-002 | 截图可配置：分辨率/质量/格式（JPEG/PNG）可调 | US04 | P1 | 设置后下次截图立即生效 | 参数切换后验证 |
| REQ-MAT-001 | 创建材质：创建 Material/MaterialInstance 资产，设置标量/向量参数 | US05 | P1 | 从命令到资产出现在 Content Browser ≤500ms | 自动化测试 |
| REQ-MAT-002 | 编辑材质图：在材质编辑器图中创建/连接 MaterialExpression 节点 | US05 | P1 | 支持 ≥20 种常见 MaterialExpression 类型 | 逐一创建验证 |
| REQ-ANI-001 | 动画蓝图：查询/编辑动画蓝图中的状态机和动画图节点 | US06 | P1 | 支持状态添加/删除/过渡创建 | 自动化测试 |
| REQ-ANI-002 | 状态机过渡：创建状态过渡并设置 BlendSettings 和过渡条件 | US06 | P1 | 过渡条件基于 UAnimGraphNode 自动发现 | 验证过渡触发 |
| REQ-REF-001 | 反射扫描：插件启动时扫描所有已加载模块的 UCLASS/UEnum/UProperty/UFunction | US07 | P0 | 扫描完成 ≤10s；覆盖 ≥90% 已加载模块 | 对照 UE5 文档 API 数量验证 |
| REQ-REF-002 | 反射缓存：扫描结果缓存到磁盘（JSON），下次启动增量更新 | US07 | P1 | 增量扫描 ≤1s（vs 全量 10s） | 两次启动计时对比 |
| REQ-REF-003 | MCP 工具生成：将缓存的反射数据自动生成为 FastMCP tool 定义 | US07 | P0 | 生成的 tool 参数 schema 匹配 UFUNCTION 签名 | 抽样 100 个函数验证参数匹配 |

**§3↔§2 交叉校验**：每条 REQ 的 `来源` 列均指向 §2 用户故事。US01-US08 每个故事至少被一条 REQ 覆盖。

---

## §4 约束与假设

### 技术约束

| # | 约束 | P |
|---|------|:---:|
| C1 | UE 5.7 编译器（MSVC 2022），C++20 | P0 |
| C2 | Windows 10 x64；暂不支持其他平台 | P0 |
| C3 | 本地回环通信（127.0.0.1），不走公网 | P0 |
| C4 | Python 3.12+，MCP Server 依赖 `fastmcp`/`websockets` | P0 |
| C5 | 函数 ≤50 行，类 ≤300 行，嵌套 ≤3 层 | P0 |
| C6 | 所有权：动态内存用 `std::unique_ptr`/`shared_ptr`，禁裸指针管理所有权 | P0 |

### 法规/合规约束

无。本地个人工具，不涉及用户数据处理、云端传输或第三方服务。

### 组织约束

| # | 约束 |
|---|------|
| O1 | 开发者单人完成，无团队分工 |
| O2 | 上线时间：尽快可用（MVP 首版预计 2-4 周业余时间） |

### 假设清单

| # | 假设 | 风险 |
|---|------|------|
| A1 | UE 5.7 反射系统覆盖 ≥90% 编辑器操作 | 已知 Epic 有少量闭源 API |
| A2 | DSPlugin 测试工程可正常编译运行 | 已验证，风险低 |
| A3 | Python websockets 库性能满足 <100ms 往返 | 本地回环，理论可达 <5ms |
| A4 | FastMCP 框架稳定且支持动态注册/注销 tool | 需验证 |
| A5 | 用户机器的 UE5 编辑器和 Claude Code 同时在同一个桌面会话中运行 | 开发机配置，成立 |

### 冲突消解规则

当约束冲突时以以下顺序为准：**C3（本地安全） > C1（编译通过） > C5（代码规范） > 性能目标**。

---

## §5 数据需求

### 持久化信息

| 数据 | 生命周期 | 大致规模 | 一致性 |
|------|----------|----------|--------|
| 反射缓存（JSON schema） | 插件安装期间；更新引擎版本后重建 | 万条级（~5MB JSON） | 写入后 fsync |
| 连接配置（端口号、重连参数） | 等同于插件生命周期 | 条数级 | 无并发冲突 |
| 用户偏好（确认模式：always/auto/混合） | 用户手动修改时更新 | 条数级 | 无并发冲突 |

### 数据流路径

```
用户自然语言输入
  → Claude Code 调用 MCP tool（通过 MCP/stdio）
    → Python MCP Server 解析 tool 调用
      → 通过 WebSocket（Python=Server, UE5=Client）发送 JSON 命令到 UE5 插件
        → UE5 插件执行（反射系统调用实际 API）
      ← 执行结果（JSON）+ 可选截图（二进制帧）通过同一 WebSocket 连接返回
    ← MCP Server 包装为 tool result
  ← Claude Code 显示结果给用户
```

> **架构说明**：Python MCP Server 启动 `asyncio` WebSocket Server（默认端口 9876），UE5 插件通过 `FWebSocketsModule::Get().CreateWebSocket()` 主动连接。WebSocket 为全双工通道，命令下发和结果回传共用同一连接。

### 输入校验规则

| 输入 | 合法范围 | 非法时行为 |
|------|----------|------------|
| Actor class_name | 当前已加载模块中存在的 UCLASS | 返回错误 + 可选相近名称建议 |
| 位置坐标 (X,Y,Z) | -1e6 ~ 1e6（UE5 FVector 合理范围） | 拒绝执行 + 返回范围提示 |
| 属性值类型 | 与 UPROPERTY 声明类型匹配 | 返回类型错误 + 期望类型 |
| 节点引脚连接 | 源引脚类型可隐式转换为目标引脚类型 | 返回类型不兼容错误 |
| 文件路径 | 相对于 `{Project}/Saved/GU5AI/`，拒绝含 `..`、`/` 开头的绝对路径、盘符路径（如 `C:\`） | 返回错误 "路径非法：仅允许项目 Saved/GU5AI 子目录" |

### 配置层级

配置加载优先级（高→低）：

| 优先级 | 来源 | 示例 |
|:---:|------|------|
| 1 | 命令行参数 | `python mcp_server.py --port 9877` |
| 2 | 环境变量 | `GU5AI_PORT=9877` |
| 3 | 项目配置文件 | `{Project}/Saved/GU5AI/config.json` |
| 4 | 默认值 | 端口 9876，重连 3 次，心跳 10s |

### 反射缓存策略

| 属性 | 值 |
|------|-----|
| 缓存文件 | `{Project}/Saved/GU5AI/reflection_cache.json` |
| 缓存 key | `{UE版本号}_{已加载模块列表MD5}` |
| 失效条件 | UE 版本变更、模块列表变化（装/卸插件）、手动清除 |
| 容量上限 | ≤10MB（JSON）；超出则按模块名排序截断 |
| 线程安全 | Python MCP Server 单线程读；写时持有 GIL；不加锁 |

### 外部数据交换

| 接口 | 格式 | 编码 | 超时 |
|------|------|------|:---:|
| WebSocket 命令 | JSON：`{"id":"uuid","method":"...","params":{}}` | UTF-8 | 单次命令 30s |
| WebSocket 响应 | JSON：`{"id":"uuid","result":{...},"error":null}` | UTF-8 | — |
| 视口截图 | JPEG（默认）或 PNG | 二进制 WebSocket 帧，前 4 字节为大小 | 30s |
| WebSocket 心跳 | PING/PONG（RFC 6455） | — | 10s 间隔 |

---

## §6 非功能需求

### ISO 25010 八大维度

#### 功能适用性

| 子维度 | 回答 |
|--------|------|
| 功能完整性 | P0 功能（通信+反射+Actor操控+蓝图+截图）覆盖核心链路；P1（材质+动画+缓存）增强体验 |
| 正确性 | 每个 API 调用直接对等 UE5 C++ 函数，正确性与引擎行为一致 |
| 功能恰当性 | 不做超过开发者需要的（不生成 C++ 源码、不跑 AI 推理） |

#### 性能效率

| 指标 | 目标值 | 度量方法 |
|------|--------|----------|
| 简单命令往返（创建Actor） | ≤80ms 引擎侧 + ≤20ms 传输 = **≤100ms 端到端** | 计时：tool调用→返回 |
| 视口截图（1920×1080 JPEG Q=85）| 捕获≤30ms + 编码≤120ms + 传输≤50ms = **≤200ms** | 计时：capture→MCP return |
| 视口截图（3840×2160 JPEG Q=85）| 允许放宽至 ≤500ms | 高分辨率压力测试 |
| 反射扫描（全量） | **≤10s** | 启动日志 |
| 反射扫描（增量） | **≤1s** | 启动日志 |
| 100 Actor 场景查询 | **≤100ms** | Profile |
| WebSocket 心跳间隔 | **10s** | 抓包验证 |
| 单次命令超时 | **30s**（含引擎侧耗时操作） | 超时后返回 TIMEOUT error |
| 读空闲超时 | **60s** | 无消息后主动 PING |
| MCP Server 内存驻留 | **≤200MB** | 任务管理器 |

#### 目标用户最低配置声明

| 组件 | 最低要求 | 备注 |
|------|----------|------|
| CPU | ≥4 核 3.0GHz | 与 UE5 编辑器最低要求一致 |
| 内存 | ≥32GB | UE5 编辑器本身推荐 32GB+ |
| GPU | ≥RTX 2060 / 6GB VRAM | UE5 编辑器视口渲染需求 |
| 磁盘 | SSD，≥10GB 可用 | 插件本体 <100MB，其余为 UE5 工程 |

#### 兼容性

| 子维度 | 回答 |
|--------|------|
| 共存性 | 与 ModelingToolsEditorMode、StateTree 等已有插件无冲突 |
| 互操作性 | MCP 协议标准兼容，WebSocket RFC 6455 兼容 |
| 向后兼容 | 无需兼容旧版本（UE 5.7 only） |

#### 可用性

| 子维度 | 回答 |
|--------|------|
| 可学习性 | 用户即为自己，无需学习材料；操作方式为自然语言，学习成本≈0 |
| 错误防护 | 高风险操作（删除Actor、修改蓝图）需确认；不可逆操作前提示 |
| 可访问性 | N/A — 通过 Claude Code 间接操控，无自有 GUI |

#### 可靠性

| 子维度 | 回答 |
|--------|------|
| 成熟性 | 首版为 MVP，允许偶发非致命错误 |
| 可用性 | 编辑期间可用即可，非 7×24 服务 |
| 容错性 | WebSocket 断连自动重试（3 次）；无效命令返回错误而非崩溃 |
| 可恢复性 | 编辑器崩溃后重启恢复（状态重置，不丢持久化配置） |

#### 安全性

| 子维度 | 回答 |
|--------|------|
| 机密性 | 本地回环 127.0.0.1，无外部网络暴露 |
| 完整性 | 无身份认证（本地单人）；命令 schemas 校验防止畸形输入 |
| 抗抵赖性 | N/A |
| 可审计性 | 命令日志（可配置开关），记录所有操作 + 时间戳 |
| 真实性 | WebSocket 限定 127.0.0.1，外部无法连接 |

#### 可维护性

| 子维度 | 回答 |
|--------|------|
| 模块化 | 三层物理隔离：Python MCP Server / UE5 C++ Plugin / 测试工程 |
| 可重用性 | 反射扫描模块可独立用于其他 UE5 工具 |
| 易分析性 | 统一日志格式：`[模块] [级别] 消息` |
| 易修改性 | 新增 UE5 功能通过反射自动暴露，无需改 Python 或 C++ 代码 |
| 易测试性 | 每个 MCP tool 可独立测试；UE5 侧提供命令行测试接口 |

#### 可移植性

| 子维度 | 回答 |
|--------|------|
| 适应性 | Win10 x64 only；UE 5.7 限定 |
| 易安装性 | 插件拷贝到项目 Plugins 目录 → 编译 → `pip install -r requirements.txt` → 配置 MCP |
| 易替换性 | 插件为独立模块，删除即恢复原状 |

### NFR 冲突优先级

当性能与功能冲突时：**功能正确性 > 安全（本地限 127.0.0.1）> 性能 > 功能完整性**

### 数值锚点汇总

| 指标 | 锚点值 | 度量时机 |
|------|--------|----------|
| 简单命令端到端延迟 | ≤100ms | 每次调用 |
| 视口截图延迟（1920×1080） | ≤200ms | 每次截图 |
| 视口截图延迟（3840×2160） | ≤500ms | 高分辨率截图 |
| 全量反射扫描 | ≤10s | 启动/模块重载 |
| 增量反射扫描 | ≤1s | 热重载 |
| 重连启动延迟 | ≤5s | 断连后 |
| WebSocket 心跳间隔 | 10s | 持续 |
| 单次命令超时 | 30s | 每次命令 |
| 读空闲超时 | 60s | 空闲连接 |
| 100 Actor 查询 | ≤100ms | 场景查询 |
| 崩溃恢复 | 完全重置 | 崩溃后重启 |
| MCP 暴露 API 数（首版） | ≥500 | 启动后 |
| 反射缓存文件上限 | ≤10MB | 启动/热重载 |

---

## §7 外部接口需求

### 7.1 用户界面

> ⏭️ 不适用。本插件无自有 GUI——用户通过 Claude Code 的文本/图像界面交互，UE5 编辑器本身为被操控目标而非插件界面。GUI 设计不属于本需求范围。

### 7.2 文件接口

| 接口 | 格式 | 读/写 | 说明 |
|------|------|--------|------|
| 反射缓存 | JSON | R/W | `{Project}/Saved/GU5AI/reflection_cache.json` |
| 用户配置 | JSON | R/W | `{Project}/Saved/GU5AI/config.json` |
| 操作日志 | 纯文本 | W | `{Project}/Saved/GU5AI/command_log.txt`，可配置 |

### 7.3 程序间接口

| 接口 | 方向 | 协议 | 说明 |
|------|------|------|------|
| Claude Code → Python MCP Server | 入 | MCP/stdio（JSON-RPC 2.0） | Claude Code 调用 `gu5_*` 工具 |
| Python MCP Server ↔ UE5 Plugin | 双向 | WebSocket（JSON + Binary） | Python 为 **Server**（`ws://127.0.0.1:9876`），UE5 为 **Client**（`FWebSocketsModule::Get().CreateWebSocket()`）；心跳 10s；命令超时 30s；读空闲 60s 断连 |
| UE5 Plugin → 编辑器子系统 | 入 | 内部反射调用 | 通过 UE5 反射系统（Game Thread） |

> **WebSocket 方向依据**：UE5 内置 `WebSockets` 模块（[官方文档 5.7](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/WebSockets)）提供 `IWebSocket` 客户端 API。创建服务端需额外库。因此 Python 侧用 `asyncio + websockets` 做 Server（三行代码启动），UE5 用稳定内置 API 连接。

---

## §8 依赖与风险

### 依赖

| 依赖 | 版本 | 类型 | 许可证 | 引入理由 |
|------|------|------|--------|----------|
| Unreal Engine | 5.7 | 引擎 | Epic EULA | 目标平台 |
| Python | 3.12+ | 运行时 | PSFL | MCP Server |
| FastMCP | latest | Python 库 | MIT | MCP 协议实现 |
| websockets | latest | Python 库 | BSD-3 | WebSocket 客户端 |
| UE5 WebSockets 模块 | 5.7 内置 | 引擎插件 | Epic EULA | C++ 侧 WebSocket 客户端（连接 Python Server）|

### 风险

| # | 风险 | 概率 | 影响 | 缓解 | 回滚方案 |
|---|------|:---:|:---:|------|------|
| R1 | UE5.7 反射系统对某些子系统覆盖不全 | 中 | 中 | 先扫描验证覆盖率，不足的手工补包装 | 对手工包装的函数加 `REQ-REF-004` 补充接口 |
| R2 | WebSocket 在 UE5 编辑器中性能不及预期 | 低 | 中 | 本地回环，理论延迟 <1ms；压力测试 | 降级为 Named Pipe 通信 |
| R3 | FastMCP 动态注册 tool 数量限制 | 低 | 高 | 调研 FastMCP 上限；必要时按模块分类注册 | 改用原始 MCP SDK 手写 tool 注册 |
| R4 | 编辑器 Tick 中执行反射调用阻塞主线程导致卡顿 | 中 | 中 | 耗时操作放到 Editor Task（异步），强制超时 | 改为同步模式 + 性能警告 |
| R5 | 蓝图图编辑 API（UEdGraph）在 5.7 中签名变更 | 低 | 高 | 查阅 Epic 官方 API 文档；编译时自动检测 API 存在性 | 条件编译或降级为不支持蓝图编辑 |
| R6 | 用户机器资源不足导致扫描/截图性能差 | 低 | 低 | 截图分辨率可降级；扫描改为懒加载 | N/A |

---

## §9 Prior Art

### 同类方案对比

| 方案 | 核心思路 | 优势 | 劣势 | 与本需求适配度 |
|------|----------|------|------|:---:|
| **jl-codes/unreal-5-mcp** | C++ 插件 (TCP 55557) + Python FastMCP | 活跃维护；支持蓝图编辑 | ~20 个工具，全部手工包装；TCP 方式需管理端口 | 中 — 可参考架构，但需增强反射发现 |
| **GenOrca/unreal-mcp** | 插件内嵌 Python + MCP，62 个手工工具 | 功能最多（含行为树）；Fab 发售 | 全部手工包装，新增功能需改代码；UE 内嵌 Python 有版本限定 | 中 — 工具设计思路可参考 |
| **@runreal/unreal-mcp** | 纯 Node.js MCP Server，用 UE 内置 Python Remote Execution | 零 C++ 编译，`npx` 即用；安装极简 | 依赖 UE Python 插件；功能受限于 Python API；无法访问低层 C++ API | 低 — 受限太多，不适合"全程操控"目标 |
| **UnrealGenAISupport** | C++ 插件同时支持 MCP + 直连 LLM API | 最"野心"；多模型支持 | 蓝图节点连接有 bug；无撤销 | 低 — 目标不同（它把 LLM 拉到 UE 里 vs 我们把 UE 暴露给 Claude Code） |
| **本需求（GU5 AI Plugin）** | 反射自动发现 + 混合粒度 MCP 工具 + WebSocket 双向通信 | 自动暴露全引擎 API（非手工包装）；混合粒度（高层语义+底层原子）；Claude Code 原生集成 | 首版功能范围大；反射扫描性能需验证 | **本需求** |

### 定位差异

本需求的独特定位：**不是"在 UE5 里加个 AI 对话框"，而是"让 Claude Code 成为 UE5 的万能操控终端"**。核心差异化：

1. **反射自发现优先**：不手工写第 63 个工具的代码，而是让引擎自己告诉 AI 它有什么 API
2. **Claude Code 原生**：设计目标就是 Claude Code 调用，而非通用 MCP 客户端
3. **视觉闭环**：AI 能看到场景 —— 这是所有现有方案都缺失的能力

---

## §10 术语表

| 术语 | 定义 | 备注 |
|------|------|------|
| MCP | Model Context Protocol，AI 客户端与外部工具间的标准通信协议 | 基于 JSON-RPC 2.0 |
| WebSocket | 全双工 TCP 通信协议，RFC 6455 | 本项目中用于 MCP Server ↔ UE5 插件 |
| 反射系统 | UE5 的 UHT 生成的类型元数据系统 | 含 UCLASS/UEnum/UProperty/UFunction |
| Actor | UE5 中可放置到关卡中的对象基类 | AActor 及其派生类 |
| Component | 附加到 Actor 上的功能模块 | UActorComponent 及其派生类 |
| 蓝图 | UE5 的可视化脚本系统 | 含 UBlueprint 和 UEdGraph |
| MCP Server | 实现 MCP 协议的服务端，暴露 tools/resources/prompts | 本项目中为 Python 进程 |
| MCP tool | MCP Server 暴露的可调用函数 | 封装为 JSON Schema 描述的函数 |
| DSPlugin | 测试用 UE5 工程名称 | 位于 `K:\GU5 AI Plugin\D\DSPlugin` |
| FastMCP | Python 的高层 MCP 框架 | 封装了底层 JSON-RPC 细节 |

---

## §11 验收策略

### 整体验收标准

1. **所有 P0 REQ fit criteria 通过**（共 14 条 P0 需求）
2. **≥80% P1 REQ fit criteria 通过**（共 7 条 P1 需求）
3. **关键演示场景全部走通**（见下方）
4. **插件连续运行 2h 无崩溃、无内存泄漏**
5. **Claude Code 中调用任意已注册 MCP tool，返回结果符合预期**

### 验收环境

- 同开发环境：Win10 x64 / i9-9900 / 64GB / 3090Ti
- UE 5.7 DSPlugin 工程
- Claude Code 最新版
- 插件从 `Plugins/GU5AI/` 加载

### 关键验收演示场景

**场景 A — "零触碰创建场景"**

```
1. 启动 UE5 编辑器，打开 DSPlugin 工程
2. 在 Claude Code 中键入："在场景中心创建一个 2m 的红色方块，上方 1m 处放一个点光源（暖色 3000K）"
3. 预期：两个 Actor 自动出现在关卡中，无需任何手动操作
4. 在 Claude Code 中键入："截图"
5. 预期：返回编辑器视口截图，可看到红色方块和暖色光源
```

**场景 B — "蓝图逻辑编辑"**

```
1. 在 UE5 中新建空白蓝图 Actor，命名为 BP_Test
2. 在 Claude Code 中键入："打开 BP_Test 的 Event Graph，在 BeginPlay 后添加 PrintString 节点，内容 'Hello from AI'"
3. 预期：蓝图图中出现 BeginPlay → PrintString 连线
4. 执行编译 → 返回编译成功
```

**场景 C — "反射自发现与万能调用"**

```
1. 插件启动后，在 Claude Code 中调用"列出所有可用 UE5 函数"
2. 预期：返回 ≥500 个函数，覆盖 Actor、Component、Level、Blueprint 等模块
3. 手动挑选一个未预封装的函数，直接调用 → 返回正确结果
```

### 未覆盖项

- 运行时（游戏态）操控 → 二期
- 多用户/远程操控 → 非目标
- Niagara VFX → 架构支持但首版不验证
- Landscape/PCG → 反射覆盖但首版不验证

---

## §12 架构考量与设计建议

> ⚠️ 本节所有内容为专业建议，非需求约束。设计者可采纳或选择更优方案。

### 12.1 架构风格推荐

**推荐：分层插件架构（3 层）**

#### L1 系统上下文图

```
┌─────────────────────────────────────────────────────────────────┐
│                      开发者工作站 (Win10 x64)                      │
│                                                                   │
│   ┌──────────┐     MCP/stdio      ┌─────────────────┐            │
│   │          │◄──────────────────►│                 │            │
│   │Claude Code│   (JSON-RPC 2.0)  │ Python MCP Server│            │
│   │(MCP Client)│                   │ (FastMCP)       │            │
│   │          │                    │                 │            │
│   └──────────┘                    └────────┬────────┘            │
│                                            │                      │
│                                     WebSocket                     │
│                                 ws://127.0.0.1:9876               │
│                              (Python=Server, UE5=Client)          │
│                                            │                      │
│   ┌────────────────────────────────────────┼──────────────────┐  │
│   │                        UE5 编辑器       │                   │  │
│   │  ┌─────────────────────────────────────▼─────────────────┐ │  │
│   │  │               GU5AI C++ Plugin                         │ │  │
│   │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │ │  │
│   │  │  │ WebSocket    │  │ 反射扫描引擎  │  │ 命令执行器   │ │ │  │
│   │  │  │ Client       │  │ (Reflection   │  │ (Command     │ │ │  │
│   │  │  │ (IWebSocket) │  │  Scanner)     │  │  Executor)   │ │ │  │
│   │  │  └──────────────┘  └──────────────┘  └──────────────┘ │ │  │
│   │  └───────────────────────────────────────────────────────┘ │  │
│   │                                                            │  │
│   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │  │
│   │  │ Actor    │  │ Blueprint│  │ Level    │  │ Viewport │  │  │
│   │  │ System   │  │ Editor   │  │ Manager  │  │ Capture  │  │  │
│   │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │  │
│   └───────────────────────────────────────────────────────────┘  │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

#### L2 容器图（数据流视角）

```
Claude Code                    Python MCP Server              UE5 Editor (GU5AI Plugin)
═══════════                    ═════════════════              ════════════════════════

MCP Client                     FastMCP App                    UEditorSubsystem
    │                              │                              │
    │  tool call (stdio)           │                              │
    ├─────────────────────────────►│                              │
    │                              │                              │
    │                   ┌──────────┴──────────┐                   │
    │                   │  Tool Registry       │                   │
    │                   │  (反射缓存→tool schema)│                   │
    │                   └──────────┬──────────┘                   │
    │                              │                              │
    │                   WebSocket Server                          │
    │                   (asyncio, ws://127.0.0.1:9876)            │
    │                              │                              │
    │                   ┌──────────┴──────────┐                   │
    │                   │  Request Manager     │                   │
    │                   │  - pending 队列      │                   │
    │                   │  - 超时计时器(30s)   │                   │
    │                   │  - 心跳定时器(10s)   │                   │
    │                   └──────────┬──────────┘                   │
    │                              │                              │
    │                              │  WebSocket JSON cmd          │
    │                              ├─────────────────────────────►│
    │                              │                              │
    │                              │                   ┌──────────┴──────────┐
    │                              │                   │ FWebSocketClient     │
    │                              │                   │ (IWebSocket,         │
    │                              │                   │  FWebSocketsModule)  │
    │                              │                   └──────────┬──────────┘
    │                              │                              │
    │                              │                   ┌──────────┴──────────┐
    │                              │                   │ TQueue<FCommandCtx>  │
    │                              │                   │ (Mpsc, 线程安全)     │
    │                              │                   └──────────┬──────────┘
    │                              │                              │
    │                              │                   ┌──────────┴──────────┐
    │                              │                   │ Game Thread Executor │
    │                              │                   │ (AsyncTask dispatch) │
    │                              │                   └──────────┬──────────┘
    │                              │                              │
    │                              │                   ┌──────────┴──────────┐
    │                              │                   │  Reflection Engine   │
    │                              │                   │  (UClass/UFunction   │
    │                              │                   │   UProperty scan)    │
    │                              │                   └──────────┬──────────┘
    │                              │                              │
    │                              │                   ┌──────────┴──────────┐
    │                              │                   │ Actual UE5 API call  │
    │                              │                   │ (Actor/BP/Level/...) │
    │                              │                   └──────────┬──────────┘
    │                              │                              │
    │                              │  WebSocket JSON result       │
    │                              │◄─────────────────────────────┤
    │                              │  + binary frame (screenshot) │
    │                              │                              │
    │                   ┌──────────┴──────────┐                   │
    │                   │  Result → MCP tool   │                   │
    │                   │  return packaging    │                   │
    │                   └──────────┬──────────┘                   │
    │                              │                              │
    │  tool result (stdio)         │                              │
    │◄─────────────────────────────┤                              │
    │                              │                              │

═══════════                    ═════════════════              ════════════════════════
传输层                           协议层                         引擎层
```

理由：
- **物理隔离**：Python 和 C++ 各自独立，修改一端不影响另一端
- **协议标准化**：MCP 和 WebSocket 均为标准协议，便于调试和替换
- **职责单一**：Python 只做协议翻译、工具注册、超时管理、心跳保活；C++ 只做引擎调用和反射扫描
- **线程安全**：WebSocket 线程通过 Mpsc 队列投递到 Game Thread，禁止跨线程直接操作 UObject
- **连接方向**：Python 做 Server（asyncio 天然适合 IO 密集型服务），UE5 做 Client（使用内置稳定 `IWebSocket` API，避免引入实验性模块）

### 12.2 并发与线程安全

**隐含异步场景**：
- UE5 插件通过 `IWebSocket` 在后台线程接收 Python Server 发来的命令
- 所有 UE5 反射调用必须在 Game Thread 执行
- 截图捕获可能在 Render Thread 完成
- Python MCP Server 维持 asyncio 事件循环（单线程），管理超时和心跳

**推荐线程模型（UE5 侧）**：
```
Python Server ──WebSocket──► IWebSocket Thread → TQueue(Mpsc) → Game Thread (AsyncTask)
    ▲                                                                   ↓
    ◄────────── WebSocket 结果回传 ◄──── 结果出队 ◄─────── 执行完成
```

- **命令队列**：线程安全 `TQueue<FCommandContext, EQueueMode::Mpsc>`（多生产者单消费者）
- **取消机制**：每个命令带 `FCancellationToken`，30s 超时自动取消，Game Thread 执行前检查
- **跨线程通信**：`AsyncTask(ENamedThreads::GameThread, ...)` 将命令从 WebSocket 线程投递到 Game Thread
- **心跳管理**：UE5 侧利用 `IWebSocket` 的 PING/PONG 自动回复（RFC 6455），Python 侧主动发送 PING

### 12.3 内存与指针安全（C++ 强制）

| 对象类型 | 所有权 | 传递方式 | 说明 |
|----------|--------|----------|------|
| UObject 派生类 | UE5 GC（FGCObject / TWeakObjectPtr） | 裸指针（UE5 约定） | UObject 由 UE5 GC 管理，不额外持有 |
| IWebSocket 实例 | 插件模块（`TSharedPtr<IWebSocket>`） | TSharedPtr | UE5 约定智能指针，引用计数管理 |
| 命令上下文 | 命令队列（`std::unique_ptr`） | std::move | 生产者→消费者，所有权转移 |
| 反射缓存 | Python MCP Server 侧（Python `dict`） | 值传递 | Python 侧，不跨进程 |

**跨 DLL 规则**：C++ 插件编译为 `.dll` 由 UE5 加载。`IWebSocket` 实例通过 `FWebSocketsModule` 工厂创建（返回 `TSharedPtr`），生命周期自动管理。不向 DLL 外部暴露裸指针。

### 12.4 数据结构关键提醒

- **反射索引**：用 `TMap<FName, FReflectedFunctionInfo>` 按函数名索引，O(1) 查找
- **命令上下文**：`struct FCommandContext { FGuid Id; FName Method; TSharedPtr<FJsonObject> Params; FCancellationToken Token; }`
- **响应**：`struct FCommandResult { FGuid Id; TSharedPtr<FJsonObject> Result; FString Error; TArray<uint8> BinaryAttachment; }`

### 12.5 异常与容错策略

**可恢复错误**：
- 无效 class_name → 返回错误 + 建议
- 属性类型不匹配 → 返回类型错误
- 蓝图编译失败 → 返回编译错误详情
- WebSocket 消息解析失败 → 丢弃 + 日志 + 返回 MALFORMED_JSON error

**不可恢复错误**：
- 插件初始化失败（端口占用等）→ 日志 + 弹窗通知
- 反射系统不可用 → 插件降级为手工 API 模式

**推荐错误码**：
```
E_GU5_OK           = 0
E_GU5_INVALID_ARG  = -1
E_GU5_NOT_FOUND    = -2
E_GU5_TYPE_MISMATCH= -3
E_GU5_TIMEOUT      = -4
E_GU5_INTERNAL     = -99
```

**日志最低要求**：`[GU5AI] [模块] [级别] [request_id] 消息` — 含时间戳、级别、函数名、关键参数。

### 12.6 技术风险预警

| 风险 | 级别 | 建议 |
|------|:---:|------|
| 🔴 反射扫描性能对超大项目的影响 | 高 | **PoC 先行**：在 DSPlugin 工程运行反射扫描，测量耗时和内存 |
| 🔴 蓝图图编辑 API 稳定性 | 高 | **先读官方文档**：验证 UEdGraph/UEdGraphNode/UEdGraphPin API 在 5.7 的可用性 |
| 🟡 WebSocket Client 在编辑器中的生命周期 | 中 | 利用 `UEditorSubsystem` 自动管理启停；Initialize() 连接，Deinitialize() 断开（[官方文档 5.6+](https://dev.epicgames.com/documentation/unreal-engine/programming-subsystems-in-unreal-engine)） |
| 🟡 MCP tool 数量爆炸（≥5000 个）| 中 | 按模块分类 + 命名空间隔离；Claude Code 工具选择机制可处理 |
| 🟢 视口截图与 Viewport 的交互 | 低 | 编辑器模式使用 `GetViewportScreenShot()`；`FScreenshotRequest::RequestScreenshot` + `OnScreenshotCaptured` delegate 作为备选 |

### 12.7 下游输入清单

| SRS 章节 | 关键需求 | 对方案的影响 | 方案应关注 |
|----------|----------|-------------|-----------|
| §3 REQ-COM | WebSocket + MCP 双层架构 | 架构骨架，决定分层 | 线程模型、消息序列化、错误传递 |
| §3 REQ-REF | 反射自动扫描 + MCP tool 生成 | 决定插件核心价值 | 反射遍历策略、缓存设计、tool schema 生成算法 |
| §3 REQ-ACT | Actor CRUD | 验证链路的基础功能 | 命令队列、Game Thread 执行、属性反射 |
| §3 REQ-BLU | 蓝图图编辑 | 最复杂交互 | UEdGraph API、引脚兼容性检查、编译流程 |
| §3 REQ-VIS | 视口截图 | 视觉闭环 | 视口捕获 API、编码传输 |
| §6 性能 | 100ms 端到端延迟 | 对序列化/传输/线程切换有严格要求 | 零拷贝传输、二进制帧分离、本地回环优化 |
| §8 R5 | 蓝图 API 可能变更 | 需要条件编译和 API 验证 | `#if UE_VERSION` 和运行时 API 可用性检查 |
