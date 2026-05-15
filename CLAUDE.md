# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> 基础纪律见 user_rules（`C:\Users\Administrator\.claude\CLAUDE.md`），本文仅补充项目特化内容。

---

## 🖥️ 环境

| 项 | 值 |
|----|----|
| 开发机 | i9-9900 / 64GB / RTX 3090Ti 24GB |
| 系统 | Win10 x64 |
| UE5 | 5.7（`F:\Epic Games\UE_5.7`）|
| Python | 3.14.0（`C:\Python314\python.exe`）|
| VS | VS2022 |
| Git 远程 | `https://github.com/zsg2022/UE5-AI-AUTO.git` |

---

## 项目架构

### 通信链路（3 层桥接）

```
Claude Code (MCP Client)
  ↕ FastMCP stdio
server.py (Python MCP Server, 72 工具)
  ↕ TCP :9876
bridge_server.py (TCP 中继)
  ↕ TCP :9876
FUE5AIAUTOWebSocketClient (UE5 C++ TCP Client, FRunnable 线程)
  ↕ AsyncTask → Game Thread
UUE5AIAUTOEditorSubsystem → CommandExecutor → 50+ Handler
```

### 目录结构

```
UE5AIAUTO/                    主仓库（Git）
├── Plugin/UE5AIAUTO/         UE5 C++ 插件
│   └── Source/UE5AIAUTO/
│       ├── Public/           头文件（8 个）
│       └── Private/          实现（10 个 cpp）
├── MCP/src/ue5aiauto/        Python MCP Server
│   ├── server.py             FastMCP 入口（72 工具）
│   ├── bridge_server.py      TCP 中继
│   ├── start_mcp.py          MCP 启动包装脚本
│   └── tools/                工具分类
└── docs/                     文档备份
UE5_test/DSPlugin/            UE5 测试项目
├── DSPlugin.uproject
└── Plugins/UE5AIAUTO/        插件副本（编译目标）
方案/                          方案与审查报告
工作日志/Work_log.md           操作日志
```

### C++ 插件核心类

| 类 | 文件 | 职责 |
|----|------|------|
| `UUE5AIAUTOEditorSubsystem` | EditorSubsystem | 生命周期 + Handler 注册 |
| `FUE5AIAUTOCommandExecutor` | CommandExecutor | `TQueue<Mpsc>` 线程安全命令分发 |
| `FUE5AIAUTOWebSocketClient` | WebSocketClient | TCP Client（FRunnable 线程） |
| `FUE5AIAUTOBlueprintEditor` | BlueprintEditor | 蓝图 CRUD + 节点/变量/函数/组件 |
| `FUE5AIAUTOCppTools` | CppTools | BT/BB/动画/UMG/输入/物理/音频/导航/DataTable |
| `FUE5AIAUTOAdvancedTools` | AdvancedTools | Niagara/Sequencer/MetaSound/资产管理 |
| `FUE5AIAUTOReflectionScanner` | ReflectionScanner | 反射系统扫描 + JSON 缓存 |
| `FUE5AIAUTOScreenshotCapture` | ScreenshotCapture | 视口截图 JPEG/PNG |

---

## 构建与启动

### 编译插件

```bash
# 关闭 UE5 编辑器后:
"F:/Epic Games/UE_5.7/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  DSPluginEditor Win64 Development \
  "-Project=K:/UE5 AI Plugin/UE5_test/DSPlugin/DSPlugin.uproject" -WaitMutex
```

### 启动全链路

```bash
# 1. 启动 Bridge
python "K:/UE5 AI Plugin/UE5AIAUTO/MCP/src/ue5aiauto/bridge_server.py" &

# 2. 启动 UE5 编辑器（插件自动连接 bridge）
"F:/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" \
  "K:/UE5 AI Plugin/UE5_test/DSPlugin/DSPlugin.uproject" -log &

# 3. Claude Code 自动通过 MCP 连接（配置在 ~/.claude.json）
```

### MCP 配置

生效位置：**`C:/Users/Administrator/.claude.json`**（`mcpServers` 段）

```json
"ue5aiauto": {
  "type": "stdio",
  "command": "C:/Python314/python.exe",
  "args": ["K:/UE5 AI Plugin/UE5AIAUTO/MCP/start_mcp.py"]
}
```

⚠️ `start_mcp.py` 用 `sys.path.insert` 设置 PYTHONPATH，不依赖环境变量。

---

## 关键踩坑记录

### UE5.7 API 变更

| 变更 | 影响 |
|------|------|
| `FindOrAddRow` 移除 | DataTable 改用 `FindRowUnchecked` + 手动 `AddRow` |
| `FBlackboardEntry::KeyType` → `TObjectPtr` | 需显式转换 |
| `MakeShareable(T*)` 弃用（1 参） | 改用 `TSharedPtr<T>(new T(...))` |
| UMG 头文件移出 `Blueprint/` 子目录 | include 路径去掉 `Blueprint/` 前缀 |
| `AddEmitterHandle` 加第 3 参数 `FGuid()` | Niagara 调用签名变更 |

### LoadObject 路径格式

`LoadObject`/`FindObject` 需要**点号分隔**的对象路径（`Package.AssetName`），不是斜线：
```
错: LoadObject<T>(nullptr, "/Game/AI/BT_TestAI")
对: LoadObject<T>(nullptr, "/Game/AI.BT_TestAI")
```

### Python 3.14 兼容

FastMCP 3.2.4 在 Python 3.14 上可用。启动 MCP 服务端须设置 `PYTHONPATH` 指向 `MCP/src/`。

---

## ⛔ 项目特化禁止

| # | 规则 |
|---|------|
| 1 | 禁凭记忆写 UE5 API——必须 WebFetch 官方文档（`dev.epicgames.com`）确认签名 |
| 2 | 禁蔓延修改——每次只动直接相关文件 |
| 3 | 禁风格分裂——遵循既有紧凑风格（单行 if/for、`MakeJsonObj`/`MakeJsonErr` 辅助函数） |

## ✅ 强制规则

| # | 规则 |
|---|------|
| 1 | 第三方 API 先查文档 |
| 2 | 读 `工作日志/Work_log.md` 最近 10 条 |
| 3 | 交付前自检：编译通过 + 核心用例跑通 + 规则逐条自查 |
| 4 | 修改后同步到 `UE5_test/DSPlugin/Plugins/UE5AIAUTO/` 副本 |
