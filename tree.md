# 目录结构说明

## 目录结构

```
.
├── README.md                本仓定位、接口、上下游
├── AGENTS.md                AI 协作规范
├── plan.md                  开发计划与进度
├── decision.md              关键工程决策日志
├── tree.md                  本文件
├── CODEOWNERS               Review 自动分派
├── .diesel-layer             所属层与依赖边界（CI 校验）
├── .gitignore
├── CMakeLists.txt          PnP、双模型与调用链的构建、安装及单元测试
├── package.xml             ROS 2 包声明
├── include/diesel_vision_frontend/  检测结果类型、双模型、PnP、图案框配对与调用链接口
├── src/                   PnP、双模型、调用链及图案框配对实现
├── tools/camera_debug.cpp  可选海康 MVS 相机只读调试窗口（不发送控制）
├── test/                  对应的离线单元测试
└── .github/
    └── workflows/
        └── ci.yml           复制自组织 CI 模板
```

> 当前正式图像入口连接双模型与四角 PnP；旧单模型 `detector.hpp/.cpp` 与 `test_detector.cpp` 已于 2026-11-14 移除，检测结果类型统一由 `armor_detection.hpp` 提供。双模型已完成真实 ONNX 离线推理，并共享预处理、并行执行两个独立 Session。HSV 仲裁、相机标定与实机验证仍待完成。

> `sp_lightbar_geometry` 是历史仿真适配工具；正式相机链路直接用物理外轮廓四角做 PnP，不转换为灯条端点。

> 双模型接口附带可选 CPU ORT 线程配置和单帧阶段计时；`test/test_dual_detector.cpp` 可用本地 ONNX/图片夹具做离线分段基准，`test/test_pnp.cpp` 可选重复解算计时。

> `DualDetector` 使用当前 Pose 标签契约 0=红、1=蓝；`test/test_dual_detector.cpp` 同时覆盖数字到颜色的映射。当前本地 ONNX 的名称元数据已修正，但换权重必须重新核对其类别表。

## 记录约束

MUST NOT 记录以下内容：

- `build/`、`install/`、`log/`、`.git/` 等依赖与产物目录
- 临时文件、缓存文件、日志文件
