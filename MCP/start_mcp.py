"""MCP 启动入口——设置 PYTHONPATH 并启动 FastMCP server."""
import sys, os
sys.path.insert(0, r"K:\UE5 AI Plugin\UE5AIAUTO\MCP\src")
os.environ["UE5AIAUTO_CACHE_PATH"] = r"K:\UE5 AI Plugin\UE5_test\DSPlugin\Saved\UE5AIAUTO\reflection_cache.json"
from ue5aiauto.server import mcp
mcp.run(transport="stdio")
