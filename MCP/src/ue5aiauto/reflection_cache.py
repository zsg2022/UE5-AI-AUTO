"""反射缓存 —— 加载/校验/查询 UE5 反射扫描 JSON 数据。

缓存文件由 UE5 插件生成（{Project}/Saved/ue5aiauto/reflection_cache.json）。
Python MCP Server 启动时加载并动态注册 MCP 工具。
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Optional

logger = logging.getLogger("ue5aiauto.cache")


class ReflectionCache:
    """UE5 反射数据缓存。"""

    def __init__(self, cache_path: str):
        self._path = Path(cache_path)
        self._data: Optional[dict] = None

    @property
    def is_valid(self) -> bool:
        return self._data is not None and "functions" in self._data

    @property
    def function_count(self) -> int:
        if not self._data:
            return 0
        return self._data.get("function_count", 0)

    @property
    def engine_version(self) -> str:
        if not self._data:
            return ""
        return self._data.get("engine_version", "")

    @property
    def module_hash(self) -> str:
        if not self._data:
            return ""
        return self._data.get("module_list_hash", "")

    def load(self) -> dict:
        """加载 JSON 缓存。成功返回 dict，失败返回空 dict。"""
        if not self._path.exists():
            logger.warning("缓存文件不存在: %s", self._path)
            logger.warning("请先启动 UE5 编辑器生成反射缓存")
            return {}

        try:
            text = self._path.read_text(encoding="utf-8")
            self._data = json.loads(text)
        except (json.JSONDecodeError, OSError) as e:
            logger.error("缓存加载失败: %s", e)
            self._data = None
            return {}

        func_count = self._data.get("function_count", 0)
        eng_ver = self._data.get("engine_version", "?")
        logger.info(
            "反射缓存已加载: %d 函数, 引擎版本 %s",
            func_count, eng_ver,
        )
        return self._data

    def get_functions(self, category: str = "all") -> list[dict]:
        """获取函数列表。可筛选 category: all/actor/blueprint/level。"""
        if not self._data:
            return []

        functions = self._data.get("functions", [])
        if category == "all":
            return functions

        # 按类名分类过滤（简单规则）
        class_keywords = {
            "actor": ["Actor", "Pawn", "Character", "Controller"],
            "blueprint": ["Blueprint", "K2Node", "EdGraph", "GraphNode"],
            "level": ["Level", "World", "Streaming"],
            "material": ["Material", "Texture", "Shader"],
            "animation": ["Anim", "Skeletal", "Montage", "BlendSpace"],
        }

        keywords = class_keywords.get(category, [])
        if not keywords:
            return functions

        return [
            f for f in functions
            if any(kw in f.get("class_name", "") for kw in keywords)
        ]

    def get_class_names(self) -> list[str]:
        if not self._data:
            return []
        return self._data.get("class_names", [])

    def get_enum_names(self) -> list[str]:
        if not self._data:
            return []
        return self._data.get("enum_names", [])
