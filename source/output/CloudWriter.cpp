#include "output/CloudWriter.h"

#define MAX_DEPTH 100

namespace fishdso {

CloudWriter::CloudWriter(CameraModel *cam, const std::string &outputDirectory,
                         const std::string &fileName)
    : cam(cam)
    , cloudHolder(fileInDir(outputDirectory, fileName)) {}

void CloudWriter::keyFramesMarginalized(
    const std::vector<const KeyFrame *> &marginalized) {
  for (const KeyFrame *kf : marginalized) {
    std::vector<Vec3> points;
    std::vector<cv::Vec3b> colors;

    for (const auto &op : kf->optimizedPoints) {
      Vec3 worldPoint = kf->thisToWorld *
                        (op->depth() * cam->unmap(op->p).normalized());
      if (worldPoint[2] < MAX_DEPTH) { // MAX_DEPTH
        points.push_back(worldPoint);
        colors.push_back(
            kf->preKeyFrame->frameColored.at<cv::Vec3b>(toCvPoint(op->p)));
      }
    }
    for (const auto &ip : kf->immaturePoints) {
      if (ip->numTraced > 0) {
        Vec3 worldPoint = kf->thisToWorld *
                          (ip->depth * cam->unmap(ip->p).normalized());
        if (worldPoint[2] < MAX_DEPTH) { // MAX_DEPTH
          points.push_back(worldPoint);
          colors.push_back(
              kf->preKeyFrame->frameColored.at<cv::Vec3b>(toCvPoint(ip->p)));
        }
      }
    }

    int kfnum = kf->preKeyFrame->globalFrameNum;
    std::ofstream kfOut(outputDirectory + "/kf" + std::to_string(kfnum) +
                        ".ply");
    printInPly(kfOut, points, colors);
    kfOut.close();
    cloudHolder.putPoints(points, colors);
  }

  cloudHolder.updatePointCount();
}

void CloudWriter::destructed(
    const std::vector<const KeyFrame *> &lastKeyFrames) {
  keyFramesMarginalized(lastKeyFrames);
}

} // namespace fishdso
