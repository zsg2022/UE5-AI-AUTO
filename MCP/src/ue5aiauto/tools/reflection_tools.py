"""反射工具 —— 从反射缓存自动生成 MCP tool。

命名规则: ue5ai_{ClassName}_{FunctionName}
"""
from __future__ import annotations

import logging

logger = logging.getLogger("ue5aiauto.tools.reflection")

TOOL_PREFIX = "gu5"


def make_tool_name(class_name: str, func_name: str) -> str:
    """生成 MCP tool 名称。"""
    # 去掉 UE5 类型前缀（A/U/F/E/...）
    clean_class = class_name[1:] if class_name[0] in "AUFEST" and len(class_name) > 1 else class_name
    clean_func = func_name[1:] if func_name[0] in "KF" and len(func_name) > 1 else func_name
    return f"{TOOL_PREFIX}_{clean_class}_{clean_func}"


def register_function_tool(mcp, func_info: dict) -> None:
    """为单个反射函数注册 MCP tool。

    Args:
        mcp: FastMCP 实例
        func_info: {"name": "...", "class_name": "...", "parameters": [...], "module_name": "..."}
    """
    name = make_tool_name(func_info["class_name"], func_info["name"])
    class_name = func_info["class_name"]
    func_name = func_info["name"]
    params = func_info.get("parameters", [])

    # 构建函数签名描述
    param_descs = []
    for p in params:
        if p.get("is_return"):
            continue
        param_descs.append(f"{p['name']}: {p['type']}")

    description = (
        f"调用 {class_name}::{func_name}({', '.join(param_descs)})。"
        f"模块: {func_info.get('module_name', 'Unknown')}"
    )

    # 动态创建 tool 函数
    async def tool_func(**kwargs):
        from ..server import tcp_server
        from ..tcp_server import CommandError, ConnectionError, TimeoutError

        if tcp_server is None:
            return {"error": "MCP Server 未初始化", "error_code": -99}

        try:
            result = await tcp_server.send_command(
                "call_function",
                {
                    "class_name": class_name,
                    "function_name": func_name,
                    "arguments": kwargs,
                },
            )
            return result
        except ConnectionError:
            return {"error": "UE5 未连接", "error_code": -5}
        except TimeoutError:
            return {"error": "命令超时", "error_code": -4}
        except CommandError as e:
            return {"error": str(e), "error_code": e.code}

    # 重命名并注册
    tool_func.__name__ = name
    tool_func.__doc__ = description
    tool_func.__annotations__ = {"return": dict}

    mcp.tool(name=name, description=description)(tool_func)
