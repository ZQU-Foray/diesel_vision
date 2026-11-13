#include "foray_vision/armor_pipeline.hpp"
#include "foray_vision/dual_detector.hpp"

#include <MvCameraControl.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string pose_model;
  std::string pattern_model;
  std::string calibration;
  float armor_width_mm = 0.0F;
  float armor_height_mm = 0.0F;
  int camera_index = 0;
  int threads = 0;
};

void check_mvs(int status, const char *action) {
  if (status != MV_OK) {
    std::ostringstream out;
    out << action << " failed: MVS error 0x" << std::hex
        << static_cast<unsigned int>(status);
    throw std::runtime_error(out.str());
  }
}

Options parse_args(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      std::cout
          << "Usage: foray_vision_camera_debug --pose-model FILE "
             "--pattern-model FILE "
             "[--calibration FILE --armor-width-mm MM --armor-height-mm MM] "
             "[--camera-index N] [--threads N]\n"
             "No serial port, gimbal command, tracking or firing is used. "
             "Press q/Esc to quit.\n";
      std::exit(0);
    }
    if (i + 1 >= argc) {
      throw std::runtime_error("missing value for " + arg);
    }
    const std::string value = argv[++i];
    if (arg == "--pose-model")
      options.pose_model = value;
    else if (arg == "--pattern-model")
      options.pattern_model = value;
    else if (arg == "--calibration")
      options.calibration = value;
    else if (arg == "--armor-width-mm")
      options.armor_width_mm = std::stof(value);
    else if (arg == "--armor-height-mm")
      options.armor_height_mm = std::stof(value);
    else if (arg == "--camera-index")
      options.camera_index = std::stoi(value);
    else if (arg == "--threads")
      options.threads = std::stoi(value);
    else
      throw std::runtime_error("unknown option: " + arg);
  }
  if (options.pose_model.empty() || options.pattern_model.empty() ||
      options.camera_index < 0 || options.threads < 0) {
    throw std::runtime_error("two model paths are required; camera "
                             "index/threads must be non-negative");
  }
  const bool has_size = std::isfinite(options.armor_width_mm) &&
                        std::isfinite(options.armor_height_mm) &&
                        options.armor_width_mm > 0.0F &&
                        options.armor_height_mm > 0.0F;
  if ((!options.calibration.empty() && !has_size) ||
      (options.calibration.empty() &&
       (options.armor_width_mm != 0.0F || options.armor_height_mm != 0.0F))) {
    throw std::runtime_error(
        "PnP needs calibration and both measured outer armor dimensions");
  }
  return options;
}

class Camera {
public:
  explicit Camera(int index) {
    try {
      check_mvs(MV_CC_Initialize(), "SDK initialize");
      initialized_ = true;
      MV_CC_DEVICE_INFO_LIST devices{};
      check_mvs(MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &devices),
                "camera enumeration");
      if (static_cast<unsigned int>(index) >= devices.nDeviceNum) {
        throw std::runtime_error("camera index out of range; discovered " +
                                 std::to_string(devices.nDeviceNum));
      }
      check_mvs(MV_CC_CreateHandle(&handle_, devices.pDeviceInfo[index]),
                "create camera handle");
      check_mvs(MV_CC_OpenDevice(handle_, MV_ACCESS_Exclusive, 0),
                "open camera (close MVS preview if it owns the camera)");
      opened_ = true;
      // 某些 MVS 设备/SDK 不支持该策略；保持默认取流仍可安全调试。
      const int strategy_status =
          MV_CC_SetGrabStrategy(handle_, MV_GrabStrategy_LatestImagesOnly);
      if (strategy_status != MV_OK) {
        std::cerr
            << "Latest-frame strategy unsupported; using camera SDK default\n";
      }
      // 不改曝光、增益、分辨率或触发模式；沿用当前相机配置。
      check_mvs(MV_CC_StartGrabbing(handle_),
                "start grabbing (check trigger mode)");
      grabbing_ = true;
    } catch (...) {
      cleanup();
      throw;
    }
  }
  Camera(const Camera &) = delete;
  Camera &operator=(const Camera &) = delete;
  ~Camera() { cleanup(); }

  bool read(cv::Mat &image, unsigned int &frame_number) {
    MV_FRAME_OUT frame{};
    const int status = MV_CC_GetImageBuffer(handle_, &frame, 500);
    if (status != MV_OK)
      return false;
    try {
      const auto &info = frame.stFrameInfo;
      if (!frame.pBufAddr || info.nWidth == 0 || info.nHeight == 0 ||
          info.nWidth > 8192 || info.nHeight > 8192) {
        throw std::runtime_error("invalid camera frame size or buffer");
      }
      std::vector<unsigned char> bgr(static_cast<std::size_t>(info.nWidth) *
                                     info.nHeight * 3);
      MV_CC_PIXEL_CONVERT_PARAM_EX convert{};
      convert.nWidth = info.nWidth;
      convert.nHeight = info.nHeight;
      convert.enSrcPixelType = info.enPixelType;
      convert.pSrcData = frame.pBufAddr;
      convert.nSrcDataLen = info.nFrameLen;
      convert.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
      convert.pDstBuffer = bgr.data();
      convert.nDstBufferSize = static_cast<unsigned int>(bgr.size());
      check_mvs(MV_CC_ConvertPixelTypeEx(handle_, &convert),
                "convert camera frame to BGR");
      image = cv::Mat(static_cast<int>(info.nHeight),
                      static_cast<int>(info.nWidth), CV_8UC3, bgr.data())
                  .clone();
      frame_number = info.nFrameNum;
    } catch (...) {
      MV_CC_FreeImageBuffer(handle_, &frame);
      throw;
    }
    check_mvs(MV_CC_FreeImageBuffer(handle_, &frame), "release camera frame");
    return true;
  }

private:
  void cleanup() {
    if (grabbing_)
      MV_CC_StopGrabbing(handle_);
    if (opened_)
      MV_CC_CloseDevice(handle_);
    if (handle_)
      MV_CC_DestroyHandle(handle_);
    if (initialized_)
      MV_CC_Finalize();
    grabbing_ = false;
    opened_ = false;
    handle_ = nullptr;
    initialized_ = false;
  }
  void *handle_ = nullptr;
  bool initialized_ = false;
  bool opened_ = false;
  bool grabbing_ = false;
};

struct Calibration {
  cv::Mat camera_matrix;
  cv::Mat distortion;
  int width = 0;
  int height = 0;
};

Calibration load_calibration(const std::string &path) {
  cv::FileStorage file(path, cv::FileStorage::READ);
  if (!file.isOpened())
    throw std::runtime_error("cannot open calibration: " + path);
  Calibration cal;
  file["image_width"] >> cal.width;
  file["image_height"] >> cal.height;
  file["camera_matrix"] >> cal.camera_matrix;
  file["distortion_coefficients"] >> cal.distortion;
  if (cal.width <= 0 || cal.height <= 0 || cal.camera_matrix.rows != 3 ||
      cal.camera_matrix.cols != 3 || cal.camera_matrix.type() != CV_64F ||
      cal.distortion.empty() || cal.distortion.type() != CV_64F ||
      !cv::checkRange(cal.camera_matrix) || !cv::checkRange(cal.distortion)) {
    throw std::runtime_error(
        "invalid calibration dimensions/matrix/distortion");
  }
  return cal;
}

void put_line(cv::Mat &image, const std::string &text, int line,
              cv::Scalar color) {
  cv::putText(image, text, cv::Point(8, 24 + line * 22),
              cv::FONT_HERSHEY_SIMPLEX, 0.52, color, 1, cv::LINE_AA);
}

std::string class_name(int id) {
  if (id == 0)
    return "sentry";
  if (id == 1)
    return "infantry";
  if (id == 2)
    return "heavy";
  return "unknown";
}

void draw(cv::Mat &image, const foray_vision::DualDetector::Frame &detected,
          const foray_vision::VisionFrameResult *solved,
          unsigned int frame_number, const std::string &pnp_status) {
  const int panel_width = 360;
  cv::Mat canvas(image.rows, image.cols + panel_width, CV_8UC3,
                 cv::Scalar(28, 28, 28));
  image.copyTo(canvas(cv::Rect(0, 0, image.cols, image.rows)));
  const int panel_x = image.cols + 8;
  auto panel = [&](const std::string &text, int line,
                   cv::Scalar color = cv::Scalar(230, 230, 230)) {
    cv::putText(canvas, text, cv::Point(panel_x, 24 + line * 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.46, color, 1, cv::LINE_AA);
  };
  panel("VISION CAMERA TEST - NO CONTROL", 0, cv::Scalar(0, 200, 255));
  panel("Frame: " + std::to_string(frame_number), 2);
  panel("Pose candidates: " + std::to_string(detected.pose_candidates), 3);
  panel("Pattern candidates: " + std::to_string(detected.pattern_candidates),
        4);
  panel("Paired armors: " + std::to_string(detected.armors.size()), 5);
  panel("Detect: " +
            std::to_string(static_cast<int>(detected.timing.total_ms)) + " ms",
        6);
  panel(pnp_status, 8, cv::Scalar(0, 190, 255));
  panel("Coordinates: CAMERA frame only", 9);
  panel("Extrinsics unverified", 10, cv::Scalar(0, 190, 255));
  panel("q/Esc: quit", 12);

  for (std::size_t i = 0; i < detected.armors.size(); ++i) {
    const auto &armor = detected.armors[i];
    if (armor.corners.size() != 4)
      continue;
    const cv::Scalar green(80, 230, 80);
    for (int j = 0; j < 4; ++j) {
      cv::line(canvas, armor.corners[j], armor.corners[(j + 1) % 4], green, 2);
      cv::circle(canvas, armor.corners[j], 4, cv::Scalar(0, 230, 255), -1);
      cv::putText(canvas, std::to_string(j),
                  armor.corners[j] + cv::Point2f(5, -5),
                  cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 230, 255), 1);
    }
    std::ostringstream label;
    label << i << ' ' << class_name(armor.id) << ' '
          << (armor.color == foray_vision::ArmorColor::Red ? "red" : "blue")
          << " conf=" << std::fixed << std::setprecision(2) << armor.confidence;
    const int y = std::max(16, static_cast<int>(armor.corners[0].y) - 8);
    cv::putText(canvas, label.str(),
                cv::Point(std::max(0, static_cast<int>(armor.corners[0].x)), y),
                cv::FONT_HERSHEY_SIMPLEX, 0.42, green, 1);
    panel(label.str(), 14 + static_cast<int>(i) * 3);
    if (solved && i < solved->pnp_by_detection.size()) {
      const auto &pose = solved->pnp_by_detection[i];
      if (pose.success) {
        std::ostringstream value;
        value << std::fixed << std::setprecision(2)
              << "cam xyz[m]=" << pose.tvec_in_camera[0] << ','
              << pose.tvec_in_camera[1] << ',' << pose.tvec_in_camera[2]
              << " err=" << pose.reprojection_error << "px";
        panel(value.str(), 15 + static_cast<int>(i) * 3,
              cv::Scalar(100, 230, 100));
      } else {
        panel("PnP failed", 15 + static_cast<int>(i) * 3,
              cv::Scalar(0, 0, 255));
      }
    }
  }
  image = std::move(canvas);
}

} // namespace

int main(int argc, char **argv) {
  try {
    const Options options = parse_args(argc, argv);
    const bool use_pnp = !options.calibration.empty();
    Calibration calibration;
    std::unique_ptr<foray_vision::Solver> solver;
    foray_vision::ArmorSize armor_size{};
    if (use_pnp) {
      calibration = load_calibration(options.calibration);
      // 只读取 Solver 的相机系结果；单位由毫米显式换成米，云台外参未验证。
      armor_size = {options.armor_width_mm / 1000.0F,
                    options.armor_height_mm / 1000.0F};
      const foray_vision::Camera2gimbal identity{cv::Matx33d::eye(),
                                                 cv::Vec3d(0, 0, 0)};
      solver = std::make_unique<foray_vision::Solver>(
          calibration.camera_matrix, calibration.distortion, identity);
    }
    foray_vision::DualDetector detector(options.pose_model,
                                        options.pattern_model, options.threads);
    Camera camera(options.camera_index);
    cv::namedWindow("Foray Vision - camera only / NO CONTROL",
                    cv::WINDOW_NORMAL);
    unsigned int failures = 0;
    while (true) {
      cv::Mat image;
      unsigned int frame_number = 0;
      if (!camera.read(image, frame_number)) {
        if (++failures % 10 == 1)
          std::cerr << "Camera frame timeout (check trigger and connection)\n";
        const int key = cv::waitKey(1) & 0xff;
        if (key == 'q' || key == 27)
          break;
        continue;
      }
      failures = 0;
      const auto detected = detector.detect(image);
      foray_vision::VisionFrameResult solved;
      const foray_vision::VisionFrameResult *solved_ptr = nullptr;
      std::string status = "PnP OFF (no calibration + size)";
      if (use_pnp) {
        if (image.cols != calibration.width ||
            image.rows != calibration.height) {
          status = "PnP OFF: image/calibration SIZE MISMATCH";
        } else {
          solved = foray_vision::process_armor_solver(detected.armors, *solver,
                                                      armor_size);
          solved_ptr = &solved;
          status = "PnP ON: outer W/H from CLI";
        }
      }
      draw(image, detected, solved_ptr, frame_number, status);
      cv::imshow("Foray Vision - camera only / NO CONTROL", image);
      const int key = cv::waitKey(1) & 0xff;
      if (key == 'q' || key == 27)
        break;
    }
    cv::destroyAllWindows();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Camera debug error: " << error.what() << '\n';
    return 1;
  }
}
