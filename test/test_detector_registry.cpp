#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "diesel_vision_frontend/detector_registry.hpp"

// 这个文件本身就是验收证据：新增一类目标 = 一个实现 + 一行注册 + 一段 config。
namespace diesel_vision_frontend {
namespace {

// 一行实现：假目标检测器，只为证明 registry 契约。
class FakeDetector final : public Detector {
public:
  explicit FakeDetector(double confidence) : confidence_(confidence) {}

  TargetClass target_class() const override {
    return TargetClass::EnergyMechanism;
  }

  std::vector<Observation> detect(const cv::Mat &bgr_image,
                                  std::uint64_t stamp_ns) override {
    (void)bgr_image;
    Observation observation;
    observation.target_class = TargetClass::EnergyMechanism;
    observation.confidence = confidence_;
    observation.stamp_ns = stamp_ns;
    observation.xyz_camera = cv::Vec3d{1.0, 2.0, 3.0};
    return {observation};
  }

private:
  double confidence_;
};

std::unique_ptr<Detector> make_fake_detector(const DetectorConfig &config) {
  return std::make_unique<FakeDetector>(config.double_param("confidence", 0.5));
}

} // namespace

// 一行注册：
DIESEL_REGISTER_DETECTOR("fake", EnergyMechanism, make_fake_detector);

TEST(DetectorRegistryTest, ArmorIsRegisteredByTheLibrary) {
  EXPECT_TRUE(DetectorRegistry::instance().contains("armor"));
}

TEST(DetectorRegistryTest,
     NewTargetNeedsOnlyImplementationRegistrationAndConfig) {
  std::istringstream config_text{"enabled = fake\nfake.confidence = 0.75\n"};
  auto detectors = create_enabled_detectors(config_text);
  ASSERT_EQ(detectors.size(), 1U);
  EXPECT_EQ(detectors.front()->target_class(), TargetClass::EnergyMechanism);

  const cv::Mat empty_image;
  const auto observations = detectors.front()->detect(empty_image, 42U);
  ASSERT_EQ(observations.size(), 1U);
  EXPECT_EQ(observations.front().target_class, TargetClass::EnergyMechanism);
  EXPECT_DOUBLE_EQ(observations.front().confidence, 0.75);
  EXPECT_EQ(observations.front().stamp_ns, 42U);
}

TEST(DetectorRegistryTest, ConfigCanEnableSeveralDetectorsInOrder) {
  std::istringstream config_text{"enabled = fake,fake\n"};
  // 同一个名字出现两次是合法的（两路相机），各自独立实例化。
  const auto detectors = create_enabled_detectors(config_text);
  EXPECT_EQ(detectors.size(), 2U);
}

TEST(DetectorRegistryTest, UnknownNameIsRejectedInsteadOfSilentlySkipped) {
  std::istringstream config_text{"enabled = armor,laser\n"};
  EXPECT_THROW(create_enabled_detectors(config_text), std::invalid_argument);
}

TEST(DetectorRegistryTest, EmptyEnabledListIsRejected) {
  std::istringstream config_text{"fake.confidence = 0.3\n"};
  EXPECT_THROW(create_enabled_detectors(config_text), std::invalid_argument);
}

TEST(DetectorRegistryTest, MalformedLineIsRejected) {
  std::istringstream config_text{"enabled = fake\ngarbage\n"};
  EXPECT_THROW(create_enabled_detectors(config_text), std::invalid_argument);
}

TEST(DetectorRegistryTest, DuplicateParamIsRejected) {
  std::istringstream config_text{
      "enabled = fake\nfake.confidence = 0.3\nfake.confidence = 0.4\n"};
  EXPECT_THROW(create_enabled_detectors(config_text), std::invalid_argument);
}

TEST(DetectorRegistryTest, BadNumberIsRejected) {
  std::istringstream config_text{"enabled = fake\nfake.confidence = high\n"};
  EXPECT_THROW(create_enabled_detectors(config_text), std::invalid_argument);
}

TEST(DetectorRegistryTest, DuplicateRegistrationIsRejected) {
  auto &registry = DetectorRegistry::instance();
  EXPECT_THROW(
      registry.add("fake", TargetClass::EnergyMechanism, make_fake_detector),
      std::invalid_argument);
}

TEST(DetectorRegistryTest, ClassMismatchBetweenConfigAndRegistryIsRejected) {
  DetectorConfig config;
  config.name = "fake";
  config.target_class = TargetClass::Armor;
  EXPECT_THROW(DetectorRegistry::instance().create(config),
               std::invalid_argument);
}

TEST(DetectorRegistryTest, MissingModelPathsFailAtAssemblyNotAtFirstFrame) {
  DetectorConfig config;
  config.name = "armor";
  EXPECT_THROW(DetectorRegistry::instance().create(config),
               std::invalid_argument);
}

} // namespace diesel_vision_frontend
