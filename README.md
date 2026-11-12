# diesel_vision

> **L3** · owner 角色：视觉

目标感知 + 态势融合 + WorldSnapshot 生成

对外输出**唯一**的 `WorldSnapshot`，是决策层的共同输入。

---

## 快速开始

> 本仓处于**初始化状态**，尚无源码。以下流程随 P0/P1 落地逐步可用。

```bash
# 1. 拉取（仓齐备后改为从元仓 diesel_ws 一键拉取）
git clone git@github.com:ZQU-Foray/diesel_vision.git
cd diesel_vision

# 2. 依赖安装
rosdep install -y -r --from-paths . --ignore-src --rosdistro humble

# 3. 构建
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release

# 4. 运行
# （待装配根就绪后补充）
```

## 依赖

| 类型 | 依赖 |
|---|---|
| **自研仓** | X2、L1、L2、L3 |
| **第三方** | OpenCV · TensorRT · ROS 2 Humble |

## 接口

| 产出 | 内容 |
|---|---|
| `WorldSnapshot` | 区域级抽象状态（**非**原始传感器数据）· 纯函数 · 定长 |
| 目标感知 | 装甲板检测/跟踪结果 |
| 融合态势 | 多源去重 / 坐标统一 / 时间对齐 / 置信度仲裁 |

`WorldSnapshot` 字段分区（详见 `diesel_interfaces`）：A `self` · B `allies[2]` · C `enemies[3]` ·
D `game` · E `context` · F `temporal`。

## `WorldSnapshot` 四条设计原则（违反任何一条都会破坏下游）



1. **定长**——不用变长数组（RL 需固定输入）；缺失用 `valid` 标志

2. **区域离散化优先**——用 `region_id` 代替原始坐标

   > 这是**关键耦合**：区域化状态 → 轻量对局仿真成为可能 → 红队 RL 可行

3. **带 `age` 与 `confidence`**——让决策器知道信息**多旧**、**多可信**

4. **纯函数、零 ROS 依赖**——可单元测试、可被仿真与实车共用



## 纪律



- `WorldSnapshot` 的 schema **必须与仿真输出完全一致**，否则离线闭环断裂

- 本仓是态势的**唯一权威来源**；其他仓不得自行拼凑世界状态

- 自瞄的目标感知细节在 `diesel_auto_aim`，本仓只提供**装甲板级**的原始感知

## 上下游

| 方向 | 对象 |
|---|---|
| **上游**（本仓依赖谁） | `diesel_interfaces` · `diesel_platform` · `diesel_localization` |
| **下游**（谁依赖本仓） | `diesel_auto_aim` · `diesel_decision` · `diesel_robots` · `diesel_ws` |

## 参考

- [算法结构](https://github.com/ZQU-Foray/foray_docs/blob/main/algorithm_structure.md)
- [仓库结构](https://github.com/ZQU-Foray/foray_docs/blob/main/repository_structure.md)
- [组织贡献指南](https://github.com/ZQU-Foray/.github/blob/main/CONTRIBUTING.md)
