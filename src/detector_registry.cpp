#include "diesel_vision_frontend/detector_registry.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace diesel_vision_frontend {
namespace {

std::string_view trim(std::string_view text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return text;
}

std::vector<std::string> split(std::string_view text, char delimiter) {
  std::vector<std::string> parts;
  std::size_t begin = 0;
  while (true) {
    const std::size_t end = text.find(delimiter, begin);
    const std::string_view part = trim(
        text.substr(begin, end == std::string_view::npos ? end : end - begin));
    if (!part.empty()) {
      parts.emplace_back(part);
    }
    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }
  return parts;
}

} // namespace

std::string_view to_string(TargetClass target_class) {
  switch (target_class) {
  case TargetClass::Armor:
    return "armor";
  case TargetClass::EnergyMechanism:
    return "energy_mechanism";
  case TargetClass::Outpost:
    return "outpost";
  case TargetClass::Base:
    return "base";
  case TargetClass::Unknown:
    return "unknown";
  }
  return "unknown";
}

std::optional<TargetClass> target_class_from_string(std::string_view name) {
  for (std::size_t index = 0; index < kTargetClassCount; ++index) {
    const auto candidate = static_cast<TargetClass>(index);
    if (to_string(candidate) == name) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::string DetectorConfig::param(std::string_view key,
                                  std::string fallback) const {
  const auto it = params.find(std::string(key));
  return it == params.end() ? std::move(fallback) : it->second;
}

bool DetectorConfig::has_param(std::string_view key) const {
  return params.find(std::string(key)) != params.end();
}

int DetectorConfig::int_param(std::string_view key, int fallback) const {
  const auto it = params.find(std::string(key));
  if (it == params.end()) {
    return fallback;
  }
  std::size_t consumed = 0;
  int value = 0;
  try {
    value = std::stoi(it->second, &consumed);
  } catch (const std::exception &) {
    throw std::invalid_argument("detector '" + name + "' param '" +
                                std::string(key) +
                                "' is not an integer: " + it->second);
  }
  if (consumed != it->second.size()) {
    throw std::invalid_argument("detector '" + name + "' param '" +
                                std::string(key) +
                                "' is not an integer: " + it->second);
  }
  return value;
}

double DetectorConfig::double_param(std::string_view key,
                                    double fallback) const {
  const auto it = params.find(std::string(key));
  if (it == params.end()) {
    return fallback;
  }
  std::size_t consumed = 0;
  double value = 0.0;
  try {
    value = std::stod(it->second, &consumed);
  } catch (const std::exception &) {
    throw std::invalid_argument("detector '" + name + "' param '" +
                                std::string(key) +
                                "' is not a number: " + it->second);
  }
  if (consumed != it->second.size()) {
    throw std::invalid_argument("detector '" + name + "' param '" +
                                std::string(key) +
                                "' is not a number: " + it->second);
  }
  return value;
}

DetectorRegistry &DetectorRegistry::instance() {
  static DetectorRegistry registry;
  return registry;
}

void DetectorRegistry::add(std::string name, TargetClass target_class,
                           DetectorFactory factory) {
  if (name.empty()) {
    throw std::invalid_argument("detector name must not be empty");
  }
  if (!factory) {
    throw std::invalid_argument("detector '" + name + "' has no factory");
  }
  const auto [it, inserted] = entries_.emplace(
      std::move(name), Entry{target_class, std::move(factory)});
  if (!inserted) {
    throw std::invalid_argument("detector '" + it->first +
                                "' is registered twice");
  }
}

bool DetectorRegistry::contains(std::string_view name) const {
  return entries_.find(name) != entries_.end();
}

std::vector<std::string> DetectorRegistry::names() const {
  std::vector<std::string> result;
  result.reserve(entries_.size());
  for (const auto &[name, entry] : entries_) {
    (void)entry;
    result.push_back(name);
  }
  return result;
}

std::unique_ptr<Detector>
DetectorRegistry::create(const DetectorConfig &config) const {
  const auto it = entries_.find(config.name);
  if (it == entries_.end()) {
    throw std::invalid_argument("unknown detector '" + config.name +
                                "'; registered: " + [this] {
                                  std::string joined;
                                  for (const auto &name : names()) {
                                    if (!joined.empty()) {
                                      joined += ", ";
                                    }
                                    joined += name;
                                  }
                                  return joined;
                                }());
  }
  if (config.target_class != TargetClass::Unknown &&
      config.target_class != it->second.target_class) {
    throw std::invalid_argument(
        "detector '" + config.name + "' is registered as '" +
        std::string(to_string(it->second.target_class)) +
        "' but the config declares '" +
        std::string(to_string(config.target_class)) + "'");
  }
  std::unique_ptr<Detector> detector = it->second.factory(config);
  if (!detector) {
    throw std::invalid_argument("detector '" + config.name +
                                "' factory returned null");
  }
  return detector;
}

std::vector<std::unique_ptr<Detector>>
create_enabled_detectors(std::istream &config_text) {
  std::vector<std::string> enabled_names;
  std::map<std::string, std::map<std::string, std::string>> params;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(config_text, line)) {
    ++line_number;
    const std::string_view stripped = trim(line);
    if (stripped.empty() || stripped.front() == '#') {
      continue;
    }
    const std::size_t equals = stripped.find('=');
    if (equals == std::string_view::npos) {
      throw std::invalid_argument("config line " + std::to_string(line_number) +
                                  " has no '=': " + std::string(stripped));
    }
    const std::string key(trim(stripped.substr(0, equals)));
    const std::string value(trim(stripped.substr(equals + 1)));
    if (key == "enabled") {
      enabled_names = split(value, ',');
      continue;
    }
    const std::size_t dot = key.find('.');
    if (dot == std::string::npos) {
      throw std::invalid_argument("config line " + std::to_string(line_number) +
                                  " key must be '<detector>.<param>': " + key);
    }
    const std::string detector_name = key.substr(0, dot);
    const std::string param_name = key.substr(dot + 1);
    auto &detector_params = params[detector_name];
    if (!detector_params.emplace(param_name, value).second) {
      throw std::invalid_argument("config line " + std::to_string(line_number) +
                                  " repeats '" + key + "'");
    }
  }
  if (enabled_names.empty()) {
    throw std::invalid_argument(
        "config has no 'enabled = <detector>,<detector>'");
  }
  std::vector<std::unique_ptr<Detector>> detectors;
  detectors.reserve(enabled_names.size());
  for (const auto &name : enabled_names) {
    DetectorConfig config;
    config.name = name;
    const auto params_it = params.find(name);
    if (params_it != params.end()) {
      config.params = params_it->second;
    }
    detectors.push_back(DetectorRegistry::instance().create(config));
  }
  return detectors;
}

std::vector<std::unique_ptr<Detector>>
create_enabled_detectors_from_file(const std::string &path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::invalid_argument("cannot open vision config: " + path);
  }
  return create_enabled_detectors(stream);
}

} // namespace diesel_vision_frontend
