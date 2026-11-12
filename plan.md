# 开发计划

## 任务列表

- [ ] 接装甲板检测/跟踪，产出带置信度的目标
- [ ] 实现多源态势融合（去重 / 坐标统一 / 时间对齐 / 置信度仲裁）
- [ ] 生成 `WorldSnapshot`（对齐 `diesel_interfaces` 的冻结 schema）
- [ ] 保证 `WorldSnapshot` 生成是纯函数、零 ROS 依赖

## 当前 Agent State

PLAN_READY

## Before Snapshot

commit hash:   （首次提交）
branch:        main
modified files: （首次初始化）
risk level:    L1

## 模糊点与待确认项

- 本仓接口尚未冻结，任务清单为**方向性**的，落地顺序以 `foray_docs/algorithm_structure.md` §9 演进阶段为准
- 依赖的自研仓尚未建齐，跨仓任务需等对方 `README.md` 明确接口后再启动

## Vector Backend Status

Backend: Markdown
Status:  ready
Environment: 仓库内 Markdown 文档（无外部向量后端）
Index: 本仓 `README.md` / `tree.md` / `decision.md`
Initialization: 2026-02-19
Commit: （首次提交）

## Acceptance Criteria

- [ ] 本仓能独立 `colcon build`（无代码时跳过）
- [ ] CI 绿灯
- [ ] 每个新增模块带独立可执行测试
