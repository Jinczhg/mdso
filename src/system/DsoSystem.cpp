#include "system/DsoSystem.h"
#include "system/AffineLightTransform.h"
#include "util/settings.h"

namespace fishdso {

DsoSystem::DsoSystem(CameraModel *cam)
    : cam(cam), camPyr(cam->camPyr()), dsoInitializer(cam),
      isInitialized(false), curFrameNum(0) {}

SE3 predictInternal(int prevFramesSkipped, const SE3 &worldToLastKf,
                    const SE3 &worldToLbo, const SE3 &worldToLast) {
  SE3 lboToLast = worldToLast * worldToLbo.inverse();

  double alpha = 1.0 / prevFramesSkipped;
  SO3 lastToCurRot = SO3::exp(alpha * lboToLast.so3().log());
  Vec3 lastToCurTrans = alpha * (lastToCurRot * lboToLast.so3().inverse() *
                                 lboToLast.translation());

  SE3 worldToCur = SE3(lastToCurRot, lastToCurTrans) * worldToLast;

  return SE3(lastToCurRot, lastToCurTrans) * worldToLast *
         worldToLastKf.inverse();
}

SE3 DsoSystem::predictKfToCur() {
  PreKeyFrame *lastKf = keyFrames.rbegin()->second.preKeyFrame.get();

  int prevFramesSkipped =
      worldToFrame.rbegin()->first - (++worldToFrame.rbegin())->first;

  SE3 worldToLbo = (++worldToFrame.rbegin())->second;
  SE3 worldToLast = worldToFrame.rbegin()->second;

  return predictInternal(prevFramesSkipped, lastKf->worldToThis, worldToLbo,
                         worldToLast);
}

SE3 DsoSystem::purePredictKfToCur() {
  PreKeyFrame *lastKf = keyFrames.rbegin()->second.preKeyFrame.get();

  int prevFramesSkipped = worldToFramePredict.rbegin()->first -
                          (++worldToFramePredict.rbegin())->first;

  SE3 worldToLbo = (++worldToFramePredict.rbegin())->second;
  SE3 worldToLast = worldToFramePredict.rbegin()->second;

  return predictInternal(prevFramesSkipped, lastKf->worldToThis, worldToLbo,
                         worldToLast);
}

void DsoSystem::addFrame(const cv::Mat &frame) {
  curFrameNum++;

  if (!isInitialized) {
    isInitialized = dsoInitializer.addFrame(frame, curFrameNum);

    if (isInitialized) {
      std::vector<KeyFrame> kf =
          dsoInitializer.createKeyFrames(DsoInitializer::SPARSE_DEPTHS);
      for (const auto &f : kf)
        worldToFramePredict[f.preKeyFrame->globalFrameNum] =
            worldToFrame[f.preKeyFrame->globalFrameNum] =
                f.preKeyFrame->worldToThis;
      for (int i = 0; i < int(kf.size()); ++i)
        keyFrames.insert(std::pair<int, KeyFrame>(i, std::move(kf[i])));

      PreKeyFrame *lastKf = keyFrames.rbegin()->second.preKeyFrame.get();
      frameTracker =
          std::unique_ptr<FrameTracker>(new FrameTracker(camPyr, lastKf));
    }

    return;
  }

  std::unique_ptr<PreKeyFrame> preKeyFrame(new PreKeyFrame(frame, curFrameNum));

  SE3 kfToCur;
  AffineLightTransform<double> lightKfToCur;

  std::cout << "\n\n\nTRACK NEXT" << std::endl;

  SE3 purePredicted = purePredictKfToCur();
  SE3 predicted = predictKfToCur();
  std::tie(kfToCur, lightKfToCur) =
      frameTracker->trackFrame(preKeyFrame.get(), predicted, lightKfToLast);

  SE3 diff = kfToCur * predicted.inverse();
  // std::cout << "diff to predicted:" << std::endl;
  // std::cout << "trans = " << diff.translation().norm()
  // << "\nrot = " << diff.so3().log().norm() << std::endl;

  std::cout << "aff = " << lightKfToCur.data[0] << ' ' << lightKfToCur.data[1]
            << std::endl;

  PreKeyFrame *lastKf = keyFrames.rbegin()->second.preKeyFrame.get();
  worldToFrame[curFrameNum] = kfToCur * lastKf->worldToThis;
  worldToFramePredict[curFrameNum] = purePredicted * lastKf->worldToThis;
  preKeyFrame->worldToThis = worldToFrame[curFrameNum];
  // worldToFrame[curFrameNum] = predicted * lastKf->worldToThis;
  lightKfToLast = lightKfToCur;
}

void putMotion(std::ostream &out, const SE3 &motion) {
  out << motion.unit_quaternion().coeffs().transpose() << ' ';
  out << motion.translation().transpose();
}

void DsoSystem::printLastKfInPly(std::ostream &out) {
  KeyFrame &lastKf = keyFrames.rbegin()->second;
  out.precision(15);
  out << R"__(ply
format ascii 1.0
element vertex )__"
      << lastKf.interestPoints.size() << R"__(
property float x
property float y
property float z
property uchar red
property uchar green
property uchar blue
end_header
)__";

  for (const InterestPoint &ip : lastKf.interestPoints) {
    Vec3 pos = cam->unmap(ip.p.data()).normalized() * ip.depth;
    out << pos[0] << ' ' << pos[1] << ' ' << pos[2] << ' ';
    cv::Vec3b color = lastKf.frameColored.at<cv::Vec3b>(toCvPoint(ip.p));
    out << int(color[2]) << ' ' << int(color[1]) << ' ' << int(color[0])
        << std::endl;
  }
}

void DsoSystem::printTrackingInfo(std::ostream &out) {
  out.precision(15);
  for (auto p : worldToFrame) {
    out << p.first << ' ';
    putMotion(out, p.second);
    out << std::endl;
  }
}

void DsoSystem::printPredictionInfo(std::ostream &out) {
  out.precision(15);
  for (auto p : worldToFramePredict) {
    out << p.first << ' ';
    putMotion(out, p.second);
    out << std::endl;
  }
}

} // namespace fishdso