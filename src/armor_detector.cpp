#include "diesel_vision_frontend/armor_detector.hpp"

#include <array>
#include <cmath>
#include <opencv2/core/persistence.hpp>
#include <stdexcept>
#include <utility>

namespace diesel_vision_frontend {
namespace {

ArmorCorners to_armor_corners(const std::vector<cv::Point2f> &corners) {
  // 约定：0=左上、1=右上、2=右下、3=左下
  return ArmorCorners{corners[0], corners[1], corners[2], corners[3]};
}

std::array<cv::Point2f, 4>
to_corner_array(const std::vector<cv::Point2f> &corners) {
  return {corners[0], corners[1], corners[2], corners[3]};
}

bool is_finite(const cv::Vec3d &value) {
  return std::isfinite(value[0]) && std::isfinite(value[1]) &&
         std::isfinite(value[2]);
}

/// 装甲板法向 = 装甲板系 z 轴（PnP 的第三列）；朝外为正。
cv::Vec3d normal_from_rotation(const cv::Matx33d &rotation) {
  const cv::Vec3d normal{rotation(0, 2), rotation(1, 2), rotation(2, 2)};
  const double norm = std::sqrt(normal.dot(normal));
  if (!std::isfinite(norm) || norm <= 0.0) {
    return cv::Vec3d{0.0, 0.0, 0.0};
  }
  return normal * (1.0 / norm);
}

} // namespace

ArmorDetectorOptions
ArmorDetectorOptions::from_config(const DetectorConfig &config) {
  ArmorDetectorOptions options;
  options.pose_model = config.param("pose_model");
  options.pattern_model = config.param("pattern_model");
  if (options.pose_model.empty() || options.pattern_model.empty()) {
    throw std::invalid_argument(
        "detector 'armor' requires pose_model and pattern_model");
  }
  options.intra_op_threads = config.int_param("intra_op_threads", 0);
  options.pose_side = config.int_param("pose_side", 640);
  options.pattern_side = config.int_param("pattern_side", 640);
  const std::string calibration = config.param("calibration");
  if (!calibration.empty()) {
    // 复用 MVS 调试工具同一套标定文件约定：camera_matrix /
    // distortion_coefficients
    cv::FileStorage file(calibration, cv::FileStorage::READ);
    if (!file.isOpened()) {
      throw std::invalid_argument("detector 'armor' cannot open calibration: " +
                                  calibration);
    }
    cv::Mat camera_matrix;
    cv::Mat distortion;
    file["camera_matrix"] >> camera_matrix;
    file["distortion_coefficients"] >> distortion;
    if (camera_matrix.rows != 3 || camera_matrix.cols != 3 ||
        camera_matrix.type() != CV_64F || distortion.type() != CV_64F ||
        distortion.empty()) {
      throw std::invalid_argument("detector 'armor' invalid calibration: " +
                                  calibration);
    }
    options.camera_matrix = camera_matrix;
    options.distortion = distortion;
  }
  options.armor_size.width =
      static_cast<float>(config.double_param("armor_width_m", 0.135));
  options.armor_size.height =
      static_cast<float>(config.double_param("armor_height_m", 0.125));
  if (options.armor_size.width <= 0.0F || options.armor_size.height <= 0.0F ||
      options.pose_side <= 0 || options.pattern_side <= 0) {
    throw std::invalid_argument(
        "detector 'armor' sizes and model sides must be positive");
  }
  return options;
}

ArmorDetector::ArmorDetector(ArmorDetectorOptions options)
    : options_(std::move(options)),
      dual_(std::make_unique<DualDetector>(
          options_.pose_model, options_.pattern_model,
          options_.intra_op_threads, options_.pose_side,
          options_.pattern_side)) {
  if (options_.camera_matrix.has_value() != options_.distortion.has_value()) {
    throw std::invalid_argument(
        "detector 'armor' needs both camera_matrix and distortion, or neither");
  }
  if (options_.camera_matrix.has_value()) {
    solver_.emplace(options_.camera_matrix.value(), options_.distortion.value(),
                    options_.camera_to_gimbal);
  }
}

ArmorDetector::ArmorDetector(const DetectorConfig &config)
    : ArmorDetector(ArmorDetectorOptions::from_config(config)) {}

std::vector<Observation> ArmorDetector::detect(const cv::Mat &bgr_image,
                                               std::uint64_t stamp_ns) {
  last_frame_ = dual_->detect(bgr_image);
  std::vector<Observation> observations;
  observations.reserve(last_frame_.armors.size());
  for (const ArmorDetection &armor : last_frame_.armors) {
    if (armor.corners.size() != 4) {
      continue; // 四点不完整的候选不做位姿，也不进入逐帧契约
    }
    Observation observation;
    observation.target_class = TargetClass::Armor;
    observation.corners = to_corner_array(armor.corners);
    observation.confidence = armor.confidence;
    observation.stamp_ns = stamp_ns;
    observation.source =
        static_cast<std::uint8_t>(ObservationSource::OwnCamera);
    observation.armor_id = armor.id;
    if (solver_.has_value()) {
      const PnPResult pose =
          solver_->solve(to_armor_corners(armor.corners), options_.armor_size);
      observation.reprojection_error = pose.reprojection_error;
      if (pose.success && is_finite(pose.tvec_in_camera) &&
          pose.tvec_in_camera[2] > 0.0) {
        observation.xyz_camera = pose.tvec_in_camera;
        if (!pose.pnp_ambiguous) {
          // 平面靶标有镜像解时法向不可信，交给下游降级处理
          observation.normal_camera = normal_from_rotation(pose.R_armor2gimbal);
          observation.normal_valid = true;
        }
      }
    }
    observations.push_back(observation);
  }
  return observations;
}

namespace {
std::unique_ptr<Detector> make_armor_detector(const DetectorConfig &config) {
  return std::make_unique<ArmorDetector>(config);
}
} // namespace

DIESEL_REGISTER_DETECTOR("armor", Armor, make_armor_detector);

} // namespace diesel_vision_frontend
