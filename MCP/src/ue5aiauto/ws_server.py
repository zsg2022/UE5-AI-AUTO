"""WebSocket Server — Python 侧作为 Server，UE5 插件作为 Client 连接。

协议:
    - JSON 文本帧: 命令/响应
    - 二进制帧: 截图等大块数据（前 4 字节为 payload 大小）
    - PING/PONG: RFC 6455 心跳
"""

from __future__ import annotations

import asyncio
import json
import logging
import uuid
from typing import Optional

import websockets
from websockets.asyncio.server import ServerConnection

from .config import Config

logger = logging.getLogger("ue5aiauto.ws")

# 错误码（与 UE5 侧 Eue5aiautoErrorCode 对齐）
E_OK = 0
E_INVALID_ARG = -1
E_NOT_FOUND = -2
E_TYPE_MISMATCH = -3
E_TIMEOUT = -4
E_DISCONNECTED = -5
E_INTERNAL = -99


class GU5WebSocketServer:
    """WebSocket Server：接收 UE5 插件连接，双向转发命令/响应。"""

    def __init__(self, config: Config):
        self._config = config
        self._server = None
        self._client: Optional[ServerConnection] = None
        self._pending: dict[str, asyncio.Future] = {}
        self._heartbeat_task: Optional[asyncio.Task] = None

    # --- 生命周期 ---

    async def start(self) -> None:
        """启动 WebSocket Server，等待 UE5 插件连接。"""
        self._server = await websockets.serve(
            self._on_connect,
            self._config.host,
            self._config.port,
        )
        logger.info(
            "WebSocket Server 已启动 ws://%s:%d",
            self._config.host,
            self._config.port,
        )

    async def stop(self) -> None:
        """停止 WebSocket Server，断开客户端连接。"""
        if self._heartbeat_task:
            self._heartbeat_task.cancel()
            self._heartbeat_task = None
        if self._client:
            await self._client.close()
            self._client = None
        if self._server:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        # 清理未完成的 pending requests
        for future in self._pending.values():
            if not future.done():
                future.set_exception(ConnectionError("Server stopped"))
        self._pending.clear()
        logger.info("WebSocket Server 已停止")

    # --- 连接处理 ---

    async def _on_connect(self, websocket: ServerConnection) -> None:
        """客户端连接回调（一次只允许一个 UE5 实例）。"""
        if self._client is not None:
            logger.warning("已有客户端连接，关闭旧连接")
            await self._client.close()
        self._client = websocket
        client_addr = websocket.remote_address
        logger.info("UE5 客户端已连接: %s", client_addr)

        # 启动心跳
        self._heartbeat_task = asyncio.create_task(self._heartbeat_loop())

        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    await self._handle_binary(message)
                else:
                    await self._handle_text(message)
        except websockets.ConnectionClosed:
            logger.info("客户端断开连接: %s", client_addr)
        finally:
            self._client = None
            if self._heartbeat_task:
                self._heartbeat_task.cancel()
                self._heartbeat_task = None

    async def _heartbeat_loop(self) -> None:
        """定期发送 PING 保持连接活跃。"""
        try:
            while self._client:
                await asyncio.sleep(self._config.heartbeat_interval)
                if self._client:
                    try:
                        await self._client.ping()
                    except websockets.ConnectionClosed:
                        break
        except asyncio.CancelledError:
            pass

    # --- 消息处理 ---

    async def _handle_text(self, raw: str) -> None:
        """处理 JSON 文本消息（命令结果 / 事件通知）。"""
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            logger.error("JSON 解析失败: %s", raw[:200])
            return

        msg_id = msg.get("id")
        if msg_id and msg_id in self._pending:
            future = self._pending.pop(msg_id)
            if "error" in msg and msg["error"]:
                error_code = msg.get("error_code", E_INTERNAL)
                future.set_exception(
                    CommandError(msg["error"], error_code)
                )
            else:
                future.set_result(msg.get("result", {}))
        else:
            logger.debug("收到非请求消息: %s", msg.get("method", "unknown"))

    async def _handle_binary(self, data: bytes) -> None:
        """处理二进制消息（截图等）。"""
        # 前 4 字节为关联的 request_id 长度，之后为 request_id，剩余为 payload
        if len(data) < 4:
            return
        id_len = int.from_bytes(data[:4], "big")
        request_id = data[4:4 + id_len].decode("utf-8")
        payload = data[4 + id_len:]

        if request_id in self._pending:
            future = self._pending.pop(request_id)
            future.set_result({"binary": payload})
        else:
            logger.debug("收到未知二进制数据，request_id=%s", request_id)

    # --- 对外接口 ---

    async def send_command(self, method: str, params: dict | None = None) -> dict:
        """向 UE5 发送命令并等待响应（30s 超时）。

        Raises:
            CommandError: UE5 返回错误时
            ConnectionError: 无客户端连接时
            TimeoutError: 超时时
        """
        if self._client is None:
            raise ConnectionError("没有 UE5 客户端连接")

        request_id = str(uuid.uuid4())
        future: asyncio.Future = asyncio.get_event_loop().create_future()
        self._pending[request_id] = future

        message = json.dumps({
            "id": request_id,
            "method": method,
            "params": params or {},
        })

        try:
            await self._client.send(message)
            result = await asyncio.wait_for(future, timeout=self._config.command_timeout)
            return result
        except asyncio.TimeoutError:
            self._pending.pop(request_id, None)
            raise TimeoutError(f"命令 '{method}' 超时（{self._config.command_timeout}s）")
        except CommandError:
            self._pending.pop(request_id, None)
            raise

    @property
    def is_connected(self) -> bool:
        return self._client is not None


class CommandError(Exception):
    """UE5 侧返回的命令执行错误。"""

    def __init__(self, message: str, code: int = E_INTERNAL):
        super().__init__(message)
        self.code = code
