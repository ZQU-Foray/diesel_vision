#ifndef DIESEL_VISION_FRONTEND_TARGET_CLASS_HPP
#define DIESEL_VISION_FRONTEND_TARGET_CLASS_HPP

#include <cstddef>
#include <optional>
#include <string_view>

namespace diesel_vision_frontend {

/// 前端可识别的目标类别。新增类别只需扩这个枚举 + 注册一个 detector。
enum class TargetClass {
  Armor,
  EnergyMechanism,
  Outpost,
  Base,
  Unknown,
};

/// 枚举项个数（含 Unknown），用于数组下标与配置校验。
constexpr std::size_t kTargetClassCount = 5;

/// 稳定字符串名，用于配置文件与日志；不要依赖枚举数值。
std::string_view to_string(TargetClass target_class);

/// 字符串转枚举；未知字符串返回
/// std::nullopt（配置写错时调用方应报错而不是静默降级）。
std::optional<TargetClass> target_class_from_string(std::string_view name);

} // namespace diesel_vision_frontend

#endif // DIESEL_VISION_FRONTEND_TARGET_CLASS_HPP
