#ifndef DIESEL_VISION_FRONTEND_ARMOR_DETECTOR_HPP
#define DIESEL_VISION_FRONTEND_ARMOR_DETECTOR_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "diesel_vision_frontend/detector.hpp"
#include "diesel_vision_frontend/detector_registry.hpp"
#include "diesel_vision_frontend/dual_detector.hpp"
#include "diesel_vision_frontend/pnp_solver.hpp"

namespace diesel_vision_frontend {

/// 装甲板检测器的装配参数。
///
/// 标定可选：不提供 `camera_matrix`/`distortion` 时只输出像素四角与置信度，
/// `xyz_camera`/`normal_camera` 保持 0 且 `normal_valid=false`。
struct ArmorDetectorOptions {
  std::string pose_model;
  std::string pattern_model;
  int intra_op_threads = 0; ///< 0 = 沿用 ONNX Runtime 默认
  int pose_side = 640;
  int pattern_side = 640;

  std::optional<cv::Mat> camera_matrix;
  std::optional<cv::Mat> distortion;
  /// 相机系到云台系；默认单位阵表示「相机系即云台系」，上机前必须标定。
  Camera2gimbal camera_to_gimbal{};
  /// 装甲板真实尺寸（米）；小装甲板默认 135×125 mm（2026 赛季规则）。
  ArmorSize armor_size{0.135F, 0.125F};

  /// 从注册表配置解析；缺失 pose_model/pattern_model 或标定不成对时抛
  /// std::invalid_argument。
  static ArmorDetectorOptions from_config(const DetectorConfig &config);
};

/// 装甲板检测器：双模型检测 + PnP，首个 TargetClass::Armor 实现。
class ArmorDetector final : public Detector {
public:
  explicit ArmorDetector(ArmorDetectorOptions options);
  explicit ArmorDetector(const DetectorConfig &config);

  TargetClass target_class() const override { return TargetClass::Armor; }
  std::vector<Observation> detect(const cv::Mat &bgr_image,
                                  std::uint64_t stamp_ns = 0) override;

  /// 最近一帧的原始检测细节，仅供调试与计时；不进入 Observation 契约。
  const DualDetector::Frame &last_frame() const { return last_frame_; }

private:
  ArmorDetectorOptions options_;
  std::unique_ptr<DualDetector> dual_;
  std::optional<Solver> solver_;
  DualDetector::Frame last_frame_;
};

} // namespace diesel_vision_frontend

#endif // DIESEL_VISION_FRONTEND_ARMOR_DETECTOR_HPP
