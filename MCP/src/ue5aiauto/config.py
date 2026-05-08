"""配置加载：CLI > 环境变量 > config.json > 默认值。

用法:
    from ue5aiauto.config import Config
    cfg = Config.load()           # 自动按优先级加载
    cfg = Config.load(port=9877)  # CLI 覆盖
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


@dataclass
class Config:
    # WebSocket Server
    host: str = "127.0.0.1"
    port: int = 9876

    # 心跳与超时
    heartbeat_interval: float = 10.0
    command_timeout: float = 30.0

    # 重连（Python Server 侧不重连；UE5 Client 侧负责重连）
    ping_interval: float = 10.0

    # 反射缓存路径（相对于 UE5 项目 Saved 目录）
    cache_filename: str = "reflection_cache.json"

    # 日志
    log_level: str = "INFO"

    @classmethod
    def load(
        cls,
        host: Optional[str] = None,
        port: Optional[int] = None,
        config_path: Optional[str] = None,
    ) -> Config:
        """按优先级加载：CLI参数 > 环境变量 > config.json > 默认值。"""
        cfg = cls()

        # 1. config.json（如果存在）
        if config_path:
            cfg._load_json(config_path)
        else:
            default_json = Path(os.getcwd()) / "config.json"
            if default_json.exists():
                cfg._load_json(str(default_json))

        # 2. 环境变量
        if os.getenv("ue5aiauto_HOST"):
            cfg.host = os.getenv("ue5aiauto_HOST")
        if os.getenv("ue5aiauto_PORT"):
            cfg.port = int(os.getenv("ue5aiauto_PORT", ""))
        if os.getenv("ue5aiauto_HEARTBEAT"):
            cfg.heartbeat_interval = float(os.getenv("ue5aiauto_HEARTBEAT", ""))
        if os.getenv("ue5aiauto_TIMEOUT"):
            cfg.command_timeout = float(os.getenv("ue5aiauto_TIMEOUT", ""))
        if os.getenv("ue5aiauto_LOG_LEVEL"):
            cfg.log_level = os.getenv("ue5aiauto_LOG_LEVEL")

        # 3. CLI 参数（最高优先级）
        if host is not None:
            cfg.host = host
        if port is not None:
            cfg.port = port

        return cfg

    def _load_json(self, path: str) -> None:
        try:
            with open(path, encoding="utf-8") as f:
                data = json.load(f)
            if "host" in data:
                self.host = data["host"]
            if "port" in data:
                self.port = data["port"]
            if "heartbeat_interval" in data:
                self.heartbeat_interval = data["heartbeat_interval"]
            if "command_timeout" in data:
                self.command_timeout = data["command_timeout"]
            if "log_level" in data:
                self.log_level = data["log_level"]
        except (FileNotFoundError, json.JSONDecodeError):
            pass
