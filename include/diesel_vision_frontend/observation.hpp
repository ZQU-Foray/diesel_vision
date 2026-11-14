#ifndef DIESEL_VISION_FRONTEND_OBSERVATION_HPP
#define DIESEL_VISION_FRONTEND_OBSERVATION_HPP

#include <array>
#include <cstdint>

#include <opencv2/core.hpp>

#include "diesel_vision_frontend/target_class.hpp"

namespace diesel_vision_frontend {

/// 前端输出的逐帧观测，是前端与后端之间唯一的逐帧契约。
///
/// 四角顺序与 `ArmorDetection` 一致：左上、右上、右下、左下。
/// 未做 PnP（无标定或该目标不做位姿）时 `xyz_camera`/`normal_camera` 全 0、
/// `normal_valid=false`、`reprojection_error=0`；调用方必须看 `normal_valid`
/// 而不是看数值。
struct Observation {
  TargetClass target_class = TargetClass::Unknown;
  std::array<cv::Point2f, 4> corners{}; ///< 像素坐标，左上/右上/右下/左下
  cv::Vec3d xyz_camera{0.0, 0.0, 0.0}; ///< 目标中心，米
  cv::Vec3d normal_camera{0.0, 0.0, 0.0}; ///< 目标法向（单位向量），无效时全 0
  bool normal_valid = false;
  double yaw = 0.0; ///< 目标朝向（rad，相机系）；不做朝向估计时为 0
  double confidence = 0.0;
  std::uint64_t stamp_ns = 0; ///< 该帧采样时刻；0 表示调用方尚未打时间戳
  std::uint8_t source = 0; ///< 0=自身相机；1=队友共享（预留）
  int armor_id = -1;       ///< 装甲板语义编号；非装甲板目标为 -1
  double reprojection_error = 0.0; ///< 像素；无 PnP 时为 0
};

/// Observation 的来源取值，避免各处写魔法数字。
enum class ObservationSource : std::uint8_t {
  OwnCamera = 0,
  AllyShared = 1,
};

} // namespace diesel_vision_frontend

#endif // DIESEL_VISION_FRONTEND_OBSERVATION_HPP
