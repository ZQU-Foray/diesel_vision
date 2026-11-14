#ifndef DIESEL_VISION_FRONTEND_DETECTOR_REGISTRY_HPP
#define DIESEL_VISION_FRONTEND_DETECTOR_REGISTRY_HPP

#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "diesel_vision_frontend/detector.hpp"
#include "diesel_vision_frontend/target_class.hpp"

namespace diesel_vision_frontend {

/// 单个检测器的装配参数：`name` 是注册名，其余键值由该检测器自己解释。
struct DetectorConfig {
  std::string name;
  TargetClass target_class = TargetClass::Unknown;
  std::map<std::string, std::string> params;

  std::string param(std::string_view key,
                    std::string fallback = std::string()) const;
  /// 缺省或非法数字时抛 std::invalid_argument（配置错误不能静默降级）。
  int int_param(std::string_view key, int fallback) const;
  double double_param(std::string_view key, double fallback) const;
  bool has_param(std::string_view key) const;
};

using DetectorFactory =
    std::function<std::unique_ptr<Detector>(const DetectorConfig &)>;

/// 编译期注册表：`add()` 在静态初始化阶段完成，`create()` 按配置装配。
class DetectorRegistry {
public:
  static DetectorRegistry &instance();

  /// 注册名重复直接抛 std::invalid_argument（同名覆盖会让装配结果不确定）。
  void add(std::string name, TargetClass target_class, DetectorFactory factory);
  bool contains(std::string_view name) const;
  std::vector<std::string> names() const;

  /// 未知注册名或工厂返回空指针时抛 std::invalid_argument。
  std::unique_ptr<Detector> create(const DetectorConfig &config) const;

private:
  struct Entry {
    TargetClass target_class = TargetClass::Unknown;
    DetectorFactory factory;
  };
  std::map<std::string, Entry, std::less<>> entries_;
};

/// 最小配置格式（不引入 YAML 依赖）：
///
/// ```
/// # 注释
/// enabled = armor, energy
/// armor.pose_model = /path/pose.onnx
/// armor.pattern_model = /path/pattern.onnx
/// ```
///
/// `enabled` 为逗号分隔的注册名列表；`<name>.<key>` 归该检测器的参数。
/// 出现未注册名、重复键、空 enabled 或格式错误时抛 std::invalid_argument。
std::vector<std::unique_ptr<Detector>>
create_enabled_detectors(std::istream &config_text);

std::vector<std::unique_ptr<Detector>>
create_enabled_detectors_from_file(const std::string &path);

/// 注册一个检测器实现。用法：在实现文件的命名空间作用域写一行
/// `DIESEL_REGISTER_DETECTOR("armor", Armor, make_armor_detector);`
#define DIESEL_DETAIL_CAT_INNER(a, b) a##b
#define DIESEL_DETAIL_CAT(a, b) DIESEL_DETAIL_CAT_INNER(a, b)
#define DIESEL_REGISTER_DETECTOR(NAME, CLASS, FACTORY)                         \
  namespace {                                                                  \
  const bool DIESEL_DETAIL_CAT(kDetectorRegistered_, __COUNTER__) = [] {       \
    ::diesel_vision_frontend::DetectorRegistry::instance().add(                \
        NAME, ::diesel_vision_frontend::TargetClass::CLASS, FACTORY);          \
    return true;                                                               \
  }();                                                                         \
  }

} // namespace diesel_vision_frontend

#endif // DIESEL_VISION_FRONTEND_DETECTOR_REGISTRY_HPP
