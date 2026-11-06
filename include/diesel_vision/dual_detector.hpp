#ifndef FORAY_VISION_DUAL_DETECTOR_HPP
#define FORAY_VISION_DUAL_DETECTOR_HPP

<<<<<<< HEAD:include/diesel_vision/dual_detector.hpp
#include "diesel_vision/detector.hpp"
#include "diesel_vision/pattern_match.hpp"
=======
#include "foray_vision/armor_detection.hpp"
#include "foray_vision/pattern_match.hpp"
>>>>>>> 02baef7 (feat(vision):在detector.cpp现在只保留双模型检测的接口,去除了单模型的接口,模拟器的单模型还保留但是不能运行后续处理):include/foray_vision/dual_detector.hpp

#include <memory>
#include <string>

namespace diesel_vision {

// 颜色/四角点与图案模型各管一件事。
class DualDetector {
  public:
    // intra_op_threads=0 沿用 ONNX Runtime 默认值；可按部署机器实测调节。
    DualDetector(const std::string& color_pose_model, const std::string& pattern_model,
                 int intra_op_threads = 0);
    ~DualDetector();
    DualDetector(const DualDetector&) = delete;
    DualDetector& operator=(const DualDetector&) = delete;

    /// 单帧阶段耗时（毫秒）；两个模型并行，分项时间不能直接相加成总耗时。
    struct Timing {
        double preprocess_ms = 0.0;
        double pose_inference_ms = 0.0;
        double pose_postprocess_ms = 0.0;
        double pattern_inference_ms = 0.0;
        double pattern_postprocess_ms = 0.0;
        double match_ms = 0.0;
        double total_ms = 0.0;
    };

    struct Frame {
        std::vector<ArmorDetection> armors; // 仅唯一配对、四点可用的目标
        PatternMatchResult matches;         // 按原始候选下标保留失败原因
        std::size_t pose_candidates = 0;
        std::size_t pattern_candidates = 0;
        Timing timing;
    };
    Frame detect(const cv::Mat& bgr_image);

  private:
    class Model;
    std::unique_ptr<Model> pose_;
    std::unique_ptr<Model> pattern_;
};

} // namespace diesel_vision
#endif
