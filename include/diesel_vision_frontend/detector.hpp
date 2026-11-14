#ifndef DIESEL_VISION_FRONTEND_DETECTOR_HPP
#define DIESEL_VISION_FRONTEND_DETECTOR_HPP

#include <cstdint>
#include <vector>

#include <opencv2/core.hpp>

#include "diesel_vision_frontend/observation.hpp"
#include "diesel_vision_frontend/target_class.hpp"

namespace diesel_vision_frontend {

/// 所有目标检测器的统一接口。
///
/// 约束：
/// - `detect()`
/// 必须自包含：不读全局状态、不打开设备、不写文件，便于单测与回放。
/// - 输出顺序即该检测器内部的置信度排序，调用方不应假设跨检测器有序。
/// - 计算必须在调用线程内完成；`DualDetector` 的 ONNX Session 已内部并行。
class Detector {
public:
  virtual ~Detector() = default;

  /// 该检测器负责的类别；registry 用它做配置校验与下游分流。
  virtual TargetClass target_class() const = 0;

  /// 处理一帧 BGR 图像。
  /// @param stamp_ns 该帧采样时刻（纳秒）；0 表示调用方未提供，观测里的
  /// stamp_ns 原样写回。
  virtual std::vector<Observation> detect(const cv::Mat &bgr_image,
                                          std::uint64_t stamp_ns = 0) = 0;
};

} // namespace diesel_vision_frontend

#endif // DIESEL_VISION_FRONTEND_DETECTOR_HPP
