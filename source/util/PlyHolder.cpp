#include "util/PlyHolder.h"
#include <glog/logging.h>
#include <fstream>

#define MAX_DEPTH 100

namespace fishdso {

const int countSpace = 19;

PlyHolder::PlyHolder(const std::string &fname)
    : fname(fname)
    , pointCount(0) {
  std::ofstream fs(fname);

  if (!fs.good())
    throw std::runtime_error("File \"" + fname + "\" could not be created.");

  // PLY
//  fs << R"__(ply
//format ascii 1.0
//element vertex 0)__" +
//            std::string(countSpace - 1, ' ') +
//            R"__(
//property float x
//property float y
//property float z
//property uchar red
//property uchar green
//property uchar blue
//end_header
//)__";
  // PCD
  fs << R"__(# .PCD v0.6 - Point Cloud Data file format
VERSION 0.7
FIELDS x y z rgb
SIZE 4 4 4 4
TYPE F F F U
COUNT 1 1 1 1
WIDTH 0)__" +
            std::string(countSpace - 1, ' ') +
            R"__(
HEIGHT 1
#VIEWPOINT 0 0 0 1 0 0 0
POINTS 0)__" +
            std::string(countSpace - 1, ' ') +
            R"__(
DATA ascii
)__";

  fs.close();
}

void PlyHolder::putPoints(const std::vector<Vec3> &points,
                          const std::vector<cv::Vec3b> &colors) {
  LOG_IF(WARNING, points.size() != colors.size())
      << "Numbers of points and colors do not correspond in "
         "PlyHolder::putPoints."
      << std::endl;

  std::ofstream fs(fname, std::ios_base::app);
  int cnt = std::min(points.size(), colors.size());
  for (int i = 0; i < cnt; ++i) {
    const Vec3 &p = points[i];
    const cv::Vec3b &color = colors[i];
    if (!p.hasNaN() and p[2] < MAX_DEPTH) { // MAX_DEPTH
      fs << p[0] << ' ' << p[1] << ' ' << p[2] << ' ';
      // PLY
//      fs << int(color[2]) << ' ' << int(color[1]) << ' ' << int(color[0])
//         << '\n';
      // PCD
      uint32_t rgb = ((uint32_t) int(color[2]) << 16 | (uint32_t) int(color[1]) << 8 | (uint32_t) int(color[0]));
      fs << rgb << '\n';
      pointCount++;
    }
  }

//  pointCount += cnt;

  fs.close();
}

void PlyHolder::updatePointCount() {
  // PLY
//  const int countPos = 36;
  // PCD
  const int countPos = 118;
  const int countPos2 = 179;
  std::fstream fs(fname);
  std::string emplacedValue = std::to_string(pointCount);
  emplacedValue += std::string(countSpace - emplacedValue.length(), ' ');
  fs.seekp(countPos) << emplacedValue;
  // PCD
  fs.seekp(countPos2) << emplacedValue;
  fs.close();
}

} // namespace fishdso
