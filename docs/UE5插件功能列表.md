# UE5 AI 插件功能列表

> 本插件让 AI（Claude Code）通过 MCP 协议直接操控 UE5 编辑器。以下是 AI 当前能帮人类做的事情。

---

## 一、场景编辑（Actor 操作）

| 功能 | 说明 |
|------|------|
| 创建 Actor | 指定类名在场景中生成 Actor（自动补 A 前缀） |
| 删除 Actor | 按名称删除场景中的 Actor |
| 查询场景 | 列出当前关卡所有 Actor（名称/类/位置） |
| 修改 Actor 属性 | 运行时修改 Actor 的 bool/float/int/string 属性 |
| 设置视口相机 | 移动编辑器视口相机到指定位置和角度 |

---

## 二、蓝图操作（可视化编程）

| 功能 | 说明 |
|------|------|
| 创建蓝图 | 指定父类和路径创建 Blueprint 资产 |
| 打开/编译/保存蓝图 | 完整的蓝图资产生命周期 |
| 列出蓝图 | 浏览指定路径下的所有蓝图 |
| 增删蓝图节点 | 在 EventGraph 等图表中添加/删除节点 |
| 连接/断开引脚 | 节点间连线/断线操作 |
| 设置引脚默认值 | 修改节点输入引脚的默认值 |
| 列出节点 | 列出图表中所有节点的 ID/类型 |
| 创建/删除变量 | 添加成员变量（支持数组） |
| 设置变量默认值 | 修改变量默认值 |
| 创建/删除函数 | 添加自定义函数（可选输入输出参数） |
| 列出函数/图表 | 查看蓝图内所有函数和图表 |
| 增删组件 | 添加/移除 Actor 组件（如 StaticMesh） |
| 接口管理 | 添加/移除/列出蓝图实现的接口 |
| 宏管理 | 创建/删除/列出蓝图宏 |
| 事件分发器 | 添加/删除事件分发器 |

---

## 三、行为树与 AI

| 功能 | 说明 |
|------|------|
| 创建 BehaviorTree | 新建行为树资产 |
| 添加 Composite 节点 | Selector / Sequence / SimpleParallel |
| 添加 Task 节点 | 按类名搜索并添加 BTTask 节点 |
| 添加 Decorator | 为节点添加装饰器 |
| 添加 Service | 为复合节点添加服务 |
| 创建 Blackboard | 新建黑板资产 |
| 添加 Blackboard Key | 支持 Bool/Int/Float/String/Vector 等 10 种类型 |

---

## 四、动画蓝图

| 功能 | 说明 |
|------|------|
| 添加动画状态 | 在 AnimGraph 状态机中添加状态节点 |
| 添加状态转换 | 在状态间创建转换连线 |
| 添加动画图节点 | 添加通用 AnimGraph 节点（按类名搜索） |

---

## 五、UMG UI 设计

| 功能 | 说明 |
|------|------|
| 创建 Widget 蓝图 | 新建 UI 蓝图资产 |
| 添加控件到画布 | 支持 Button/TextBlock/Image/CanvasPanel/HorizontalBox/VerticalBox/SizeBox/Border |
| 设置控件文本 | 设置 TextBlock/Button 的文字内容 |
| 设置字体样式 | 字号 + 颜色（RRGGBBAA 格式） |
| 设置控件位置 | Canvas 中的坐标和尺寸 |
| 设置可见性 | visible/collapsed/hidden/hit_test_invisible/self_hit_test_invisible |
| 查看控件树 | 列出 Widget 内所有控件及类型 |
| 添加到视口 | 编译并显示到编辑器视口 |

---

## 六、材质与特效

| 功能 | 说明 |
|------|------|
| 创建 PBR 材质 | 设置基色/粗糙度/金属度/高光/自发光/透明度（8 种材质类型） |
| 创建 Niagara 系统 | 新建 Niagara 特效系统资产 |
| 添加 Emitter | 从模板添加发射器到 Niagara 系统 |
| 设置 Niagara 参数 | 修改 Niagara 系统参数值 |

---

## 七、Sequencer 过场动画

| 功能 | 说明 |
|------|------|
| 创建 LevelSequence | 新建过场动画资产 |
| 添加 Actor 到 Sequencer | 将场景 Actor 加入 Sequencer 轨道 |
| 添加 TransformTrack | 为 Actor 绑定添加变换轨道 |
| 设置关键帧 | 设置指定时间点的位置关键帧 |

---

## 八、DataTable 数据表

| 功能 | 说明 |
|------|------|
| 创建 DataTable | 指定行结构体创建数据表 |
| 添加行数据 | JSON 格式写入一行数据 |
| 查询行数据 | 按行名读取数据为 JSON |

---

## 九、资产管理

| 功能 | 说明 |
|------|------|
| 复制资产 | 复制资产到目标路径 |
| 删除资产 | 删除指定路径的资产 |
| 列出资产 | 浏览 Content Browser 中的资产列表 |

---

## 十、物理与音频

| 功能 | 说明 |
|------|------|
| 启用/禁用物理 | 控制 Actor 的物理模拟 |
| 设置质量 | 修改 Actor 的 Mass 属性 |
| 施加力 | 对 Actor 施加方向力 |
| 播放/停止声音 | 控制 Actor 上的音频播放 |

---

## 十一、输入映射

| 功能 | 说明 |
|------|------|
| 添加 Action 映射 | 绑定按键到 Action（如 Jump → SpaceBar） |
| 添加 Axis 映射 | 绑定按键到 Axis 并设置缩放 |

---

## 十二、调试与辅助

| 功能 | 说明 |
|------|------|
| 截图 | 捕获编辑器视口画面（JPEG/PNG + Base64，可设分辨率） |
| Ping 连通检查 | 验证通信链路是否正常 |
| 反射扫描 | 扫描 UE5 反射系统生成 API 缓存（为后续自动发现新功能做准备） |

---

## 通信架构简图

```
AI (Claude Code)
  ↓ MCP over stdio
Python MCP Server（78 个工具）
  ↓ TCP :9876
Bridge 中继
  ↓ TCP :9876
UE5 编辑器插件
  → 执行实际操作
```

---

> **当前状态**：78 个 MCP 工具全部注册，核心测试通过率 97%（37/38），可投入实际使用。
