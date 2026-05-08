"""MCP 工具动态注册 —— 从反射缓存批量注册 FastMCP tools。

两类工具:
    1. 语义工具（semantic_tools.py）— 高层便捷操作（create_actor, screenshot...）
    2. 反射工具（reflection_tools.py）— 低层自动生成（ue5ai_{Class}_{Func}）
"""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

from .reflection_cache import ReflectionCache

if TYPE_CHECKING:
    from fastmcp import FastMCP

logger = logging.getLogger("ue5aiauto.registry")

# 命名空间前缀
TOOL_PREFIX = "gu5"

# 批量注册每批数量（防止单次注册过多导致 MCP 客户端超时）
BATCH_SIZE = 200


class ToolRegistry:
    """管理 MCP 工具的注册/注销。"""

    def __init__(self, mcp: FastMCP, cache: ReflectionCache):
        self._mcp = mcp
        self._cache = cache
        self._registered_tools: list[str] = []

    # --- 注册 ---

    def register_semantic_tools(self) -> int:
        """注册高层语义工具（create_actor, screenshot 等）。"""
        from .tools import semantic_tools

        count = semantic_tools.register_all(self._mcp)
        self._registered_tools.extend(semantic_tools.SEMANTIC_TOOL_NAMES)
        logger.info("语义工具已注册: %d 个", count)
        return count

    def register_reflection_tools(self, max_tools: int = 0) -> int:
        """从反射缓存批量注册反射工具。

        Args:
            max_tools: 最大注册数，0 表示无限制。
        Returns:
            注册的工具数。
        """
        functions = self._cache.get_functions("all")
        if not functions:
            logger.warning("反射缓存为空，无工具注册")
            return 0

        from .tools.reflection_tools import make_tool_name, register_function_tool

        count = 0
        for func in functions[:max_tools] if max_tools else functions:
            name = make_tool_name(func["class_name"], func["name"])
            try:
                register_function_tool(self._mcp, func)
                self._registered_tools.append(name)
                count += 1
            except Exception as exc:
                logger.debug("注册失败 %s: %s", name, exc)
                continue

        logger.info("反射工具已注册: %d 个", count)
        return count

    # --- 注销 ---

    def unregister_all(self) -> None:
        """移除全部已注册工具。"""
        for name in self._registered_tools:
            try:
                self._mcp.remove_tool(name)
            except Exception as exc:
                logger.debug("注销失败 %s: %s", name, exc)
        self._registered_tools.clear()
        logger.info("全部工具已注销")

    # --- 查询 ---

    @property
    def registered_count(self) -> int:
        return len(self._registered_tools)

    def get_registered_names(self) -> list[str]:
        return list(self._registered_tools)
