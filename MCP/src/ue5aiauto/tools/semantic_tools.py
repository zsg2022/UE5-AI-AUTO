"""语义工具 —— 高层便捷操作。

规则:
    - 简单操作（create_actor, screenshot）自动执行
    - 高风险操作（delete_actor）在混合模式下先确认
"""

from __future__ import annotations

import logging

logger = logging.getLogger("ue5aiauto.tools.semantic")

SEMANTIC_TOOL_NAMES: list[str] = []


def register_all(mcp) -> int:
    """注册所有语义工具，返回注册数量。"""
    tools = [
        (create_actor, "在关卡中创建 Actor。class_name 如 'StaticMeshActor', 'PointLight'"),
        (modify_actor_property, "修改指定 Actor 的属性值"),
        (delete_actor, "删除 Actor。confirm 必须为 true 才会执行"),
        (query_scene, "列出当前关卡所有 Actor（名/类型/位置）"),
        (set_viewport_camera, "设置编辑器视口相机位置和旋转"),
        (screenshot, "捕获编辑器视口截图，返回 Base64 编码的图片"),
    ]

    for func, desc in tools:
        SEMANTIC_TOOL_NAMES.append(func.__name__)
        mcp.tool()(func)

    return len(SEMANTIC_TOOL_NAMES)


# ═══════════════════════════════════════════
# Actor 操控
# ═══════════════════════════════════════════

async def create_actor(
    class_name: str,
    location: dict = None,
    rotation: dict = None,
    scale: dict = None,
) -> dict:
    """创建 Actor 并放置到关卡中。

    参数:
        class_name: Actor 类名，如 'StaticMeshActor', 'PointLight', 'CameraActor'
        location: 位置 {"x": 0, "y": 0, "z": 0}
        rotation: 旋转 {"pitch": 0, "yaw": 0, "roll": 0}
        scale: 缩放 {"x": 1, "y": 1, "z": 1}
    """
    from ..server import tcp_server

    if tcp_server is None or not tcp_server.is_connected:
        return {"error": "UE5 编辑器未连接", "error_code": -5}

    params = {
        "class_name": class_name,
        "location": location or {"x": 0, "y": 0, "z": 0},
        "rotation": rotation or {"pitch": 0, "yaw": 0, "roll": 0},
        "scale": scale or {"x": 1, "y": 1, "z": 1},
    }
    return await tcp_server.send_command("create_actor", params)


async def modify_actor_property(
    actor_name: str,
    property_name: str,
    value,
) -> dict:
    """修改指定 Actor 的属性值。

    参数:
        actor_name: Actor 名称（如 'StaticMeshActor_0'）
        property_name: 属性名（如 'bHidden', 'Mobility'）
        value: 新值
    """
    from ..server import tcp_server

    if tcp_server is None or not tcp_server.is_connected:
        return {"error": "UE5 编辑器未连接", "error_code": -5}

    return await tcp_server.send_command("modify_actor_property", {
        "actor_name": actor_name,
        "property_name": property_name,
        "value": value,
    })


async def delete_actor(actor_name: str, confirm: bool = False) -> dict:
    """删除 Actor。需设置 confirm=true 才会执行。

    参数:
        actor_name: Actor 名称
        confirm: 必须为 true 才执行删除
    """
    from ..server import tcp_server

    if tcp_server is None or not tcp_server.is_connected:
        return {"error": "UE5 编辑器未连接", "error_code": -5}

    if not confirm:
        return {
            "warning": f"即将删除 '{actor_name}'",
            "message": "请确认删除操作：设置 confirm=true 后重试",
        }

    return await tcp_server.send_command("delete_actor", {
        "actor_name": actor_name,
    })


async def query_scene(filter_class: str = "") -> dict:
    """列出当前关卡所有 Actor。

    参数:
        filter_class: 可选过滤类名（如 'StaticMeshActor'），留空则全部列出
    """
    from ..server import tcp_server

    if tcp_server is None or not tcp_server.is_connected:
        return {"error": "UE5 编辑器未连接", "error_code": -5}

    return await tcp_server.send_command("query_scene", {
        "filter_class": filter_class,
    })


# ═══════════════════════════════════════════
# 关卡操控
# ═══════════════════════════════════════════

async def set_viewport_camera(
    location: dict,
    rotation: dict = None,
) -> dict:
    """设置编辑器视口相机位置。

    参数:
        location: 位置 {"x": 0, "y": 0, "z": 1000}
        rotation: 旋转 {"pitch": -90, "yaw": 0, "roll": 0}（俯视）
    """
    from ..server import tcp_server

    if tcp_server is None or not tcp_server.is_connected:
        return {"error": "UE5 编辑器未连接", "error_code": -5}

    return await tcp_server.send_command("set_viewport_camera", {
        "location": location,
        "rotation": rotation or {"pitch": 0, "yaw": 0, "roll": 0},
    })


# ═══════════════════════════════════════════
# 截图
# ═══════════════════════════════════════════

async def screenshot(
    width: int = 1920,
    height: int = 1080,
    quality: int = 85,
    format: str = "jpeg",
) -> str:
    """捕获编辑器视口截图。

    参数:
        width: 宽度（默认 1920）
        height: 高度（默认 1080）
        quality: JPEG 质量 1-100（默认 85）
        format: "jpeg" 或 "png"

    返回:
        Base64 编码的图片字符串。
    """
    from ..server import tcp_server

    if tcp_server is None or not tcp_server.is_connected:
        return "错误: UE5 编辑器未连接"

    import base64

    result = await tcp_server.send_command("screenshot", {
        "width": width,
        "height": height,
        "quality": quality,
        "format": format,
    })

    # WebSocket 二进制帧会通过 tcp_server._pending 机制处理
    if "binary" in result:
        return base64.b64encode(result["binary"]).decode("ascii")
    if "base64" in result:
        return result["base64"]
    return str(result)
