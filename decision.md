# 关键工程决策日志

> 记录 fork patch、换版本、**破坏性接口变更**、架构取舍。
> 格式见 `foray_docs/repository_structure.md` §7.2。

---

## Decision

Date: 2026-02-19

Context: 本仓从 0 初始化。需要确定它在整体架构中的位置与依赖边界。

Decision: 本仓定位为 **L3** 层，owner 角色为「视觉」，
依赖层级 X2、L1、L2、L3。

Reason:
- 切分判据 C1–C6 见 `foray_docs/repository_structure.md` §1
- 粒度结论：**一个仓 = 一个团队角色 + 一个变更率档位**
- 对外输出**唯一**的 `WorldSnapshot`，是决策层的共同输入

Alternatives:
- 并入相邻仓（减少仓库数量，但违反团队边界或变更率差异）

Rejected:
- 按 ROS 2 package 粒度切仓 —— 会导致版本矩阵地狱
- 按分层机械切仓 —— 会切断团队边界（Conway）

---

## Decision

Date: 2026-11-14

Context: 原 `diesel_vision`（检测 + 融合 + WorldSnapshot）与原 `diesel_auto_aim`（自瞄）职责重叠，
且 `diesel_auto_aim` 侧的 `detector.hpp` 只有空接口、本仓分支已实现装甲板双模型检测。

Decision: 本仓改名为 `diesel_vision_frontend`，定位为 **L3 检测前端**：相机取流 + 多任务检测器
（装甲板为首个实现，`TargetClass` 预留能量机关 / 前哨站 / 基地）+ PnP 几何，输出逐帧 `Observation`。
多源融合与 `WorldSnapshot` 生成上移到 L4 后端 `diesel_vision_backend`；契约消息放 `diesel_interfaces`。

Reason:
- 检测器需要 OpenCV / ONNXRuntime 重依赖，与硬实时跟踪分离（C4 依赖重量）
- 前端只产观测、后端产世界模型，消除两仓检测职责重叠
- 后端只链 Eigen，跟踪环 1–10 ms 不被模型推理抖动污染

Rejected:
- 保持原「感知 + 融合」定位（检测职责继续与 backend 重叠）
- 把跟踪也放前端（重依赖进实时环，破坏 C4）

Source: `foray_docs/repository_structure.md` §2、§6.6；`algorithm_structure.md` §1.4/§1.5；
迁移计划 P0–P6（dev_task `vision-front-backend-split`）。
