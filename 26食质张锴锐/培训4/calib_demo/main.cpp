// 培训4 · 相机标定：用棋盘格照片求内参 K 和畸变系数 D，并输出畸变矫正图。
// 用法：./calib_demo <标定图目录> [--show]
// 113 张标定原图不进仓库，目录由 argv[1] 传入（见仓库根 README 注意事项第 13 条）。

#include <opencv2/opencv.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
using namespace cv;
using namespace std;

int main(int argc, char** argv) {
   if (argc < 2) {
      cerr << "用法: " << argv[0] << " <标定图目录> [--show]" << endl;
      return -1;
   }
   const string dir = argv[1];
   const bool show = (argc > 2 && string(argv[2]) == "--show");

   const Size boardSize(10, 7);    // 内角点 10x7，不是方格数 11x8
   const float squareSize = 15.f;  // 每格 15mm，只影响平移向量尺度，不影响 K 和 D

   vector<Point3f> obj;
   for (int i = 0; i < boardSize.height; ++i)
      for (int j = 0; j < boardSize.width; ++j)
         obj.push_back(Point3f(j * squareSize, i * squareSize, 0.f));

   vector<string> images;
   glob(dir + "/*.jpg", images);
   sort(images.begin(), images.end());
   if (images.empty()) {
      cerr << "目录下没找到 jpg：" << dir << endl;
      return -1;
   }

   vector<vector<Point3f>> objectPoints;
   vector<vector<Point2f>> imagePoints;
   Mat gray;
   int detected = 0;
   for (size_t i = 0; i < images.size(); ++i) {
      Mat img = imread(images[i], IMREAD_COLOR);
      if (img.empty()) continue;
      cvtColor(img, gray, COLOR_BGR2GRAY);
      vector<Point2f> corners;
      bool found = findChessboardCorners(gray, boardSize, corners,
         CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);
      if (!found) continue;   // 十几张检不出来是正常的，跳过而不是中断
      cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1),
         TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.1));
      imagePoints.push_back(corners);
      objectPoints.push_back(obj);
      ++detected;
      if (show) {
         drawChessboardCorners(img, boardSize, corners, found);
         imshow("Chessboard", img);
         waitKey(500);
      }
   }

   cout << "发现图片: " << images.size() << " 张" << endl;
   cout << "角点检出: " << detected << " / " << images.size() << " 张" << endl;
   if (detected < 3) {
      cerr << "检出的图太少，无法标定" << endl;
      return -1;
   }

   Mat cameraMatrix, distCoeffs;
   vector<Mat> rvecs, tvecs;
   double rms = calibrateCamera(objectPoints, imagePoints, gray.size(),
                                cameraMatrix, distCoeffs, rvecs, tvecs);

   cout << "========== 标定结果 ==========" << endl;
   cout << "重投影误差 RMS = " << rms << " 像素" << endl;
   cout << "内参矩阵 K =\n" << cameraMatrix << endl;
   cout << "畸变系数 D =\n" << distCoeffs << "  (顺序 k1,k2,p1,p2,k3)" << endl;
   cout << "fx=" << cameraMatrix.at<double>(0, 0)
        << " fy=" << cameraMatrix.at<double>(1, 1)
        << " cx=" << cameraMatrix.at<double>(0, 2)
        << " cy=" << cameraMatrix.at<double>(1, 2) << endl;

   Mat src = imread(images[0], IMREAD_COLOR);
   Mat undistorted;
   undistort(src, undistorted, cameraMatrix, distCoeffs);
   imwrite("undistort_result.jpg", undistorted);
   Mat compare;
   hconcat(src, undistorted, compare);
   imwrite("shots/undistort_compare.jpg", compare);
   cout << "已写出 undistort_result.jpg 与 shots/undistort_compare.jpg" << endl;
   if (show) {
      imshow("undistort_compare", compare);
      waitKey(0);
   }
   return 0;
}
