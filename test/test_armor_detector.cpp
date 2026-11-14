#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>

#include <cstdlib>
#include <string>

#include "diesel_vision_frontend/armor_detector.hpp"

namespace diesel_vision_frontend {
namespace {

ArmorDetectorOptions fixture_options() {
  ArmorDetectorOptions options;
  const char *pose = std::getenv("DIESEL_TEST_POSE_ONNX");
  const char *pattern = std::getenv("DIESEL_TEST_PATTERN_ONNX");
  options.pose_model = pose ? pose : "";
  options.pattern_model = pattern ? pattern : "";
  const char *threads_text = std::getenv("DIESEL_TEST_INTRA_THREADS");
  options.intra_op_threads = threads_text ? std::stoi(threads_text) : 0;
  return options;
}

} // namespace

TEST(ArmorDetectorTest, RejectsHalfCalibration) {
  ArmorDetectorOptions options = fixture_options();
  if (options.pose_model.empty() || options.pattern_model.empty()) {
    GTEST_SKIP() << "Set DIESEL_TEST_POSE_ONNX and DIESEL_TEST_PATTERN_ONNX";
  }
  options.camera_matrix = (cv::Mat_<double>(3, 3) << 800.0, 0.0, 640.0, 0.0,
                           800.0, 512.0, 0.0, 0.0, 1.0);
  // 只有内参没有畸变 → 构造期就要报错，不能等到第一帧
  EXPECT_THROW(ArmorDetector{options}, std::invalid_argument);
}

TEST(ArmorDetectorTest,
     ProducesObservationsFromRealModelsWhenFixtureIsProvided) {
  ArmorDetectorOptions options = fixture_options();
  const char *image = std::getenv("DIESEL_TEST_IMAGE");
  if (options.pose_model.empty() || options.pattern_model.empty() || !image) {
    GTEST_SKIP() << "Set model and image fixture paths";
  }
  const cv::Mat bgr = cv::imread(image);
  ASSERT_FALSE(bgr.empty());

  ArmorDetector detector(options);
  EXPECT_EQ(detector.target_class(), TargetClass::Armor);

  const auto observations = detector.detect(bgr, 123456U);
  ASSERT_FALSE(observations.empty());
  EXPECT_EQ(detector.last_frame().armors.size(), observations.size());
  for (const Observation &observation : observations) {
    EXPECT_EQ(observation.target_class, TargetClass::Armor);
    EXPECT_EQ(observation.stamp_ns, 123456U);
    EXPECT_EQ(observation.source,
              static_cast<std::uint8_t>(ObservationSource::OwnCamera));
    EXPECT_GE(observation.confidence, 0.0);
    EXPECT_LE(observation.confidence, 1.0);
    // 无标定时只保证像素四角，位置/法向必须显式标记无效
    EXPECT_FALSE(observation.normal_valid);
    EXPECT_DOUBLE_EQ(observation.xyz_camera[2], 0.0);
  }
}

TEST(ArmorDetectorTest, ConfigWithoutModelsFailsAtAssembly) {
  DetectorConfig config;
  config.name = "armor";
  EXPECT_THROW(ArmorDetectorOptions::from_config(config),
               std::invalid_argument);
}

} // namespace diesel_vision_frontend
