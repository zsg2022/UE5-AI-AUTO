"""元工具 —— 查询可用 API、刷新缓存、查看状态。"""

from __future__ import annotations

import logging

logger = logging.getLogger("ue5aiauto.tools.meta")


def register_all(mcp) -> int:
    """注册所有元工具，返回注册数量。"""
    META_TOOLS.append("list_available_apis")
    mcp.tool()(list_available_apis)

    META_TOOLS.append("refresh_reflection_cache")
    mcp.tool()(refresh_reflection_cache)

    META_TOOLS.append("get_engine_info")
    mcp.tool()(get_engine_info)

    return len(META_TOOLS)


META_TOOLS: list[str] = []


async def list_available_apis(category: str = "all") -> str:
    """列出所有可用的 UE5 API 函数。

    参数:
        category: 筛选类别。可选: all, actor, blueprint, level, material, animation

    返回:
        函数列表及数量。
    """
    from ..server import tcp_server
    from ..reflection_cache import ReflectionCache

    # 尝试从缓存读取
    import os
    cache_path = os.environ.get(
        "ue5aiauto_CACHE_PATH",
        "reflection_cache.json",
    )
    cache = ReflectionCache(cache_path)
    cache.load()

    if not cache.is_valid:
        return "反射缓存不可用。请先启动 UE5 编辑器生成缓存。"

    functions = cache.get_functions(category)
    total = cache.function_count

    lines = [f"=== UE5 API: {category} ({len(functions)}/{total}) ==="]
    for func in functions[:50]:  # 最多展示 50 个
        params = ", ".join(
            f"{p.get('name', '?')}:{p.get('type', '?')}"
            for p in func.get("parameters", [])
            if not p.get("is_return", False)
        )
        lines.append(
            f"  {func['class_name']}::{func['name']}({params})"
        )
    if len(functions) > 50:
        lines.append(f"  ... 还有 {len(functions) - 50} 个函数")
    return "\n".join(lines)


async def refresh_reflection_cache() -> str:
    """请求 UE5 重新扫描反射并更新缓存。"""
    from ..server import tcp_server

    if tcp_server is None:
        return "错误: MCP Server 未初始化"

    if not tcp_server.is_connected:
        return "错误: UE5 编辑器未连接。请先启动 UE5 编辑器"

    try:
        result = await tcp_server.send_command("scan_reflection", {})
        return f"反射扫描已触发: {result}"
    except Exception as e:
        return f"反射扫描失败: {e}"


async def get_engine_info() -> str:
    """获取 UE5 引擎版本和模块信息。"""
    from ..reflection_cache import ReflectionCache
    import os

    cache_path = os.environ.get(
        "ue5aiauto_CACHE_PATH",
        "reflection_cache.json",
    )
    cache = ReflectionCache(cache_path)
    cache.load()

    if not cache.is_valid:
        return "反射缓存不可用"

    return (
        f"引擎版本: {cache.engine_version}\n"
        f"模块哈希: {cache.module_hash}\n"
        f"函数数量: {cache.function_count}\n"
        f"类数量: {len(cache.get_class_names())}\n"
        f"枚举数量: {len(cache.get_enum_names())}"
    )
