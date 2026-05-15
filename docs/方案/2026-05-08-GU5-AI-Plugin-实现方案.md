# GU5 AI Plugin 实现方案

| 元数据 | 值 |
|--------|-----|
| 版本 | 1.1 |
| 生成时间 | 2026-05-08（修订） |
| 技能版本 | implementation-plan-writer |
| plan-review 状态 | ✅ 已通过（审查报告 v1.1，全部问题已修复） |
| 参考 SRS | 需求规格/GU5 AI Plugin-需求规格说明书.md v1.1 |

**项目分类**：

| Q1 库 | Q2 GUI | Q3 网络 | Q4 数据库 | Q5 性能 | Q6 线程 |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 是 | 否 | 是 | 否 | 是 | 是 |

**加载板块**：架构与设计(01) + 质量保障(02) + 基础设施运维(03) + 数据与持久化(04) + 集成与互操作(05) + 并发与分发(06) + AUTOCAP 特化(Q1触发)

**核心价值主张**：让 Claude Code 成为 UE5 编辑器的万能操控终端——通过 MCP + WebSocket + 反射自发现，AI 可以调用引擎全部编辑器 API，无需人工逐一手工包装。

---

## 非目标（Non-Goals）

| # | 不做 | 原因 |
|---|------|------|
| 1 | 独立 GUI 配置面板 | 插件为后台通信模块，无自有 GUI |
| 2 | 运行时模式（Packaged Game） | 仅编辑器；运行时段架构预留但本方案不实现 |
| 3 | C++ 源码生成 | AI 不直接生成 .h/.cpp 文件 |
| 4 | 多用户并发 | 仅单人本地使用 |
| 5 | 自动化测试框架（CI） | 个人项目，手动验收 |

---

## 架构决策

| 编号 | 决策 | 理由 |
|------|------|------|
| ADR-001 | **Python 做 WebSocket Server，UE5 做 Client** | UE5 内置 `WebSockets` 模块仅提供 `IWebSocket` 客户端 API（[官方文档 5.7](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/WebSockets)）。Python `asyncio + websockets` 天然适合做 IO 密集的 Server。避免引入 UE5 实验性服务端插件 |
| ADR-002 | **UEditorSubsystem 管理插件生命周期** | [官方文档 5.6+](https://dev.epicgames.com/documentation/unreal-engine/programming-subsystems-in-unreal-engine) 确认 `Initialize()`/`Deinitialize()` 稳定跨版本。引擎自动管理实例创建和销毁，无需手动维护 |
| ADR-003 | **反射自动发现 → MCP 动态注册** | 核心差异化。`TObjectIterator<UClass>` + `TFieldIterator<UFunction>` 遍历全部已加载模块的反射数据。Python 侧用 FastMCP `add_tool()` 动态注册。新增 UE5 功能无需改 Python/C++ 代码 |
| ADR-004 | **TQueue<EQueueMode::Mpsc> + AsyncTask 线程模型** | WebSocket 线程接收命令 → Mpsc 队列 → `AsyncTask(ENamedThreads::GameThread)` 投递。所有 UObject 操作在 Game Thread 执行，避免跨线程访问崩溃 |
| ADR-005 | **JSON-RPC 风格命令协议** | 格式：`{"id":"uuid","method":"create_actor","params":{...}}`。响应：`{"id":"uuid","result":{...},"error":null}`。二进制帧用于截图传输 |
| ADR-006 | **FastMCP 框架** | 官方 MCP Python SDK 的高层封装。支持 `listChanged` 通知，可运行时注册/注销工具。装饰器语法简洁 |
| ADR-007 | **新建 Editor 模块，不动现有 Runtime 模块** | DSPlugin 现有模块为 Runtime 类型。新建独立的 `GU5AI` Editor 模块，不修改 DSPlugin 源文件，降低侵入性 |
| ADR-008 | **配置优先级: CLI > ENV > config.json > 默认值** | 灵活且符合 12-Factor App 原则。端口默认 9876，可通过 `GU5AI_PORT` 覆盖 |
| ADR-009 | **截图用 JPEG 默认 Q=85，二进制帧传输** | WebSocket 原生支持二进制帧。JPEG 编码 1920×1080 下 ≤120ms（VP8/VP9 评估后选定）。首 4 字节为 payload 大小 |
| ADR-010 | **混合粒度工具：高层语义 + 低层反射** | 语义工具（`create_actor`）提供便捷操作；反射工具（`gu5_UStaticMeshComponent_SetStaticMesh`）提供万能兜底。两者共存不互斥 |

---

## 数值锚点

| SRS 要求 | 代码实现位置 | 状态 |
|---------|------------|------|
| 简单命令端到端 ≤100ms | `GU5AICommandExecutor::ExecuteCommand()` → `AsyncTask` | ⬜ 待验证 |
| 视口截图 ≤200ms @ 1920×1080 | `GU5AIScreenshotCapture::CaptureViewport()` | ⬜ 待验证 |
| 视口截图 ≤500ms @ 3840×2160 | 同上，高分辨率分支 | ⬜ 待验证 |
| 全量反射扫描 ≤10s | `GU5AIReflectionScanner::ScanAllModules()` | ⬜ 待验证 |
| 增量反射扫描 ≤1s | `GU5AIReflectionScanner::ScanIncremental()` | ⬜ 待验证 |
| WebSocket 端口 9876 | `config.py` → `DEFAULT_PORT` | ⬜ 待验证 |
| 心跳间隔 10s | `GU5AIWebSocketClient::StartHeartbeat()` + `ws_server.py` | ⬜ 待验证 |
| 命令超时 30s | `GU5AICommandExecutor` → `FCancellationToken` | ⬜ 待验证 |
| 重连 ≤5s，指数退避 1/2/4s，3次 | `GU5AIWebSocketClient::Reconnect()` | ⬜ 待验证 |
| MCP 工具数（首版）≥500 | `tool_registry.py` → `register_all_tools()` 计数 | ⬜ 待验证 |
| MCP Server 内存 ≤200MB | `server.py` → Python 进程 RSS | ⬜ 待验证 |
| 反射缓存 ≤10MB | `GU5AIReflectionScanner::SaveCache()` → JSON 大小检查 | ⬜ 待验证 |
| 100 Actor 查询 ≤100ms | `GU5AICommandExecutor::HandleQueryScene()` | ⬜ 待验证 |
| 请求-响应配对率 100% | `GU5AIWebSocketClient` → `TMap<FGuid, FPromise>` | ⬜ 待验证 |
| JPEG 质量 Q=85 | `GU5AIScreenshotCapture` → `ImageCompressionQuality` | ⬜ 待验证 |

---

## 依赖图

### UE5 侧（C++ 编译依赖，单向向下）

```
GU5AI.uplugin
 └── GU5AI.Build.cs
      ├── Core, CoreUObject, Engine          (引擎基础)
      ├── WebSockets                         (IWebSocket client)
      ├── Json, JsonUtilities                (JSON 序列化)
      ├── EditorSubsystem                    (UEditorSubsystem)
      ├── UnrealEd                           (编辑器 API)
      ├── BlueprintGraph, KismetCompiler     (蓝图编辑，P1)
      └── SlateCore                          (视口截图)

GU5AI.h/.cpp  (模块入口)
 └── GU5AIEditorSubsystem.h/.cpp  (UEditorSubsystem)
      ├── GU5AIWebSocketClient.h/.cpp  (IWebSocket wrapper)
      │    └── 依赖: WebSockets, Json
      ├── GU5AICommandExecutor.h/.cpp  (TQueue + AsyncTask)
      │    ├── 依赖: GU5AITypes.h
      │    └── GU5AIReflectionScanner.h/.cpp  (TObjectIterator)
      └── GU5AIScreenshotCapture.h/.cpp  (GetViewportScreenShot)
```

### Python 侧（包导入依赖，单向向下）

```
server.py  (FastMCP app, entry point)
 ├── ws_server.py  (asyncio WebSocket server)
 │    └── config.py  (CLI/ENV/config.json/defaults)
 ├── tool_registry.py  (dynamic MCP tool registration)
 │    ├── reflection_cache.py  (JSON read/write/validate)
 │    └── tools/
 │         ├── semantic_tools.py  (高层: create_actor, screenshot...)
 │         ├── reflection_tools.py  (低层: 反射生成)
 │         └── meta_tools.py  (list_apis, status, refresh...)
 └── requirements.txt  (fastmcp, websockets)
```

### 混合语言边界

```
Python MCP Server  ──WebSocket(JSON)──▶  UE5 C++ Plugin
                  ◄──WebSocket(JSON+Binary)──
```

无编译期依赖。通过 WebSocket 协议解耦，两端可独立编译和测试。

**跨 DLL 资源传递策略**（Q1=是 触发）：
- 所有跨 DLL 对象通过 UE5 **工厂方法**创建（`FWebSocketsModule::Get().CreateWebSocket()` → `TSharedPtr<IWebSocket>`），不暴露裸指针到 DLL 外
- 反射数据通过 **JSON 文件**（非内存指针）跨进程传递到 Python 侧
- 截图数据通过 **WebSocket 二进制帧**（非共享内存）传输
- UE5 模块间接口全部使用 `TSharedPtr`/`TUniquePtr`/`TArray<uint8>`，不传递不透明 `void*` 句柄
- 无跨 DLL 的 C 接口或 `extern "C"` 工厂函数需求

---

## Prior Art / 先行技术调研

| 调研对象 | 关键发现 | 对本方案的启示 |
|---------|---------|--------------|
| **jl-codes/unreal-5-mcp** | C++ 插件 (TCP 55557) + Python FastMCP；~20 工具手工包装；蓝图编辑支持；活跃维护 | 架构可参考但 TCP 端口管理复杂；确认 WebSocket 更优（全双工 + 二进制帧） |
| **GenOrca/unreal-mcp** | 62 个手工工具含行为树；UE 内嵌 Python 执行；Fab 发售 | 工具数量可作为我们的首版目标（≥500）；但手工包装模式不可取，我们的反射自发现更好 |
| **@runreal/unreal-mcp** | 纯 Node.js MCP，用 UE Python Remote Execution，零 C++ | 安装极简但功能受限于 Python API；我们的 C++ 插件方案可访问全部引擎 API |
| **UEditorSubsystem 官方** | 生命周期：`StartupModule()→Initialize()`，`Deinitialize()→ShutdownModule()` | 确认 Init 时连接 WebSocket，Deinit 时断开 |
| **FastMCP `add_tool()` API** | 支持运行时动态注册，需开启 `capabilities.tools.listChanged: true` | 反射数据加载后批量调用 `add_tool()` 注册 |

---

## 配置文件变更

### 新建文件

| 文件 | 内容 | 默认值 |
|------|------|--------|
| `{Project}/Saved/GU5AI/config.json` | 端口、重连参数、截图质量、确认模式 | port=9876, heartbeat=10s, timeout=30s |
| `{Project}/Saved/GU5AI/reflection_cache.json` | 反射扫描结果（自动生成） | — |
| `{Project}/Saved/GU5AI/command_log.txt` | 操作日志（可关闭） | — |
| `mcp_server/.env.example` | 配置模板（环境变量方式） | 含所有默认值 |
| `.claude/mcp.json` | Claude Code MCP 配置 | 指向 Python server.py |

### 修改文件

| 文件 | 变更 |
|------|------|
| `DSPlugin.uproject` | Plugins 列表新增 `{"Name":"GU5AI","Enabled":true}` |

---

## 里程碑与任务

---

### 里程碑 M1：通信链路打通 —— "ping/pong 能往返了"

**范围**：UE5 插件骨架 + Python MCP Server 骨架 + WebSocket 连接 + echo 验证

**验收**：在 Claude Code 中 `ping` → UE5 编辑器收到 → 返回 `pong` → Claude Code 显示

---

### Task 1: UE5 插件骨架 + 模块注册

**优先级**：P0
**前置条件**：DSPlugin 工程可正常编译运行
**交付物**：可编译的 GU5AI 插件模块，在编辑器启动时执行 Initialize/Deinitialize

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/GU5AI.uplugin` — 插件描述文件，类型 Editor
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/GU5AI.Build.cs` — 模块构建规则
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AI.h` — 模块头文件
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AI.cpp` — 模块实现（StartupModule/ShutdownModule）
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIEditorSubsystem.h` — UEditorSubsystem 子类声明
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIEditorSubsystem.cpp` — 生命周期实现（Initialize 日志 + Deinitialize 日志）

**修改文件**：
- `D/DSPlugin/DSPlugin.uproject` — Plugins 数组新增 `{"Name":"GU5AI","Enabled":true}`

**关键接口**：
```cpp
// GU5AIEditorSubsystem.h
UCLASS()
class GU5AI_API UGU5AIEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
};
```

**验收标准**：
- [ ] `GenerateProjectFiles.bat` 生成 VS 项目包含 GU5AI 模块
- [ ] UE5 编辑器启动后 Output Log 出现 `[GU5AI] EditorSubsystem Initialized`
- [ ] 关闭编辑器时 Output Log 出现 `[GU5AI] EditorSubsystem Deinitialized`

**数值锚点**：无（纯脚手架）

**验证步骤**：
1. 创建 Plugins/GU5AI/ 目录结构
2. 编写 uplugin + Build.cs + 源文件
3. 修改 DSPlugin.uproject 注册插件
4. 运行 `GenerateProjectFiles.bat`
5. 编译 DSPlugin 项目 → 成功
6. 启动 UE5 编辑器 → 检查 Output Log

**依赖**：无
**规模**：M（6 文件）
**可并行于**：Task 2（Python 端独立）
**架构分层**：引擎层（插件模块）
**异常策略**：`StartupModule()` 失败返回 false，阻止模块加载
**并发模型**：无
**取消策略**：无

---

### Task 2: Python MCP Server 骨架 + WebSocket Server

**优先级**：P0
**前置条件**：Python 3.12+ 已安装
**交付物**：可启动的 Python MCP Server，WebSocket 端口 9876 监听

**新建文件**：
- `mcp_server/requirements.txt` — `fastmcp`, `websockets`
- `mcp_server/src/gu5ai/__init__.py` — 包标识
- `mcp_server/src/gu5ai/server.py` — FastMCP app + stdio transport 入口
- `mcp_server/src/gu5ai/ws_server.py` — asyncio WebSocket Server，监听 `ws://127.0.0.1:9876`，心跳 10s
- `mcp_server/src/gu5ai/config.py` — 配置加载（CLI > ENV > config.json > 默认值）

**关键接口**：
```python
# server.py
from fastmcp import FastMCP

mcp = FastMCP(
    "GU5-AI-Plugin",
    capabilities={"tools": {"listChanged": True}}
)

@mcp.tool()
def ping() -> str:
    """测试连通性：向 UE5 发送 ping，返回 pong"""
    ...

# ws_server.py
class GU5WebSocketServer:
    def __init__(self, host="127.0.0.1", port=9876):
        ...
    async def start(self) -> None:
        """启动 WebSocket Server，10s 心跳"""
        ...
    async def send_command(self, method: str, params: dict) -> dict:
        """发送命令并等待响应，30s 超时"""
        ...
    async def stop(self) -> None:
        ...
```

**验收标准**：
- [ ] `python -m gu5ai.server` 启动后 `ws://127.0.0.1:9876` 可连接
- [ ] 用 wscat 连接，发送 PING 收到 PONG
- [ ] 10s 无消息时收到 PING 帧
- [ ] `GU5AI_PORT=9877 python -m gu5ai.server` 在 9877 端口监听
- [ ] `mypy src/` 无类型错误，`ruff check src/` 无警告

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| WebSocket 端口 9876 | `config.py:DEFAULT_PORT` |
| 心跳 10s | `ws_server.py:heartbeat_interval` |
| 命令超时 30s | `ws_server.py:command_timeout` |

**验证步骤**：
1. `pip install -r requirements.txt`
2. `python -m gu5ai.server` 启动
3. 另一个终端: `wscat -c ws://127.0.0.1:9876` → 连接成功
4. 发送 `{"id":"1","method":"ping","params":{}}` → `{"id":"1","result":"pong","error":null}`
5. Ctrl+C 停止 → 干净退出

**依赖**：无（独立于 UE5）
**规模**：S（5 文件，含 requirements.txt）
**可并行于**：Task 1
**架构分层**：服务层
**异常策略**：`asyncio.CancelledError` → 优雅关闭；`websockets.ConnectionClosed` → 清理连接
**并发模型**：asyncio 单线程事件循环
**取消策略**：`asyncio.wait_for(command_future, timeout=30.0)`

---

### Task 3: UE5 WebSocket Client + 连接 Python Server

**优先级**：P0
**前置条件**：Task 1（UE5 插件骨架），Task 2（Python Server）
**交付物**：UE5 编辑器启动时自动连接 Python Server，可发送/接收 JSON 消息

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIWebSocketClient.h` — IWebSocket 封装
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIWebSocketClient.cpp`

**修改文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIEditorSubsystem.cpp` — Initialize 中创建 WebSocket 连接，Deinitialize 中断开

**关键接口**：
```cpp
// GU5AIWebSocketClient.h
class GU5AI_API FGU5AIWebSocketClient final  // final: 禁止继承，无需虚析构
{
public:
    explicit FGU5AIWebSocketClient(const FString& ServerUrl);
    ~FGU5AIWebSocketClient();  // 非虚析构 (final 类)，先 Disconnect 再清理

    void Connect();
    void Disconnect();  // 清理 PendingRequests、取消心跳、关闭 WebSocket
    bool IsConnected() const;

    // 发送命令，返回 Future（跨线程安全）
    // 注意：WebSocket 断开时，所有未完成的 PendingRequests 会收到 E_GU5_DISCONNECTED 错误
    TFuture<TSharedPtr<FJsonObject>> SendCommand(const FString& Method, TSharedPtr<FJsonObject> Params);

    DECLARE_MULTICAST_DELEGATE(FOnConnected);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisconnected, const FString& Reason);
    FOnConnected OnConnected;
    FOnDisconnected OnDisconnected;

private:
    void OnWebSocketConnected();
    void OnWebSocketConnectionError(const FString& Error);
    void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void OnWebSocketMessage(const FString& Message);
    void CleanupPendingRequests();  // 所有未完成 Promise 返回 E_GU5_DISCONNECTED

    TSharedPtr<IWebSocket> WebSocket;         // 所有权: TSharedPtr (UE5 约定)
    TMap<FGuid, TPromise<TSharedPtr<FJsonObject>>> PendingRequests;  // 所有权: 值类型
    FTimerHandle HeartbeatTimerHandle;         // 所有权: FTimerManager (外部注入)
    FCriticalSection PendingRequestsLock;      // 保护 PendingRequests 跨线程访问
    int32 ReconnectAttempt;
    static constexpr int32 MaxReconnectAttempts = 3;
    static constexpr float ReconnectDelays[3] = {1.0f, 2.0f, 4.0f};
};

// 析构顺序原则:
// ~FGU5AIWebSocketClient:
//   1. Disconnect() → 关闭 WebSocket + CleanupPendingRequests() + 取消心跳
//   2. 清理委托 (OnConnected/OnDisconnected .Clear())
//   3. 隐式析构成员 (TSharedPtr → 引用计数归零自动释放, TMap → 值清理)
```

**验收标准**：
- [ ] 先启动 Python Server，再启动 UE5 编辑器 → Output Log 显示 `[GU5AI] WebSocket connected to ws://127.0.0.1:9876`
- [ ] 先启动 UE5，再启动 Python Server → UE5 自动重连（1s/2s/4s），第三次失败后停止
- [ ] `SendCommand("ping", {})` → 返回 `{"result": "pong"}`
- [ ] Python Server 关闭 → UE5 Output Log 显示断连，自动开始重连

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| 重连 ≤5s | `ReconnectDelays[0] = 1.0f` |
| 指数退避 1/2/4s | `ReconnectDelays` 数组 |
| 最多 3 次重连 | `MaxReconnectAttempts = 3` |
| 心跳 10s | `HeartbeatTimerHandle` |

**验证步骤**：
1. 启动 Python Server
2. 启动 UE5 编辑器 → 观察 Output Log 连成功
3. 关闭 Python Server → 观察断连 + 重连日志
4. 重启 Python Server → 观察重连成功
5. 关闭 UE5 编辑器 → Python Server 显示客户端断开

**依赖**：Task 1, Task 2
**规模**：M（2 新建 + 1 修改）
**可并行于**：无（依赖 Task 1+2）
**架构分层**：引擎层
**异常策略**：连接失败 → `OnWebSocketConnectionError` → 触发重连；消息解析失败 → 日志 + 丢弃
**并发模型**：IWebSocket 后台线程 → `OnMessage` 回调 → `AsyncTask` 投递 Game Thread
**线程安全**：`PendingRequests` 由 `FCriticalSection PendingRequestsLock` 保护（OnWebSocketMessage 可能在 WebSocket 线程回调，SendCommand 在 Game Thread）；`ReconnectAttempt` 由 `FCriticalSection` 保护；`WebSocket` 的 `Connect()`/`Disconnect()` 均在 Game Thread 调用
**取消策略**：`Deinitialize()` 中取消心跳定时器，关闭 WebSocket

---

### 里程碑 M1 交付物

- UE5 编辑器启动后自动连接 Python MCP Server
- 双向 JSON 消息通信（ping/pong 验证）
- 断连自动重连（指数退避，3 次上限）

### 里程碑 M1 验证

- [ ] DSPlugin 编译通过，无警告
- [ ] `python -m gu5ai.server` 正常启动
- [ ] wscat 连接 9876 端口收发消息正常
- [ ] UE5 编辑器与 Python Server 双向 ping/pong 通过
- [ ] 断连重连流程验证通过
- [ ] `ruff check + mypy` Python 静态分析通过
- [ ] Clang-Tidy / Cppcheck 无严重警告

---

### 里程碑 M2：反射扫描 + Actor 操控 —— "能创建方块了"

**范围**：反射系统扫描、缓存、MCP 动态注册、命令执行框架、Actor CRUD

**验收**：Claude Code 中 `create_actor("StaticMeshActor", {"x":0,"y":0,"z":100})` → 场景中出现方块

---

### Task 4: 反射扫描引擎

**优先级**：P0
**前置条件**：Task 3（WebSocket 已连通）
**交付物**：扫描全部已加载 UCLASS/UEnum/UProperty/UFunction，序列化为 JSON 缓存

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIReflectionScanner.h`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIReflectionScanner.cpp`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AITypes.h` — 共享数据类型

**关键接口**：
```cpp
// GU5AITypes.h

// --- 反射类型 ---
struct GU5AI_API FReflectedProperty
{
    FString Name;
    FString Type;
    FString MetaData; // BlueprintReadWrite, EditAnywhere, etc.
};

struct GU5AI_API FReflectedParameter
{
    FString Name;
    FString Type;
    bool bIsReturnParam;
};

struct GU5AI_API FReflectedFunction
{
    FString Name;
    FString ClassName;
    FString ModuleName;
    TArray<FReflectedParameter> Parameters;
    bool bIsStatic;
};

// --- 命令类型（跨线程 DTO）---
enum class EGU5AIErrorCode : int32
{
    OK = 0,
    INVALID_ARG = -1,
    NOT_FOUND = -2,
    TYPE_MISMATCH = -3,
    TIMEOUT = -4,
    DISCONNECTED = -5,
    INTERNAL = -99
};

struct GU5AI_API FCommandContext
{
    FGuid Id;                              // 请求唯一标识
    FString Method;                        // 命令名（如 "create_actor"）
    TSharedPtr<FJsonObject> Params;        // 参数（所有权: 值复制）
    double EnqueueTime;                    // 入队时间（FPlatformTime::Seconds()）
};

struct GU5AI_API FCommandResult
{
    FGuid Id;                              // 匹配的请求 Id
    TSharedPtr<FJsonObject> Result;        // 成功结果（与 Error 互斥）
    FString Error;                         // 错误描述（与 Result 互斥）
    int32 ErrorCode = 0;                   // EGU5AIErrorCode
    TArray<uint8> BinaryAttachment;        // 可选二进制附件（截图等）
};

// GU5AIReflectionScanner.h
class GU5AI_API FGU5AIReflectionScanner
{
public:
    struct FScanResult
    {
        TArray<FReflectedFunction> Functions;
        TArray<FString> ClassNames;
        TArray<FString> EnumNames;
        FString EngineVersion;
        FString ModuleListHash;
        int32 FunctionCount;
    };

    static FScanResult ScanAllModules();
    static FScanResult ScanIncremental(const FString& PreviousModuleHash);
    static bool SaveCacheToJson(const FScanResult& Result, const FString& FilePath);
    static bool LoadCacheFromJson(FScanResult& OutResult, const FString& FilePath);
};
```

**验收标准**：
- [ ] 扫描 DSPlugin 工程加载的全部模块，返回函数数 ≥500（含引擎运行时函数）
- [ ] 扫描耗时 ≤10s（Output Log 记录）
- [ ] JSON 文件写入 `{Project}/Saved/GU5AI/reflection_cache.json`，大小 ≤10MB
- [ ] JSON 含 `EngineVersion` + `ModuleListHash` 两个 key 字段
- [ ] 增量扫描（模块未变时）≤1s

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| 全量扫描 ≤10s | `FGU5AIReflectionScanner::ScanAllModules()` 计时 |
| 增量扫描 ≤1s | `FGU5AIReflectionScanner::ScanIncremental()` |
| 缓存 ≤10MB | `SaveCacheToJson()` → 文件大小检查 |

**验证步骤**：
1. 在 UE5 编辑器中触发扫描（通过 WebSocket 发 `scan_reflection` 命令）
2. 检查 Output Log 扫描耗时
3. 检查 `Saved/GU5AI/reflection_cache.json` 是否存在且内容正确
4. 抽样验证 10 个已知函数（如 `AActor::SetActorLocation`）JSON 签名正确
5. 关闭再打开编辑器 → 增量扫描耗时 <1s

**依赖**：Task 3
**规模**：M（3 新建 + GU5AIWebSocketClient 增加消息处理）
**可并行于**：Task 5（Python 工具注册，可先 Mock 缓存数据）
**架构分层**：引擎层
**异常策略**：某模块扫描失败 → 跳过该模块，继续扫描其余，日志记录失败模块名
**并发模型**：同步扫描（Game Thread 执行，启动时一次性操作）
**取消策略**：无（扫描为原子操作，不取消）

---

### Task 5: MCP 动态工具注册

**优先级**：P0
**前置条件**：Task 4（反射 JSON 缓存存在），Task 2（Python Server 运行）
**交付物**：Python 侧读取反射缓存，动态注册 ≥500 个 MCP 工具

**新建文件**：
- `mcp_server/src/gu5ai/reflection_cache.py` — 缓存加载、校验
- `mcp_server/src/gu5ai/tool_registry.py` — FastMCP 动态工具注册
- `mcp_server/src/gu5ai/tools/__init__.py`
- `mcp_server/src/gu5ai/tools/meta_tools.py` — list_available_apis, refresh_reflection, status
- `mcp_server/src/gu5ai/tools/reflection_tools.py` — 反射函数自动包装为 MCP tool

**关键接口**：
```python
# reflection_cache.py
class ReflectionCache:
    def __init__(self, cache_path: str):
        ...
    def load(self) -> dict:
        """加载 JSON 缓存，校验 EngineVersion + ModuleListHash"""
        ...
    def is_valid(self) -> bool:
        """检查缓存是否存在且有效"""
        ...

# tool_registry.py
class ToolRegistry:
    def __init__(self, mcp: FastMCP, cache: ReflectionCache):
        ...
    def register_semantic_tools(self) -> int:
        """注册高层语义工具，返回注册数量"""
        ...
    def register_reflection_tools(self, max_tools: int = 0) -> int:
        """从缓存批量注册反射工具，返回注册数量"""
        ...
    def unregister_all(self) -> None:
        """移除全部已注册工具（用于重载）"""
        ...

# meta_tools.py
@mcp.tool()
def list_available_apis(category: str = "all") -> str:
    """列出所有可用 UE5 API。category: all/actor/blueprint/level/material/animation"""
    ...

@mcp.tool()
def refresh_reflection_cache() -> str:
    """请求 UE5 重新扫描反射，更新缓存"""
    ...
```

**验收标准**：
- [ ] 启动 Python Server → 自动加载 `reflection_cache.json` → 注册 ≥500 个 MCP 工具
- [ ] Claude Code `mcp list-tools gu5` 返回 ≥500 工具
- [ ] `list_available_apis("actor")` 返回 Actor 相关函数列表
- [ ] `refresh_reflection_cache()` 触发 UE5 重新扫描并更新缓存
- [ ] 删除缓存文件 → Server 启动时提示"缓存缺失，请启动 UE5 编辑器生成"

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| MCP 工具数 ≥500 | `ToolRegistry.register_reflection_tools()` 返回值 |
| MCP Server 内存 ≤200MB | 任务管理器 RSS |

**验证步骤**：
1. 确保 `reflection_cache.json` 存在
2. 启动 `python -m gu5ai.server`
3. 在 Claude Code 中 `mcp list-tools gu5` → 统计数量 ≥500
4. 调用 `list_available_apis("actor")` → 返回 Actor 函数列表
5. 调用一个反射注册的函数（如 `gu5_AActor_SetActorLocation`）→ 返回正确结果

**依赖**：Task 4（缓存 JSON），Task 2（Server）
**规模**：M（4 文件）
**可并行于**：Task 6（命令执行框架，Python 侧可 Mock 验证）
**架构分层**：服务层
**异常策略**：JSON 解析失败 → 返回错误并提示重新扫描；单个 tool 注册失败 → 跳过，继续注册其余
**并发模型**：asyncio 单线程
**取消策略**：无（启动时一次性批量注册）

---

### Task 6: 命令执行框架（UE5 侧）

**优先级**：P0
**前置条件**：Task 3（WebSocket 连通），Task 4（反射数据就绪）
**交付物**：WebSocket 收到的命令通过 Mpsc 队列投递到 Game Thread，执行后返回结果

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AICommandExecutor.h`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AICommandExecutor.cpp`

**修改文件**：
- `GU5AIWebSocketClient.cpp` — `OnWebSocketMessage` 中投递命令到 Executor

**关键接口**：
```cpp
// GU5AICommandExecutor.h
using FCommandHandler = TFunction<TSharedPtr<FJsonObject>(TSharedPtr<FJsonObject> Params)>;

class GU5AI_API FGU5AICommandExecutor
{
public:
    void RegisterHandler(const FString& MethodName, FCommandHandler Handler);
    void EnqueueCommand(const FGuid& Id, const FString& Method, TSharedPtr<FJsonObject> Params);
    void Tick(float DeltaTime);

private:
    void ProcessQueue(); // 在 Game Thread 中调用
    TMap<FString, FCommandHandler> Handlers;
    TQueue<TSharedPtr<FCommandContext>, EQueueMode::Mpsc> CommandQueue;
    TArray<TSharedPtr<FCommandContext>> ExpiredCommands;
    static constexpr float CommandTimeoutSeconds = 30.0f;
};
```

**验收标准**：
- [ ] WebSocket 收到 `{"id":"uuid","method":"echo","params":{"msg":"hello"}}` → `{"id":"uuid","result":{"msg":"hello"},"error":null}` 返回
- [ ] 注册 handler 后调用 `RegisterHandler("test", ...)` → 命令正确路由到 handler
- [ ] 发 100 条并发命令 → 100 条响应全部配对，无孤儿
- [ ] 命令执行超过 30s → 返回 `E_GU5_TIMEOUT` 错误
- [ ] 未注册的 method → 返回 `E_GU5_NOT_FOUND` 错误

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| 命令超时 30s | `CommandTimeoutSeconds` |
| 请求-响应配对 100% | `PendingRequests` 在析构中检查 |

**验证步骤**：
1. 在 UE5 编辑器中通过 WebSocket 发 `echo` 命令 → 验证返回
2. 连续发 100 条 `echo` → 验证全部配对
3. 发一个死循环 handler → 30s 后验证 TIMEOUT

**依赖**：Task 3, Task 4
**规模**：M（2 新建 + 1 修改）
**可并行于**：Task 5（Python 注册工具可并行）
**架构分层**：引擎层
**异常策略**：Handler 执行异常 → 捕获 → 返回 `E_GU5_INTERNAL` + 堆栈；队列满 → 丢弃 + 日志 + 返回资源不足错误
**并发模型**：WebSocket Thread → `TQueue::Enqueue()` → Game Thread `Tick()` → `ProcessQueue()` → `AsyncTask` 投递 handler → 结果通过 WebSocket Thread 回传
**线程安全**：`TQueue<EQueueMode::Mpsc>` 保证读写安全；`Handlers` 注册仅在 Game Thread，读取无锁
**取消策略**：超时检查在 `Tick()` 中执行，过期命令移除并返回 TIMEOUT

---

### Task 7: Actor CRUD 命令

**优先级**：P0
**前置条件**：Task 6（命令执行框架就绪）
**交付物**：通过自然语言命令创建/修改/删除/查询场景 Actor

**新建文件**：
- `mcp_server/src/gu5ai/tools/semantic_tools.py` — Actor、Level、截图等高层语义工具

**修改文件**：
- `GU5AICommandExecutor.cpp` — 注册 Actor handler（在 Game Thread 中操作 UWorld）

**关键接口**：
```python
# semantic_tools.py
@mcp.tool()
def create_actor(
    class_name: str,
    location: dict[str, float] = {"x": 0, "y": 0, "z": 0},
    rotation: dict[str, float] = {"pitch": 0, "yaw": 0, "roll": 0},
    scale: dict[str, float] = {"x": 1, "y": 1, "z": 1}
) -> dict:
    """创建 Actor 并放置到关卡中。class_name 如 'StaticMeshActor', 'PointLight' 等"""
    ...

@mcp.tool()
def modify_actor_property(actor_name: str, property_name: str, value) -> dict:
    """修改指定 Actor 的属性值"""
    ...

@mcp.tool()
def delete_actor(actor_name: str, confirm: bool = False) -> dict:
    """删除 Actor。需确认（混合模式）"""
    ...

@mcp.tool()
def query_scene(filter_class: str = "") -> dict:
    """列出当前关卡所有 Actor（名/类型/位置/组件）"""
    ...
```

**C++ Handler 注册（在 GU5AIEditorSubsystem::Initialize 中）**：
```cpp
CommandExecutor->RegisterHandler("create_actor", [](auto Params) {
    FString ClassName = Params->GetStringField("class_name");
    FVector Location = ParseVector(Params->GetObjectField("location"));
    // FActorSpawnParameters + GEditor->GetEditorWorldContext().World()->SpawnActor()
    // 返回: {"actor_name": "StaticMeshActor_0", "location": {...}}
});
// ... modify_actor, delete_actor, query_scene handlers
```

**验收标准**：
- [ ] `create_actor("StaticMeshActor", {"x":0,"y":0,"z":100})` → 场景中心出现一个立方体
- [ ] `modify_actor_property("StaticMeshActor_0", "bHidden", true)` → Actor 隐藏
- [ ] `delete_actor("StaticMeshActor_0")` → 先返回确认请求，确认后删除
- [ ] `query_scene()` → 返回全部 Actor 列表，含名称和位置
- [ ] 调用不存在的 `class_name` → 返回错误 + 建议列表

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| 创建 Actor ≤80ms | `create_actor` handler（不含类加载）|
| 修改属性 ≤30ms | `modify_actor_property` handler |
| 100 Actor 查询 ≤100ms | `query_scene` handler |

**验证步骤**：
1. Claude Code: `create_actor("StaticMeshActor", {"x":0,"y":0,"z":100})` → 场景中出现方块
2. `modify_actor_property("StaticMeshActor_0", "Mobility", "Movable")` → 属性变更
3. `query_scene()` → 列表中包含 StaticMeshActor_0
4. `delete_actor("StaticMeshActor_0", true)` → Actor 消失
5. 截图验证（下一个任务）

**依赖**：Task 6
**规模**：M（1 新建 Python + 1 修改 C++）
**可并行于**：Task 8（截图功能可并行开发）
**架构分层**：服务层（Python tool）+ 引擎层（C++ handler）
**异常策略**：class_name 不存在 → `E_GU5_NOT_FOUND` + 相近名称建议；SpawnActor 失败 → 返回引擎错误原因
**并发模型**：Game Thread 同步执行（Actor 操作必须在 Game Thread）
**取消策略**：删除操作在混合模式下先返回确认请求，等用户确认后继续

---

### 里程碑 M2 交付物

- 反射扫描覆盖全部已加载模块（≥500 函数）
- MCP 工具动态注册（≥500 工具可用）
- Actor 创建/修改/删除/查询全部可操作
- 数据流完整闭环：Claude Code → MCP/stdio → Python → WebSocket → UE5 → 执行 → 返回

### 里程碑 M2 验证

- [ ] DSPlugin 编译通过
- [ ] Python Server 启动，工具数 ≥500
- [ ] Claude Code 调用 `create_actor` → 编辑器中出现 Actor
- [ ] 100ms 延迟达标（简单命令）
- [ ] 反射扫描 ≤10s
- [ ] 请求-响应配对率 100%
- [ ] Clang-Tidy / Cppcheck 无严重警告

---

### 里程碑 M3：截图 + P1 功能 —— "能截图看场景 + 编辑蓝图"

**范围**：视口截图、蓝图编辑、关卡操控、材质/动画基础操作

**验收**：Claude Code 中 `screenshot()` → 返回场景截图；`create_blueprint_node(...)` → 蓝图节点出现

---

### Task 8: 视口截图

**优先级**：P0
**前置条件**：Task 6（命令执行框架）
**交付物**：调用 `screenshot` 返回编辑器视口的 JPEG 截图

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIScreenshotCapture.h`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIScreenshotCapture.cpp`

**修改文件**：
- `GU5AICommandExecutor.cpp` — 注册 `screenshot` handler
- `semantic_tools.py` — 新增 `screenshot()` MCP tool

**关键接口**：
```cpp
// GU5AIScreenshotCapture.h
class GU5AI_API FGU5AIScreenshotCapture
{
public:
    struct FCaptureSettings
    {
        int32 Width = 1920;
        int32 Height = 1080;
        int32 Quality = 85;        // JPEG quality
        bool bJPEG = true;         // true=JPEG, false=PNG
    };

    static TArray<uint8> CaptureViewport(const FCaptureSettings& Settings);
};
```

```python
# semantic_tools.py
@mcp.tool()
def screenshot(width: int = 1920, height: int = 1080, quality: int = 85, format: str = "jpeg") -> str:
    """捕获编辑器视口截图，返回 Base64 编码的图片"""
    ...
```

**验收标准**：
- [ ] `screenshot()` → 返回 1920×1080 JPEG Base64，Claude Code 可显示
- [ ] `screenshot(3840, 2160)` → 返回 4K 截图
- [ ] `screenshot(format="png")` → 返回 PNG 截图
- [ ] 捕获+编码+传输 ≤200ms @ 1920×1080

**数值锚点**：
| SRS 要求 | 本任务实现位置 |
|---------|-------------|
| 截图 ≤200ms @ 1920×1080 | `CaptureViewport` 计时 |
| JPEG Q=85 | `FCaptureSettings::Quality = 85` |

**验证步骤**：
1. Claude Code: `screenshot()` → 显示场景截图
2. 在场景中放 Actor → `screenshot()` → 截图可见新 Actor
3. `screenshot(3840, 2160)` → 4K 截图 ≤500ms

**依赖**：Task 6
**规模**：S（2 C++ + 1 Python 修改）
**可并行于**：Task 7, Task 9
**架构分层**：引擎层 + 服务层
**异常策略**：视口不可用 → 返回错误 "视口未就绪"
**并发模型**：Render Thread 捕获 → Game Thread 编码 → WebSocket Thread 发送

---

### Task 9: 蓝图节点图编辑（P1）

**优先级**：P1
**前置条件**：Task 7（Actor CRUD）
**交付物**：打开蓝图编辑器图、创建节点、连线、编译

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIBlueprintEditor.h`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIBlueprintEditor.cpp`

**修改文件**：
- `GU5AICommandExecutor.cpp` — 注册蓝图相关 handler
- `semantic_tools.py` — 新增蓝图 MCP tools

**关键接口**：
```cpp
// GU5AIBlueprintEditor.h
class GU5AI_API FGU5AIBlueprintEditor
{
public:
    struct FNodeCreateParams
    {
        FString BlueprintPath;
        FString GraphName;         // "EventGraph", "ConstructionScript", etc.
        FString NodeClassName;     // "K2Node_CallFunction", "K2Node_Event", etc.
        FString FunctionName;      // 要调用的函数名（如 "PrintString"）
        FVector2D Position;
    };

    static TSharedPtr<FJsonObject> CreateNode(const FNodeCreateParams& Params);
    static TSharedPtr<FJsonObject> ConnectPins(
        const FString& BlueprintPath,
        const FString& SourceNodeId, const FString& SourcePinName,
        const FString& TargetNodeId, const FString& TargetPinName);
    static TSharedPtr<FJsonObject> CompileBlueprint(const FString& BlueprintPath);
};
```

**验收标准**：
- [ ] `create_blueprint_node("BP_Test", "EventGraph", "K2Node_CallFunction", "PrintString")` → 节点出现在蓝图图中
- [ ] `connect_blueprint_pins("BP_Test", "node_1", "then", "node_2", "execute")` → 连线创建
- [ ] `compile_blueprint("BP_Test")` → 返回编译成功/失败及错误详情
- [ ] 类型不兼容连线 → 返回类型错误而不崩溃

**验证步骤**：
1. 创建 BP_Test 蓝图资产
2. Claude Code 创建 BeginPlay + PrintString 节点并连线
3. 编译 → 成功
4. 故意连不兼容类型 → 返回错误

**依赖**：Task 7
**规模**：M（2 C++ + Python tool）
**可并行于**：Task 8, Task 10, Task 11
**架构分层**：引擎层
**异常策略**：蓝图资产不存在 → `E_GU5_NOT_FOUND`；编译失败 → 返回错误列表（文件/行号/描述）
**并发模型**：Game Thread 同步（UEdGraph 操作必须在 Game Thread）

---

### Task 10: 关卡操控（P1）

**优先级**：P1
**前置条件**：Task 7
**交付物**：加载/卸载子关卡、设置视口相机、World Outliner 操作

**新建文件**：无新类（在 CommandExecutor 中注册 handler）
**修改文件**：
- `GU5AICommandExecutor.cpp` — 注册关卡 handler
- `semantic_tools.py` — 新增关卡 MCP tools

**关键接口**：
```python
@mcp.tool()
def set_viewport_camera(location: dict, rotation: dict = None) -> dict:
    """设置编辑器视口相机位置和旋转"""
    ...

@mcp.tool()
def load_sublevel(level_name: str) -> dict:
    """加载子关卡"""
    ...

@mcp.tool()
def unload_sublevel(level_name: str) -> dict:
    """卸载子关卡"""
    ...
```

**验收标准**：
- [ ] `set_viewport_camera({"x":0,"y":0,"z":1000})` → 视口切换到俯视
- [ ] `load_sublevel("SubLevel_Test")` → 子关卡加载
- [ ] 不存在的子关卡 → 返回错误 + 可用列表

**依赖**：Task 7
**规模**：S（0 新建 + 2 修改）
**可并行于**：Task 8, Task 9, Task 11
**架构分层**：引擎层 + 服务层

---

### Task 11: 材质 + 动画蓝图（P1）

**优先级**：P1
**前置条件**：Task 7
**交付物**：材质创建/编辑、动画蓝图状态机编辑

**新建文件**：
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIMaterialEditor.h`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIMaterialEditor.cpp`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Public/GU5AIAnimationEditor.h`
- `D/DSPlugin/Plugins/GU5AI/Source/GU5AI/Private/GU5AIAnimationEditor.cpp`

**关键接口**：
```python
@mcp.tool()
def create_material(name: str, base_color: dict, metallic: float = 0.0, roughness: float = 0.5) -> dict:
    """创建材质资产，设置基础参数"""
    ...

@mcp.tool()
def edit_animation_state_machine(
    anim_bp_path: str,
    from_state: str,
    to_state: str,
    condition: str
) -> dict:
    """在动画蓝图中创建状态过渡"""
    ...
```

**验收标准**：
- [ ] `create_material("M_Red", {"r":1,"g":0,"b":0}, 0.8, 0.3)` → Content Browser 出现 M_Red 材质
- [ ] `edit_animation_state_machine("ABP_Char", "Idle", "Run", "Speed > 0")` → 状态机过渡创建
- [ ] 支持 ≥20 种 MaterialExpression 类型

**依赖**：Task 7
**规模**：M（4 C++ + Python tools）
**可并行于**：Task 8, Task 9, Task 10
**架构分层**：引擎层

---

### 里程碑 M3 交付物

- 视口截图（可配置分辨率/质量/格式）
- 蓝图节点图编辑（创建/连线/编译）
- 关卡操控（视口相机/子关卡）
- 材质创建 + 动画蓝图编辑

### 里程碑 M3 验证

- [ ] `screenshot()` 截图验证通过
- [ ] 蓝图编辑完整流程：创建节点 → 连线 → 编译 → 成功
- [ ] 所有 P1 核心用例跑通
- [ ] 连续运行 2h 无崩溃
- [ ] Clang-Tidy / Cppcheck 无严重警告
- [ ] ruff check + mypy 通过

---

## 风险与缓解

| 风险 | 影响 | 概率 | 缓解 | 回滚 |
|------|:---:|:---:|------|------|
| UE5.7 反射对某些编辑器子系统覆盖不全 | 中 | 中 | M2 先扫描验证覆盖率；不足的手工补 10-20 个关键 API | 手工补充 `REQ-REF-004` 包装器，不影响反射生成的主路径 |
| 蓝图 UEdGraph API 签名在 5.7 中变更 | 高 | 低 | Task 9 开始前先查官方文档；编译时 `#if ENGINE_MAJOR_VERSION` 做版本隔离 | 条件编译降级：不支持蓝图编辑的 UE 版本自动隐藏蓝图 MCP tools |
| FastMCP 大量 tool 注册导致 MCP 客户端超时 | 高 | 低 | 分批注册（200 个/批）；按模块命名空间分组 | 降级为仅注册高层语义工具（~10 个），反射工具改为按需懒注册 |
| Game Thread 阻塞导致编辑器卡顿 | 中 | 中 | 耗时操作限 30s 超时；复杂反射调用放在下一帧 Tick | 开启同步模式 + 在 Output Log 输出性能警告，提醒用户降低并发 |
| WebSocket 大截图传输阻塞命令通道 | 低 | 低 | 二进制帧走独立 WebSocket 消息通道，不阻塞 JSON 命令 | 降级为降低默认分辨率到 1280×720 |

---

## 开放问题

1. **反射工具命名策略**：函数名如 `gu5_AActor_SetActorLocation` 还是 `gu5.actor.set_location`？建议 PoC 时两种都试试，选 Claude Code 工具选择效果更好的
2. **截图二进制帧最大尺寸**：4K PNG 可能达 30MB，WebSocket 是否需分片？建议先限制 4K 用 JPEG
3. **DSPlugin.uproject 插件注册**：是否直接用项目插件目录（`Project/Plugins/`）还是引擎插件目录？建议用项目插件目录，不污染引擎

---

## 已知限制

- 蓝图编辑首次支持的节点类型有限（CallFunction、Event、Variable），复杂控制流节点（如 Branch、ForLoop）在后续版本支持
- MaterialExpression 支持 ≥20 种常见类型，非全部 100+ 种
- Niagara VFX、Landscape、PCG 仅反射覆盖，不提供专用高层语义工具

---

## 方案变更记录

| 日期 | 变更内容 | 原因 | 影响的任务 |
|------|---------|------|-----------|
| 2026-05-08 | 初始版本 | 基于 SRS v1.1 + plan-review | 全部 |
| 2026-05-08 | v1.1 修复审查问题 | 修复 3 🟡 + 2 🟢：① FGU5AIWebSocketClient 加 final + 析构顺序文档 + PendingRequests 清理 + FCriticalSection；② GU5AITypes.h 补充 FCommandContext/FCommandResult/EGU5AIErrorCode；③ 虚析构 (final 类免)；④ 风险表补充回滚列；⑤ 跨 DLL 资源传递策略 | Task 3, Task 4, Task 6 |

---

## 参考文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| SRS v1.1 | 需求规格/GU5 AI Plugin-需求规格说明书.md | 需求规格 |
| 审查报告 v1.1 | 方案/GU5 AI Plugin-审查报告.md | plan-review 输出 |
| UE5 WebSockets API | https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/WebSockets | 官方 API 参考 |
| UEditorSubsystem | https://dev.epicgames.com/documentation/unreal-engine/programming-subsystems-in-unreal-engine | 子系统编程指南 |
| FastMCP | https://github.com/modelcontextprotocol/python-sdk | MCP Python SDK |
