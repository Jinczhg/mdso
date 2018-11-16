#ifndef INCLUDE_AFFINELIGHTTRANSFORM
#define INCLUDE_AFFINELIGHTTRANSFORM

#include <Eigen/Core>
#include <ceres/ceres.h>
#include <ostream>
#include <cstring>

namespace fishdso {

template <typename T> struct AffineLightTransform {
  AffineLightTransform() : data{T(0.0), T(0.0)} {}
  AffineLightTransform(const T &a, const T &b) : data{a, b} {}

  AffineLightTransform<T> &operator=(const AffineLightTransform<T> &other) {
    memcpy(data, other.data, 2 * sizeof(T));
    return *this;
  }

  EIGEN_STRONG_INLINE T operator()(const T &x) {
    return ceres::exp(data[0]) * (x + data[1]);
  }

  EIGEN_STRONG_INLINE friend AffineLightTransform<T>
  operator*(AffineLightTransform<T> first, AffineLightTransform<T> second) {
    return AffineLightTransform<T>(first.data[0] + second.data[0],
                                   ceres::exp(second.data[0]) * second.data[1] +
                                       first.data[1]);
  }

  EIGEN_STRONG_INLINE static void
  normalizeMultiplier(AffineLightTransform<T> &toNormalize,
                      AffineLightTransform<T> &relative) {
    relative.data[0] -= toNormalize.data[0];
    toNormalize.data[0] = T(0);
  }

  friend std::ostream &operator<<(std::ostream &os,
                                  AffineLightTransform<T> affLight) {
    os << "raw =     " << affLight.data[0] << ' ' << affLight.data[1]
       << "\nas ax+b = " << ceres::exp(affLight.data[0]) << ' '
       << ceres::exp(affLight.data[0]) * affLight.data[1] << '\n';
    return os;
  }

  T data[2];
};

} // namespace fishdso

#endif