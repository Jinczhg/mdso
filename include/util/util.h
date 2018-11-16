#ifndef INCLUDE_UTIL
#define INCLUDE_UTIL
#include "util/settings.h"
#include "util/types.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace fishdso {

extern cv::Mat dbg;
extern double minDepthCol, maxDepthCol;

template <typename T>
EIGEN_STRONG_INLINE std::vector<T> reservedVector(int toReserve) {
  std::vector<T> res;
  res.reserve(toReserve);
  return res;
}

void setDepthColBounds(const std::vector<double> &depths);

void putMotion(std::ostream &out, const SE3 &motion);

double angle(const Vec3 &a, const Vec3 &b);

void putDot(cv::Mat &img, const cv::Point &pos, const cv::Scalar &col);
void putCross(cv::Mat &img, const cv::Point &pos, const cv::Scalar &col,
              int size, int thikness);

void grad(const cv::Mat &img, cv::Mat1d &gradX, cv::Mat1d &gradY,
          cv::Mat1d &gradNorm);
double gradNormAt(const cv::Mat1b &img, const cv::Point &p);

cv::Scalar depthCol(double d, double mind, double maxd);

void insertDepths(cv::Mat &img, const StdVector<Vec2> &points,
                  const std::vector<double> &depths, double minDepth,
                  double maxDepth, bool areSolidPnts);

Vec2 toVec2(cv::Point p);
cv::Point toCvPoint(Vec2 vec);
cv::Point toCvPoint(const Vec2 &vec, double scaleX, double scaleY,
                    cv::Point shift);

cv::Vec3b toCvVec3bDummy(cv::Scalar scalar);

template <typename TT> struct accum_type { typedef TT type; };
template <> struct accum_type<unsigned char> { typedef int type; };
template <> struct accum_type<signed char> { typedef int type; };
template <> struct accum_type<char> { typedef int type; };
template <> struct accum_type<cv::Vec3b> { typedef cv::Vec3i type; };

template <typename T> cv::Mat boxFilterPyrUp(const cv::Mat &img) {
  constexpr int d = 2;
  cv::Mat result(img.rows / d, img.cols / d, img.type());
  for (int y = 0; y < img.rows / d * d; y += d)
    for (int x = 0; x < img.cols / d * d; x += d) {
      typename accum_type<T>::type accum = typename accum_type<T>::type();
      for (int yy = 0; yy < d; ++yy)
        for (int xx = 0; xx < d; ++xx)
          accum += img.at<T>(y + yy, x + xx);
      result.at<T>(y / d, x / d) = T(accum / (d * d));
    }

  return result;
}

extern template cv::Mat boxFilterPyrUp<unsigned char>(const cv::Mat &img);
extern template cv::Mat boxFilterPyrUp<cv::Vec3b>(const cv::Mat &img);

cv::Mat pyrNUpDepth(const cv::Mat1d &integralWeightedDepths,
                    const cv::Mat1d &integralWeights, int levelNum);

cv::Mat drawDepthedFrame(const cv::Mat1b &frame, const cv::Mat1d &depths,
                         double minDepth, double maxDepth);

} // namespace fishdso

#endif