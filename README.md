# UE5-AI-AUTO 插件 v1.0

Claude Code 通过 MCP 协议操控 UE5 编辑器——蓝图、材质、Actor、关卡、组件、接口、宏、事件分发器。

## 项目结构

```
UE5AIAUTO\
├── README.md
├── Plugin\UE5AIAUTO\          ← UE5 C++ 插件（拷贝到项目 Plugins\）
│   ├── UE5AIAUTO.uplugin
│   └── Source\UE5AIAUTO\
│       ├── UE5AIAUTO.Build.cs
│       ├── Public\   (8 files)
│       └── Private\  (8 files)
└── MCP\                        ← Python MCP Server
    ├── requirements.txt
    ├── src\ue5aiauto\
    │   ├── server.py            ← FastMCP 入口（40 tools）
    │   └── bridge_server.py     ← TCP 中继服务器
    └── tests\
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
# 输出: Bridge on 127.0.0.1:9876
```

### 3. 启动 UE5 编辑器
插件自动连接 Bridge。Output Log 出现 `[UE5AIAUTO] TCP connected` 即成功。

### 4. Claude Code MCP 配置
已将 `ue5aiauto` 加入 `~/.claude.json` 的 `mcpServers`。重启 Claude Code 后 `/mcp` 可见。

## 架构

```
Claude Code ←→ MCP/stdio ←→ Python Server ←→ Bridge TCP(9876) ←→ UE5 Plugin
                                                                    │
                                                              UEditorSubsystem
                                                              CommandExecutor (40 handlers)
                                                              BlueprintEditor / MaterialEditor / ...
```

## 全部 40 个 MCP 命令

### Actor & 场景 (7)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `ping` | — | ✅ |
| `create_actor` | class_name, location, rotation, scale | ✅ |
| `modify_actor_property` | actor_name, property_name, value | ✅ |
| `delete_actor` | actor_name, confirm | ✅ |
| `query_scene` | filter_class | ✅ |
| `set_viewport_camera` | location, rotation | ✅ |
| `screenshot` | width, height, quality, fmt | ✅ |

### 材质 (1)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `create_material` | asset_path, base_color_hex, opacity, roughness, metallic, specular | ✅ |

### 反射 (1)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `scan_reflection` | — | ✅ 133K 函数 |

### 蓝图 CRUD (6)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_create` | path, parent_class | ✅ |
| `bp_open` | path | ✅ |
| `bp_compile` | path | ✅ |
| `bp_save` | path | ✅ |
| `bp_list` | path | ✅ |
| `bp_list_graphs` | path | ✅ |

### 蓝图节点 (6)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_nodes` | path, graph | ✅ |
| `bp_create_node` | path, graph, node_class, x, y, defaults | ✅ |
| `bp_remove_node` | path, graph, node_id | ✅ |
| `bp_connect_pins` | path, graph, src_node, src_pin, dst_node, dst_pin | ✅ |
| `bp_disconnect_pin` | path, graph, node_id, pin | ✅ |
| `bp_set_pin_default` | path, graph, node_id, pin, value | ✅ |

### 蓝图变量 (4)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_variables` | path | ✅ |
| `bp_create_variable` | path, name, type, is_array | ✅ |
| `bp_remove_variable` | path, name | ✅ |
| `bp_set_variable_default` | path, name, value | ✅ |

### 蓝图函数 (3)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_functions` | path | ✅ |
| `bp_create_function` | path, name, inputs, outputs | ✅ |
| `bp_remove_function` | path, name | ✅ |

### 蓝图组件 (3)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_components` | path | ✅ |
| `bp_add_component` | path, component_class, variable_name | ✅ |
| `bp_remove_component` | path, name | ✅ |

### 蓝图接口 (3)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_interfaces` | path | ✅ |
| `bp_add_interface` | path, interface_class | ✅ |
| `bp_remove_interface` | path, interface_class | ✅ |

### 蓝图宏 (3)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_macros` | path | ✅ |
| `bp_create_macro` | path, name | ✅ |
| `bp_remove_macro` | path, name | ✅ |

### 事件分发器 (2)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_add_event_dispatcher` | path, name | ✅ |
| `bp_remove_event_dispatcher` | path, name | ✅ |

### 工具 (1)
| 命令 | 参数 | 验证 |
|------|------|:---:|
| `bp_list_node_classes` | — | ✅ 255 种 |

## 已知限制

- **变量修改后编译**：`bp_compile` 在变量直接写 `NewVariables` 数组后可能不稳定。建议在 UE 编辑器内手动 Compile。
- **首次材质创建**：shader 编译耗时 ~30s，Bridge 可能超时但文件已落盘。
- **仅 UE 5.7+ Windows x64**。
- **蓝图变量写入**：绕过 `AddMemberVariable` 直接操作 `NewVariables` 数组，标记 `MarkBlueprintAsModified`。需手动编译。

## 技术要点

- **通信**：UE5 原生 `FSocket` TCP（绕过 libwebsockets 的 Windows `getprotobyname` bug）
- **线程**：`FRunnable` 后台线程读 TCP → `AsyncTask` 投递 Game Thread
- **缓存**：`TMap<FString, UBlueprint*>` 内存缓存
- **JSON**：`TCondensedJsonPrintPolicy`，`\n` 帧分隔
- **Bridge**：单文件 TCP 中继，大小写不敏感 UUID 匹配

## 版本

v1.0 — 2026-05-08 — 40 工具全部 MCP 实测通过
