# 关键工程决策日志

> 记录 fork patch、换版本、**破坏性接口变更**、架构取舍。
> 格式见 `foray_docs/repository_structure.md` §7.2。

---

## Decision

Date: 2026-11-08

Context: 本批 Pose 原始标注与训练标签均为 0=红、1=蓝，但训练 `data.yaml`、导出 ONNX 名称及部署代码沿用了旧的 0=蓝、1=红口径，导致显示/输出颜色与实物反向。

Decision: 当前 Pose 部署映射改为 0=红、1=蓝，并加入 0/1/非法类单元测试；本地当前训练集 `ready_no_pending7/data.yaml` 和 `best.pt`/`best.onnx` 类别元数据同步修正。原模型分别保留在同目录的 `best.pt.before_color_metadata_fix`、`best.onnx.before_color_metadata_fix`；未重训或修改数值权重。

Reason: 类别数字是训练与部署的真正契约，显示名称错误不能靠相机通道交换解决；本次只修复已核实的模型，不假设所有历史/后续权重都遵守同一映射。

---

## Decision

Date: 2026-11-07

Context: 需要用当前海康 MVS 相机观察实车靶子检测效果，但云台外参与时间对齐尚未验收，现场不允许发送云台控制。

Decision: 增加可选 MVS SDK 调试可执行程序，独立于生产视觉库。只采集 BGR 帧、运行现有双模型，并在显式提供同分辨率相机标定和实测外轮廓宽高时显示相机系 PnP；程序没有串口、控制帧或发射依赖。分辨率不符时禁用 PnP。

Reason: 先验证图像、角点与位姿链路，避免未标定云台系结果被误当作可下发目标；MVS SDK 也不应成为无相机 CI 的强制依赖。

---

## Decision

Date: 2026-11-06

Context: 正式图像入口已经只接双模型，但旧单模型 `Detector` 的实现、测试和公共检测类型仍混在一个头文件中。

Decision: 将 `ArmorDetection`、颜色和尺寸枚举移到独立头文件；移除旧 `[1,26,N]` 单模型前端源码及测试。Foray 后端与可选 SP 关联策略不变，完整 SP 跟踪器不在本次范围。

Reason: 保持双模型到 PnP 的接口稳定，同时避免旧推理路径被误用；后端对照可继续在现有基线上逐步提炼。

---

## Decision

Date: 2026-11-06

Context: 双模型已能推理，但正式 `process_armor_frame` 仍只接受旧单模型；真实靶车调试不应经过历史仿真灯条转换。

Decision: 正式图像入口改为 `DualDetector`，仅将唯一配对且四角可用的检测送入原有四角 PnP。旧 `Detector` 与灯条几何工具暂保留作离线兼容，不参与真实链路；不在本次接入相机采集或电机控制。

Reason: 让两种模型输出和 PnP 的数据契约实际贯通，同时把仿真适配与真实链路分开。

---

## Decision

Date: 2026-11-05

Context: 双模型输出装甲板外轮廓四角，而 SP 求解器使用灯条端点；模拟器适配器中的转换不能长期充当 Vision 正式接口。

Decision: 将平面单应转换放入 `foray_vision`，显式传入模型有效外轮廓宽高，灯条间距/长度默认采用 SP 小装甲板的 135/56 mm 约定。模拟器仅作为该接口的调用和测试端。

Reason: 明确四角点语义和物理尺寸，避免将仿真拟合出的 0.83/0.80 缩放值误当成赛季或实车标定。真实装甲板尺寸、标注外缘和相机标定仍须独立验证。

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

Context: 原 `diesel_vision_frontend`（检测 + 融合 + WorldSnapshot）与原 `diesel_auto_aim`（自瞄）职责重叠，
且 `diesel_auto_aim` 侧的 `detector.hpp` 只有空接口、本仓分支已实现装甲板双模型检测。

Decision: 本仓改名为 `diesel_vision_frontend`，定位为 **L3 检测前端**：相机取流 + 多任务检测器
（装甲板为首个实现，`TargetClass` 预留能量机关 / 前哨站 / 基地）+ PnP 几何，输出逐帧 `Observation`。
多源融合与 `WorldSnapshot` 生成上移到 L4 后端 `diesel_vision_frontend_backend`；契约消息放 `diesel_interfaces`。

Reason:
- 检测器需要 OpenCV / ONNXRuntime 重依赖，与硬实时跟踪分离（C4 依赖重量）
- 前端只产观测、后端产世界模型，消除两仓检测职责重叠
- 后端只链 Eigen，跟踪环 1–10 ms 不被模型推理抖动污染

Rejected:
- 保持原「感知 + 融合」定位（检测职责继续与 backend 重叠）
- 把跟踪也放前端（重依赖进实时环，破坏 C4）

Source: `foray_docs/repository_structure.md` §2、§6.6；`algorithm_structure.md` §1.4/§1.5；
迁移计划 P0–P6（dev_task `vision-front-backend-split`）。
## Decision

Date: 2026-10-17

Context: 装甲板检测原型位于 L4 `foray_auto_aim`，但仓库结构将目标感知归于 L3 `diesel_vision_frontend`。

Decision: 将 Detector 的头文件、实现与测试复制到 `diesel_vision_frontend`，仅调整命名空间与包含路径；在视觉包的 CMake 中链接 OpenCV、ONNX Runtime，并导出库与头文件。原仓未提交文件暂时保留，待调用方迁移后再决定清理。

Reason: 保持 L4 消费 L3 的单向依赖，同时不在迁移时改动未完成的检测算法。

Alternatives: 继续将检测实现留在 `foray_auto_aim`。

Rejected: L3 反向依赖 L4，不符合 `.foray-layer` 的依赖方向。

## Decision

Date: 2026-10-18

Context: 视觉调用层需要处理一帧中多块装甲板，但 Detector 构造依赖真实 ONNX 模型，不便用合成角点单独测试循环行为。

Decision: 保留 `process_armor_frame` 作为图像入口，将逐块 PnP 与结果汇总放在可传入检测列表的 `process_armor_solver`；新增空列表和无效目标夹在两块有效目标之间的测试。

Reason: 不加载模型也能验证多目标配对和局部失败后的继续处理；真实模型与标定数据仍需另行集成验证。

## Decision

Date: 2026-10-31

Context: 颜色/角点与图案分别由两个模型输出，候选数量和顺序可能不同，且同一区域可能有重复候选。

Decision: 新增纯函数 `match_pattern_centers`：同一帧、原图像素坐标下，以图案框中心是否落在装甲板框内配对；漏检、坏框、重叠装甲板及同板多个图案分别保留未配对或歧义状态，不按下标硬配，也不强行选最高置信度。双模型解码和推理调用暂未接入。

Reason: 数量与顺序检查只能报警，空间关系可处理漏检后的局部配对；歧义显式暴露，避免误分类直接流入 PnP/跟踪。

## Decision

Date: 2026-10-31

Context: 新训练的颜色/四角点与图案模型输出分别为 `[1,18,N]`、`[1,6,N]`，旧 `Detector` 固定接受 `[1,26,N]`。

Decision: 保留旧 `Detector`，新增独立 `DualDetector`。两个模型共享预处理约定但分别解码；按原图框中心配对，只把唯一配对且四角点可见的目标交给 PnP。模拟器可选双模型入口；HSV 阈值和角点质量仍需按新场景验证。

Reason: 不让新模型契约破坏旧模型测试和调用；不把“步兵”误称为具体机器人编号。

## Decision

Date: 2026-11-03

Context: 模型类别曾被当成机器人号码，导致步兵、哨兵 ID 在 Vision 与 Tracker 间不一致。

Decision: `ArmorDetection.id` 统一为有符号类别：`-1` 无有效目标、`0` 哨兵、`1` 步兵、`2` 重装（暂无模型输出）。双模型图案索引同为 `0=哨兵、1=步兵`；旧模型的 1–5 号步兵都转成类别 1，英雄等未支持类别转为 -1 并不进入 PnP。颜色仍由独立字段表示。2026-11-04 根据最新约定将哨兵/步兵的 0/1 顺序对调，覆盖前一天的口径。

Reason: 类别不能代表具体哪辆车；两辆步兵的身份须由后续时序关联决定，不能用同一个类别值冒充唯一轨迹 ID。

## Decision

Date: 2026-11-04

Context: 双模型对同一帧图像重复做预处理，然后串行执行两个 ONNX Session；模拟器双模型采样间隔明显高于理想输入。

Decision: `DualDetector::detect` 只生成一份输入张量，由两个独立 Session 并行推理，等待两份结果后才做同帧配对。测试增加同图重复检测一致性与可选固定图耗时打印；不改模型输出、匹配与 PnP 契约。

Reason: 减少重复预处理和串行等待。当前 CPU 环境固定图 20 次均值约 124 ms 降到约 94 ms；这一数据不代表实机帧率，也不保证不同 ONNX Runtime 后端有同样收益。

## Decision

Date: 2026-11-04

Context: 对同一固定图进一步分段计时发现，CPU ORT 推理占大头；配对与 PnP 远小于推理。默认 Session 线程配置的耗时在不同轮次波动明显。

Decision: `DualDetector` 增加可选 `intra_op_threads`（默认 0 保留 ORT 默认配置），由部署调用方按机器实测选择；增加单帧阶段耗时记录，不在库内写死本机最优线程数。固定图本机测试 8 线程约 39–43 ms，4 线程约 44–45 ms，12 线程约 73 ms；PnP 约 0.017 ms/块。待模拟器连续出帧后再核对 Tracker 和端到端延迟。

Reason: 优化应先作用于实际瓶颈，且线程数具有硬件/后端依赖；离线吞吐不是控制闭环延迟或实机 FPS。

Implementation note: 将 RGB→CHW 写入改为按行读取，保持数值映射不变；同一固定图、8 线程复测预处理约 5.17 ms、双模型总耗时约 35.75 ms。不同轮次系统负载未完全控制，需用多图复核。

---

## Decision

Date: 2026-11-14

Context: `feat/sp-dual-detector-vision` 基于旧 `foray_vision` 命名开发，`dev` 已完成 diesel 改名，
两者分叉（分支领先 7、落后 2）；分支同时存在旧单模型 `detector.hpp` 与双模型 `armor_detection.hpp` 两套
检测结果类型，`detector.hpp` 已无任何实现文件与测试。

Decision: 分支 rebase 到 `dev`，命名统一为 `diesel_vision_frontend`（namespace、include 路径、include guard、
`DIESEL_TEST_*` 环境变量）；删除 `detector.hpp` / `detector.cpp` / `test_detector.cpp`，
`ArmorType` / `ArmorColor` / `ArmorDetection` 只由 `armor_detection.hpp` 提供；rebase 前状态留在
`backup/sp-dual-pre-rebase-1114`。

Reason:
- 保留两套同名类型会让 `DualDetector` 与 PnP 的 `ArmorDetection` 产生 ODR 风险，且 `detector.hpp` 无人实现
- 先收敛到 `dev` 基线，后续 I4 改仓名只需一次机械改名，不再叠加 rebase 冲突

Rejected:
- 直接把分支合并进 `dev`（绕过分叉，历史里留下 foray 命名）
- 保留 `detector.hpp` 作兼容别名（仓库尚未对外发布，无兼容负担）

Source: `foray_docs/repository_structure.md` §2、§6.6；dev_task `vision-front-backend-split` I3。

---

## Decision

Date: 2026-11-14

Context: I4 —— 仓名与包名从 `diesel_vision` 改为 `diesel_vision_frontend`，与「前端检测 / 后端融合跟踪」的分家边界对齐。

Decision:
- GitHub 仓库原地改名 `diesel_vision` → `diesel_vision_frontend`（平台自动重定向旧 URL），本地目录与 `origin` 同步改名。
- ROS 包名、CMake `project()`、`include/` 目录与 `#include` 路径、`namespace`、头文件 guard（`DIESEL_VISION_FRONTEND_*`）、调试可执行文件（`diesel_vision_frontend_camera_debug`）一并改为新名。
- `README.md` 定位改为「视觉前端：装甲板检测 + PnP 几何」，对外输出逐帧 `Observation`；多源融合、跟踪与 `WorldSnapshot` 生成明确归 `diesel_vision_backend`。

Reason: 前端只做检测与几何，仓名必须能表达这一点；后端改名 `diesel_vision_backend` 后，若本仓仍叫 `diesel_vision`，上下游文档描述的拓扑就自相矛盾。原地改名保留提交历史、PR 与权限。

Source: 迁移计划 I4（dev_task `vision-front-backend-split`）；PR #2（merge commit `55ae500`）。

Rejected: 新建 `diesel_vision_frontend` 仓并搬代码（丢历史与 PR）；保留 `diesel_vision` 仓名只改包名（仓名与职责仍然错位）。
