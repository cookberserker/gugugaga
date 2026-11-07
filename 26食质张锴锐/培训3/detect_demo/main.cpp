// 培训3 · 任务2：逐帧检测红色灯条，绘制外接矩形/角点/装甲板四边形并输出视频。
// 命令行参数、快捷键与 ARMOR_HEADLESS 开关见 README.md。

#include "armor_common.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

namespace {

const cv::Scalar kYellow(0, 255, 255);  // 最大外接矩形 / 装甲板连线
const cv::Scalar kOrange(255, 180, 0);  // 最小外接矩形
const cv::Scalar kGreen(0, 255, 0);     // 灯条四个角点
const cv::Scalar kRed(0, 0, 255);       // 装甲板四个角点

std::string frameShotName(int index) {
    std::ostringstream oss;
    oss << "frame_" << std::setw(4) << std::setfill('0') << index << ".png";
    return oss.str();
}

}  // namespace

int main(int argc, char** argv) {
    const bool headless = armor::headlessMode();
    const std::string videoPath =
        (argc > 1) ? argv[1] : armor::resolveAsset("assets/armor.mp4");
    const std::string outputPath = (argc > 2) ? argv[2] : "detect_demo.mp4";

    cv::VideoCapture cap(videoPath, cv::CAP_ANY);
    if (!cap.isOpened()) {
        std::cerr << "[task2] 打不开视频: " << videoPath << '\n'
                  << "用法: detect_demo [视频路径] [结果视频路径]\n";
        return 1;
    }

    const double reportedFps = cap.get(cv::CAP_PROP_FPS);
    const double fps =
        (reportedFps > 1.0 && reportedFps < 120.0) ? reportedFps : 30.0;
    const cv::Size frameSize(
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));

    std::error_code ec;
    const auto outDir = std::filesystem::path(outputPath).parent_path();
    if (!outDir.empty()) std::filesystem::create_directories(outDir, ec);

    cv::VideoWriter writer;
    if (frameSize.width > 0 && frameSize.height > 0) {
        writer.open(outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                    fps, frameSize, true);
    }
    const bool writingVideo = writer.isOpened();

    std::cout << "[task2] 输入: " << videoPath << '\n';
    std::cout << "[task2] 帧率: " << fps << ", 尺寸: "
              << frameSize.width << "x" << frameSize.height << '\n';
    std::cout << "[task2] 结果视频: "
              << (writingVideo ? outputPath : std::string("(编码器不可用，仅实时显示)"))
              << '\n';
    if (headless) {
        std::cout << "[task2] headless: 不弹窗，处理完整段视频\n";
    } else {
        std::cout << "[task2] q / ESC 退出, 空格 暂停, s 保存当前帧\n";
    }

    cv::Mat frame, canvas;
    int frameIndex = 0;
    int twoBarFrames = 0;
    bool savedFirstDetection = false;
    bool paused = false;

    while (true) {
        if (!paused) {
            cap >> frame;
            if (frame.empty()) break;
            ++frameIndex;
        }
        if (frame.empty()) break;

        const cv::Mat mask = armor::makeRedMask(frame);
        const std::vector<armor::LightBar> bars = armor::detectLightBars(mask);
        canvas = frame.clone();
        if (bars.size() >= 2) ++twoBarFrames;

        for (std::size_t i = 0; i < bars.size(); ++i) {
            const armor::LightBar& bar = bars[i];

            cv::rectangle(canvas, bar.bounding, kYellow, 2);

            cv::Point2f raw[4];
            bar.rotated.points(raw);
            std::vector<cv::Point> rotatedQuad;
            rotatedQuad.reserve(4);
            for (int k = 0; k < 4; ++k) {
                rotatedQuad.emplace_back(cvRound(raw[k].x), cvRound(raw[k].y));
            }
            armor::drawPolygon(canvas, rotatedQuad, kOrange, 1);

            armor::drawCorners(canvas, bar.corners, kGreen,
                               "P" + std::to_string(i) + "-");
        }

        if (bars.size() >= 2) {
            const std::vector<cv::Point> quad = armor::armorQuad(bars);
            armor::drawPolygon(canvas, quad, kYellow, 2);
            for (std::size_t i = 0; i < quad.size(); ++i) {
                cv::circle(canvas, quad[i], 5, kRed, cv::FILLED, cv::LINE_AA);
                cv::putText(canvas, "A" + std::to_string(i),
                            quad[i] + cv::Point(7, -7),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, kRed, 1, cv::LINE_AA);
            }
        } else if (bars.size() == 1) {
            // 只检出一条灯条时，降级为连接它自身的四个角点
            std::vector<cv::Point> quad;
            for (const auto& c : bars.front().corners) {
                quad.emplace_back(cvRound(c.x), cvRound(c.y));
            }
            armor::drawPolygon(canvas, quad, kYellow, 2);
        }

        std::ostringstream status;
        status << "frame " << frameIndex << " | bars " << bars.size()
               << (paused ? " | PAUSED" : "");
        cv::putText(canvas, status.str(), cv::Point(12, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, kYellow, 2, cv::LINE_AA);

        if (!headless) {
            cv::imshow("armor detection", canvas);
            cv::imshow("red mask", mask);
        }
        if (writingVideo) writer.write(canvas);

        if (!savedFirstDetection && !bars.empty()) {
            if (cv::imwrite("detection.png", canvas)) {
                std::cout << "[task2] 已保存检测截图: detection.png\n";
                savedFirstDetection = true;
            }
        }

        if (!headless) {
            int delay = static_cast<int>(1000.0 / fps);
            if (delay < 1) delay = 1;
            const int key = cv::waitKey(paused ? 0 : delay);
            if (key == 'q' || key == 'Q' || key == 27) break;
            if (key == ' ') paused = !paused;
            if (key == 's' || key == 'S') {
                const std::string shot = frameShotName(frameIndex);
                if (cv::imwrite(shot, canvas)) {
                    std::cout << "[task2] 已保存: " << shot << '\n';
                }
            }
        } else if (frameIndex % 100 == 0) {
            std::cout << "[task2] frame " << frameIndex
                      << " | bars " << bars.size() << '\n';
        }
    }

    cap.release();
    if (writingVideo) writer.release();
    if (!headless) cv::destroyAllWindows();

    std::cout << "[task2] 共处理 " << frameIndex << " 帧\n";
    std::cout << "[task2] 检出双灯条的帧: " << twoBarFrames << " / "
              << frameIndex << '\n';
    if (writingVideo) std::cout << "[task2] 结果视频: " << outputPath << '\n';
    return 0;
}
