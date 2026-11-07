// 培训3 · 任务1：HSV 通道分离 + 红色二值掩膜（用法见 README.md）

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#ifndef HSV_DEMO_SOURCE_DIR
#define HSV_DEMO_SOURCE_DIR ""
#endif

namespace {

// 红色在 HSV 里跨 0/180 边界，必须取两段并集；画面整体偏暗，V 下限放低。
struct RedThreshold {
    int lowH1 = 0, highH1 = 10;
    int lowH2 = 170, highH2 = 180;
    int minS = 80, minV = 50;
    int maxS = 255, maxV = 255;
};

cv::Mat makeRedMask(const cv::Mat& bgr, const RedThreshold& t = RedThreshold{}) {
    CV_Assert(!bgr.empty());
    cv::Mat src = bgr;
    if (src.channels() == 4) {
        cv::cvtColor(src, src, cv::COLOR_BGRA2BGR);
    } else if (src.channels() == 1) {
        cv::cvtColor(src, src, cv::COLOR_GRAY2BGR);
    }

    cv::Mat blurred, hsv;
    cv::GaussianBlur(src, blurred, cv::Size(3, 3), 0);
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);

    cv::Mat lower, upper, mask;
    cv::inRange(hsv, cv::Scalar(t.lowH1, t.minS, t.minV),
                cv::Scalar(t.highH1, t.maxS, t.maxV), lower);
    cv::inRange(hsv, cv::Scalar(t.lowH2, t.minS, t.minV),
                cv::Scalar(t.highH2, t.maxS, t.maxV), upper);
    cv::bitwise_or(lower, upper, mask);

    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    return mask;
}

std::string resolveAsset(const std::string& relative) {
    std::error_code ec;
    if (std::filesystem::exists(relative, ec)) return relative;
    const std::string root = HSV_DEMO_SOURCE_DIR;
    if (!root.empty()) {
        const std::string absolute = (std::filesystem::path(root) / relative).string();
        if (std::filesystem::exists(absolute, ec)) return absolute;
    }
    return relative;
}

bool headlessMode() {
    const char* value = std::getenv("ARMOR_HEADLESS");
    return value != nullptr && std::strcmp(value, "0") != 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string imagePath = "assets/armor.jpg";
    std::string maskPath = "red_mask.png";
    bool show = !headlessMode();

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--image" && i + 1 < argc) {
            imagePath = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            maskPath = argv[++i];
        } else if (arg == "--show") {
            show = true;
        } else if (arg == "--no-show") {
            show = false;
        } else if (arg.rfind("--", 0) != 0) {
            imagePath = arg;
        } else {
            std::cerr << "[hsv_demo] 未知参数: " << arg << '\n';
            return 1;
        }
    }

    const std::string resolved = resolveAsset(imagePath);
    const cv::Mat bgr = cv::imread(resolved, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        std::cerr << "[hsv_demo] 读不到图片: " << resolved << '\n'
                  << "用法: hsv_demo [--image assets/armor.jpg] [--out red_mask.png] [--show|--no-show]\n";
        return 1;
    }

    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    if (show) {
        cv::imshow("original", bgr);
        cv::imshow("H channel", channels[0]);
        cv::imshow("S channel", channels[1]);
        cv::imshow("V channel", channels[2]);
        cv::moveWindow("original", 30, 40);
        cv::moveWindow("H channel", 360, 40);
        cv::moveWindow("S channel", 690, 40);
        cv::moveWindow("V channel", 1020, 40);
    }

    const cv::Mat mask = makeRedMask(bgr);
    if (show) cv::imshow("red mask", mask);
    if (!cv::imwrite(maskPath, mask)) {
        std::cerr << "[hsv_demo] 保存失败: " << maskPath << '\n';
        return 2;
    }

    std::cout << "[hsv_demo] 输入图片: " << resolved << '\n';
    std::cout << "[hsv_demo] 尺寸: " << bgr.cols << "x" << bgr.rows << '\n';
    std::cout << "[hsv_demo] 掩膜非零像素: " << cv::countNonZero(mask) << '\n';
    std::cout << "[hsv_demo] 掩膜已保存: " << maskPath << '\n';

    if (show) {
        std::cout << "[hsv_demo] 按任意键关闭窗口...\n";
        cv::waitKey(0);
        cv::destroyAllWindows();
    } else {
        std::cout << "[hsv_demo] headless: 已跳过窗口展示\n";
    }
    return 0;
}
