#pragma once

// 培训3 公共部分：红色掩膜、灯条检测、角点/连线绘制、路径解析
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifndef ARMOR_PROJECT_ROOT
#define ARMOR_PROJECT_ROOT ""
#endif

namespace armor {

struct RedThreshold {
    // 红色在 HSV 中跨 0/180 边界，需两段区间取并集；画面偏暗，S/V 下限放低。
    int lowH1 = 0;
    int highH1 = 10;
    int lowH2 = 170;
    int highH2 = 180;
    int minS = 80;
    int minV = 50;
    int maxS = 255;
    int maxV = 255;
};

struct DetectParams {
    double minArea = 40.0;
    double maxArea = 60000.0;
    int minWidth = 3;
    int minHeight = 12;
    double minAspect = 1.8;
    std::size_t maxBars = 2;
};

inline std::string projectPath(const std::string& relative) {
    const std::string root = ARMOR_PROJECT_ROOT;
    if (root.empty()) return relative;
    return (std::filesystem::path(root) / relative).string();
}

inline std::string resolveAsset(const std::string& relative) {
    std::error_code ec;
    if (std::filesystem::exists(relative, ec)) return relative;
    const std::string absolute = projectPath(relative);
    if (std::filesystem::exists(absolute, ec)) return absolute;
    return relative;
}

// ARMOR_HEADLESS=1 时跳过 imshow/waitKey，用于自动化跑批。
inline bool headlessMode() {
    const char* value = std::getenv("ARMOR_HEADLESS");
    return value != nullptr && std::strcmp(value, "0") != 0;
}

inline cv::Mat makeRedMask(const cv::Mat& bgr,
                           const RedThreshold& t = RedThreshold{}) {
    CV_Assert(!bgr.empty());
    cv::Mat src = bgr;
    if (src.channels() == 4) {
        cv::cvtColor(src, src, cv::COLOR_BGRA2BGR);
    } else if (src.channels() == 1) {
        cv::cvtColor(src, src, cv::COLOR_GRAY2BGR);
    }

    cv::Mat blurred;
    cv::GaussianBlur(src, blurred, cv::Size(3, 3), 0);

    cv::Mat hsv;
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);

    cv::Mat lowerRed, upperRed, mask;
    cv::inRange(hsv,
                cv::Scalar(t.lowH1, t.minS, t.minV),
                cv::Scalar(t.highH1, t.maxS, t.maxV),
                lowerRed);
    cv::inRange(hsv,
                cv::Scalar(t.lowH2, t.minS, t.minV),
                cv::Scalar(t.highH2, t.maxS, t.maxV),
                upperRed);
    cv::bitwise_or(lowerRed, upperRed, mask);

    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    return mask;
}

struct LightBar {
    cv::Rect bounding;
    cv::RotatedRect rotated;
    // 顺序固定为 左上/右上/右下/左下，armorQuad() 依赖它
    std::vector<cv::Point2f> corners;
    cv::Point2f center;
    double area = 0.0;
};

inline std::vector<cv::Point2f> orderedCorners(const cv::RotatedRect& rr) {
    cv::Point2f raw[4];
    rr.points(raw);
    std::vector<cv::Point2f> p(raw, raw + 4);

    // 不能按 x 排序分左右：灯条倾斜约 15° 时 x 最小的两点属于同一端，会错标角点、使连线交叉。
    // 用 45° 对角线极值定角：x+y 最小=左上，x-y 最大=右上，x+y 最大=右下，x-y 最小=左下。
    const auto bySum = [](const cv::Point2f& a, const cv::Point2f& b) {
        return a.x + a.y < b.x + b.y;
    };
    const auto byDiff = [](const cv::Point2f& a, const cv::Point2f& b) {
        return a.x - a.y < b.x - b.y;
    };
    std::vector<cv::Point2f> ordered{
        *std::min_element(p.begin(), p.end(), bySum),
        *std::max_element(p.begin(), p.end(), byDiff),
        *std::max_element(p.begin(), p.end(), bySum),
        *std::min_element(p.begin(), p.end(), byDiff),
    };

    // 长轴恰为 45° 时极值可能重合，退化为「按 y 分上下、组内按 x 分左右」。
    bool duplicated = false;
    for (std::size_t i = 0; i < ordered.size() && !duplicated; ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (std::fabs(ordered[i].x - ordered[j].x) < 0.5f &&
                std::fabs(ordered[i].y - ordered[j].y) < 0.5f) {
                duplicated = true;
                break;
            }
        }
    }
    if (duplicated) {
        const auto byX = [](const cv::Point2f& a, const cv::Point2f& b) {
            return a.x < b.x;
        };
        std::sort(p.begin(), p.end(),
                  [](const cv::Point2f& a, const cv::Point2f& b) { return a.y < b.y; });
        std::sort(p.begin(), p.begin() + 2, byX);
        std::sort(p.begin() + 2, p.end(), byX);
        return {p[0], p[1], p[3], p[2]};
    }
    return ordered;
}

inline std::vector<LightBar> detectLightBars(const cv::Mat& mask,
                                             const DetectParams& p = DetectParams{}) {
    std::vector<std::vector<cv::Point>> contours;
    // findContours 会改动输入（InputOutputArray），故在副本上检测。
    cv::Mat work = mask.clone();
    cv::findContours(work, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LightBar> bars;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < p.minArea || area > p.maxArea) continue;

        const cv::Rect box = cv::boundingRect(contour);
        if (box.width < p.minWidth || box.height < p.minHeight) continue;

        const double longSide = static_cast<double>(std::max(box.width, box.height));
        const double shortSide =
            static_cast<double>(std::max(1, std::min(box.width, box.height)));
        if (longSide / shortSide < p.minAspect) continue;

        const cv::RotatedRect rr = cv::minAreaRect(contour);
        LightBar bar;
        bar.bounding = box;
        bar.rotated = rr;
        bar.corners = orderedCorners(rr);
        bar.center = rr.center;
        bar.area = area;
        bars.push_back(bar);
    }

    // 先按面积挑最显著的两条，再按 x 排成左 -> 右
    std::sort(bars.begin(), bars.end(),
              [](const LightBar& a, const LightBar& b) { return a.area > b.area; });
    if (bars.size() > p.maxBars) bars.resize(p.maxBars);
    std::sort(bars.begin(), bars.end(),
              [](const LightBar& a, const LightBar& b) { return a.center.x < b.center.x; });
    return bars;
}

// 装甲板四个角点 = 左灯条的左上/左下 + 右灯条的右上/右下
inline std::vector<cv::Point> armorQuad(const std::vector<LightBar>& bars) {
    std::vector<cv::Point> quad;
    if (bars.empty()) return quad;

    const LightBar& left = bars.front();
    const LightBar& right = bars.back();
    const auto toInt = [](const cv::Point2f& pt) {
        return cv::Point(cvRound(pt.x), cvRound(pt.y));
    };

    quad.push_back(toInt(left.corners[0]));
    quad.push_back(toInt(right.corners[1]));
    quad.push_back(toInt(right.corners[2]));
    quad.push_back(toInt(left.corners[3]));
    return quad;
}

inline void drawCorners(cv::Mat& canvas,
                        const std::vector<cv::Point2f>& corners,
                        const cv::Scalar& color,
                        const std::string& prefix) {
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const cv::Point pt(cvRound(corners[i].x), cvRound(corners[i].y));
        cv::circle(canvas, pt, 4, color, cv::FILLED, cv::LINE_AA);
        cv::putText(canvas, prefix + std::to_string(i), pt + cv::Point(7, -7),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, color, 1, cv::LINE_AA);
    }
}

inline void drawPolygon(cv::Mat& canvas,
                        const std::vector<cv::Point>& quad,
                        const cv::Scalar& color,
                        int thickness = 2) {
    if (quad.size() < 3) return;
    const std::vector<std::vector<cv::Point>> polygons{quad};
    cv::polylines(canvas, polygons, true, color, thickness, cv::LINE_AA);
}

}  // namespace armor
