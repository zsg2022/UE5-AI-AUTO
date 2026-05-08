"""TCP Server — Python 侧 TCP Server，UE5 插件作为 Client 连接。

协议（替代原 WebSocket）:
    JSON lines: 每条消息以 \n 结尾
    命令: {"id":"uuid","method":"...","params":{...}}
    响应: {"id":"uuid","result":{...},"error":null}
"""

from __future__ import annotations

import asyncio
import json
import logging
import uuid
from typing import Optional

from .config import Config

logger = logging.getLogger("ue5aiauto.tcp")

E_OK = 0
E_INVALID_ARG = -1
E_NOT_FOUND = -2
E_TYPE_MISMATCH = -3
E_TIMEOUT = -4
E_DISCONNECTED = -5
E_INTERNAL = -99


class GU5TcpServer:
    """TCP Server：接收 UE5 插件连接，双向转发 JSON 命令/响应。"""

    def __init__(self, config: Config):
        self._config = config
        self._server: Optional[asyncio.AbstractServer] = None
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._pending: dict[str, asyncio.Future] = {}
        self._lock = asyncio.Lock()

    async def start(self) -> None:
        self._server = await asyncio.start_server(
            self._handle_client, self._config.host, self._config.port)
        logger.info("TCP Server listening on %s:%d", self._config.host, self._config.port)

    async def stop(self) -> None:
        for fut in self._pending.values():
            if not fut.done():
                fut.set_exception(ConnectionError("Server stopped"))
        self._pending.clear()
        if self._writer:
            self._writer.close()
        if self._server:
            self._server.close()
            await self._server.wait_closed()
        logger.info("TCP Server stopped")

    async def _handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        addr = writer.get_extra_info('peername')
        logger.info("UE5 connected from %s", addr)

        async with self._lock:
            if self._writer:
                logger.warning("Closing previous connection")
                self._writer.close()
            self._reader = reader
            self._writer = writer

        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                try:
                    msg = json.loads(line.decode('utf-8'))
                except json.JSONDecodeError:
                    logger.error("Invalid JSON: %s", line[:200])
                    continue

                msg_id = msg.get("id")
                if msg_id and msg_id in self._pending:
                    fut = self._pending.pop(msg_id)
                    if msg.get("error"):
                        fut.set_exception(
                            CommandError(msg["error"], msg.get("error_code", E_INTERNAL)))
                    else:
                        fut.set_result(msg.get("result", {}))
                else:
                    logger.debug("Unsolicited message: %s", msg.get("method", "?"))
        except (ConnectionResetError, asyncio.IncompleteReadError):
            logger.info("UE5 disconnected")
        finally:
            async with self._lock:
                if self._writer is writer:
                    self._reader = None
                    self._writer = None
            writer.close()

    async def send_command(self, method: str, params: dict | None = None) -> dict:
        async with self._lock:
            if self._writer is None:
                raise ConnectionError("UE5 not connected")

            request_id = str(uuid.uuid4())
            fut: asyncio.Future = asyncio.get_event_loop().create_future()
            self._pending[request_id] = fut

            msg = json.dumps({"id": request_id, "method": method, "params": params or {}})
            self._writer.write((msg + "\n").encode('utf-8'))
            await self._writer.drain()

        try:
            return await asyncio.wait_for(fut, timeout=self._config.command_timeout)
        except asyncio.TimeoutError:
            self._pending.pop(request_id, None)
            raise TimeoutError(f"Command '{method}' timed out ({self._config.command_timeout}s)")

    @property
    def is_connected(self) -> bool:
        return self._writer is not None


class CommandError(Exception):
    def __init__(self, message: str, code: int = E_INTERNAL):
        super().__init__(message)
        self.code = code
