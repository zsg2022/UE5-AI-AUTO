# UE5-AI-AUTO 插件 v1.2

Claude Code 通过 MCP 协议操控 UE5 编辑器——蓝图、材质、Actor、关卡、组件、接口、宏、事件分发器、物理（apply_force/stop_sound）、音频、导航（需NavMesh）、行为树/黑板、动画蓝图、UMG、Niagara、Sequencer、DataTable、资产管理。

**当前测试状态**：34/38 PASS (92%) | [覆盖率审计](.trae/documents/test-coverage-audit-2026-05-13.md)

## 项目结构

```
UE5AIAUTO/
├── README.md
├── Plugin/UE5AIAUTO/          ← UE5 C++ 插件（拷贝到项目 Plugins/）
│   ├── UE5AIAUTO.uplugin
│   └── Source/UE5AIAUTO/
│       ├── UE5AIAUTO.Build.cs
│       ├── Public/   (10 files)
│       └── Private/  (10 files)
└── MCP/                        ← Python MCP Server
    ├── requirements.txt
    ├── src/ue5aiauto/
    │   ├── server.py            ← FastMCP 入口（69 tools）
    │   └── bridge_server.py     ← TCP 中继服务器
    └── tests/
```

## 快速开始

### 1. 安装 Python 依赖
```bash
cd MCP
pip install -r requirements.txt
```

### 2. 启动 Bridge TCP Server
```bash
cd MCP
PYTHONPATH=src python -m ue5aiauto.bridge_server
```

### 3. 启动 UE5 编辑器
插件自动连接 Bridge。Output Log 出现 `[UE5AIAUTO] TCP connected` 即成功。

### 4. Claude Code MCP 配置
将 `ue5aiauto` 加入 `~/.claude.json` 的 `mcpServers`。重启 Claude Code 后 `/mcp` 可见。

## 架构

```
Claude Code ←→ MCP/stdio ←→ Python Server ←→ Bridge TCP(9876) ←→ UE5 Plugin
```## 全部命令（~72 个）

### Actor & 场景 (7)
| 命令 | 说明 |
|------|------|
| `ping` | 连通性测试 |
| `create_actor` | 创建 Actor |
| `modify_actor_property` | 修改属性 |
| `delete_actor` | 删除 Actor |
| `query_scene` | 列出场景 Actor |
| `set_viewport_camera` | 设置视口相机 |
| `screenshot` | 视口截图 |

### C++ 原生工具 (8)
| 命令 | 实现 | 说明 |
|------|:---:|------|
| `add_action_mapping` | C++ | 添加输入动作映射 |
| `add_axis_mapping` | C++ | 添加轴向映射 |
| `enable_physics` | C++ | 启用/禁用物理模拟 |
| `set_mass` | C++ | 设置质量 |
| `apply_force` | C++ | 施加力 |
| `play_sound` | C++ | 播放音效 |
| `stop_sound` | C++ | 停止音效 |
| `find_path` | C++ | 导航寻路 |

### 行为树 (7)
| 命令 | 实现 | 说明 |
|------|:---:|------|
| `bt_create` | C++ | 创建 BehaviorTree 资产 |
| `create_blackboard` | C++ | 创建 Blackboard 资产 |
| `add_bb_key` | C++ | 添加 Blackboard Key |
| `bt_add_composite` | C++ | 添加 Composite 节点 (Selector/Sequence/SimpleParallel) |
| `bt_add_task` | C++ | 添加 Task 节点 |
| `bt_add_decorator` | C++ | 添加 Decorator 节点 |
| `bt_add_service` | C++ | 添加 Service 节点 |

### 动画蓝图 (3)
| 命令 | 实现 | 说明 |
|------|:---:|------|
| `anim_add_state` | C++ | 添加动画状态机节点 |
| `anim_add_transition` | C++ | 添加状态转换 |
| `anim_set_graph_node` | C++ | 添加动画图表节点 (按类名查找) |

### UMG Widget (2)
| 命令 | 实现 | 说明 |
|------|:---:|------|
| `create_widget` | C++ | 创建 Widget Blueprint 资产 |
| `add_widget_to_canvas` | C++ | 向 CanvasPanel 添加控件 (Button/TextBlock/Image 等 8 种) |

### 材质 (1)
| 命令 | 说明 |
|------|------|
| `create_material` | 创建 PBR Fresnel 玻璃材质 |

### 反射 (1)
| 命令 | 说明 |
|------|------|
| `scan_reflection` | 扫描全部反射 API（133K 函数） |

### 资产管理 (3)
| 命令 | 实现 | 说明 |
|------|:---:|------|
| `copy_asset` | C++ | 复制资产到新路径 |
| `delete_asset` | C++ | 删除资产 |
| `list_assets` | C++ | 列出指定路径资产 |

### Niagara / Sequencer (7)
| 命令 | 说明 |
|------|------|
| `create_niagara` | 创建 Niagara 系统 |
| `add_emitter` | 添加 Niagara Emitter 到系统 |
| `set_niagara_param` | 设置 Niagara 参数值 |
| `create_sequence` | 创建 Level Sequence |
| `add_transform_track` | 添加 3D Transform 轨道到 Actor 绑定 |
| `set_keyframe` | 设置位置关键帧 |

### PCG (已禁用)
PCG 功能因项目未包含 PCG 插件而禁用。如需启用，需在 Build.cs 添加 `"PCG"` 依赖并去除 `#if 0` 包裹。
| 命令 | 说明 |
|------|------|
| `create_pcg` | 创建 PCG 图（已禁用） |

### DataTable (3)
| 命令 | 实现 | 说明 |
|------|:---:|------|
| `create_datatable` | C++ | 创建 DataTable 资产（需指定行结构体） |
| `add_datatable_row` | C++ | 添加/更新 DataTable 行 |
| `get_datatable_row` | C++ | 读取 DataTable 行（返回 JSON） |

### 蓝图 CRUD (6)
| 命令 | 说明 |
|------|------|
| `bp_create` / `bp_open` / `bp_compile` / `bp_save` / `bp_list` / `bp_list_graphs` |

### 蓝图节点 (6)
| 命令 | 说明 |
|------|------|
| `bp_list_nodes` / `bp_create_node` / `bp_remove_node` / `bp_connect_pins` / `bp_disconnect_pin` / `bp_set_pin_default` |

### 蓝图变量 (4)
| 命令 | 说明 |
|------|------|
| `bp_list_variables` / `bp_create_variable` / `bp_remove_variable` / `bp_set_variable_default` |

### 蓝图函数 (3)
| 命令 | 说明 |
|------|------|
| `bp_list_functions` / `bp_create_function` / `bp_remove_function` |

### 蓝图组件 (3)
| 命令 | 说明 |
|------|------|
| `bp_list_components` / `bp_add_component` / `bp_remove_component` |

### 蓝图接口 (3)
| 命令 | 说明 |
|------|------|
| `bp_list_interfaces` / `bp_add_interface` / `bp_remove_interface` |

### 蓝图宏 (3)
| 命令 | 说明 |
|------|------|
| `bp_list_macros` / `bp_create_macro` / `bp_remove_macro` |

### 事件分发器 (2)
| 命令 | 说明 |
|------|------|
| `bp_add_event_dispatcher` / `bp_remove_event_dispatcher` |

### 工具 (1)
| 命令 | 说明 |
|------|------|
| `bp_list_node_classes` | 列出 255 种可用节点类型 |

## 已知限制

- 变量修改后需在 UE 编辑器内手动 Compile
- 首次材质创建 shader 编译耗时 ~30s
- 仅 UE 5.7+ Windows x64
- Niagara 系统图节点编辑需在对应编辑器中进行
- PCG 图节点编辑需 PCG 插件支持（当前已禁用）
- AnimBlueprint 转换条件表达式暂不支持（Condition 参数预留）

## 技术要点

- **通信**：UE5 原生 `FSocket` TCP（绕过 libwebsockets Windows bug）
- **线程**：`FRunnable` 后台线程 + `FScopeLock` 保护 Socket
- **缓存**：`TMap<FString, UBlueprint*>` 内存缓存
- **Bridge**：单文件 TCP 中继，大小写不敏感 UUID
- **健壮性**：GEditor 空指针防护、Socket 生命周期锁保护
- **配置**：支持 GEditorIni 配置 Bridge Host/Port，默认 127.0.0.1:9876

## 版本

v1.2 — 2026-05-12
  - 新增 BehaviorTree/Blackboard 节点编辑 (7 命令)
  - 新增 Animation Blueprint 状态机编辑 (3 命令)
  - 新增 UMG Widget 蓝图编辑 (2 命令)
  - 新增 DataTable 创建与读写 (3 命令)
  - 新增 Niagara add_emitter/set_niagara_param (2 命令)
  - 新增 Sequencer add_transform_track/set_keyframe (2 命令)
  - PCG 模块禁用（项目缺少 PCG 插件）
  - 命令总数 55 → 72