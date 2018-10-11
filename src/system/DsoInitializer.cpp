#include "system/DsoInitializer.h"
#include "util/SphericalTerrain.h"
#include "util/defs.h"
#include "util/util.h"
#include <algorithm>
#include <opencv2/opencv.hpp>

namespace fishdso {

DsoInitializer::DsoInitializer(CameraModel *cam)
    : cam(cam), stereoMatcher(cam), hasFirstFrame(false), framesSkipped(0) {}

bool DsoInitializer::addFrame(const cv::Mat &frame, int globalFrameNum) {
  if (!hasFirstFrame) {
    frames[0] = frame;
    globalFrameNums[0] = globalFrameNum;
    hasFirstFrame = true;
    return false;
  } else {
    if (framesSkipped < settingFirstFramesSkip) {
      ++framesSkipped;
      return false;
    }

    frames[1] = frame;
    globalFrameNums[1] = globalFrameNum;
    return true;
  }
}

std::vector<KeyFrame> DsoInitializer::createKeyFrames(
    DsoInitializer::DebugOutputType debugOutputType) {
  return createKeyFramesFromStereo(NORMAL, debugOutputType);
}

std::vector<KeyFrame> DsoInitializer::createKeyFramesFromStereo(
    InterpolationType interpolationType,
    DsoInitializer::DebugOutputType debugOutputType) {
  StdVector<Vec2> keyPoints[2];
  std::vector<double> depths[2];
  SE3 motion = stereoMatcher.match(frames, keyPoints, depths);

  std::vector<KeyFrame> keyFrames;
  for (int i = 0; i < 2; ++i)
    keyFrames.push_back(KeyFrame(frames[i], globalFrameNums[i]));

  keyFrames[0].preKeyFrame->worldToThis = SE3();
  keyFrames[1].preKeyFrame->worldToThis = motion;

  if (interpolationType == PLAIN) {
    Terrain kpTerrains[2] = {Terrain(cam, keyPoints[0], depths[0]),
                             Terrain(cam, keyPoints[1], depths[1])};
    for (int i = 0; i < 2; ++i) {
      for (InterestPoint &ip : keyFrames[i].interestPoints) {
        double depth;
        if (kpTerrains[i](ip.p, depth))
          ip.depth = depth;
      }
      keyFrames[i].setDepthPyrs();
    }
  } else if (interpolationType == NORMAL) {
    std::vector<Vec3> depthedRays[2];
    for (int kfInd = 0; kfInd < 2; ++kfInd) {
      depthedRays[kfInd].reserve(keyPoints[kfInd].size());
      for (int i = 0; i < int(keyPoints[kfInd].size()); ++i)
        depthedRays[kfInd].push_back(
            cam->unmap(keyPoints[kfInd][i].data()).normalized() *
            depths[kfInd][i]);
    }

    SphericalTerrain kpTerrains[2] = {SphericalTerrain(depthedRays[0]),
                                      SphericalTerrain(depthedRays[1])};

    for (int kfInd = 0; kfInd < 2; ++kfInd) {
      const int reselectCount = 1;
      for (int i = 0; i < reselectCount + 1; ++i) {
        for (InterestPoint &ip : keyFrames[kfInd].interestPoints) {
          double depth;
          if (kpTerrains[kfInd](cam->unmap(ip.p.data()), depth))
            ip.depth = depth;
        }

        int pointsTotal = keyFrames[kfInd].interestPoints.size();
        auto it = std::remove_if(keyFrames[kfInd].interestPoints.begin(),
                                 keyFrames[kfInd].interestPoints.end(),
                                 [](InterestPoint p) { return p.depth < 0; });
        keyFrames[kfInd].interestPoints.erase(
            it, keyFrames[kfInd].interestPoints.end());
        int pointsInTriang = keyFrames[kfInd].interestPoints.size();
        int pointsNeeded = settingInterestPointsUsed *
                           (static_cast<double>(pointsTotal) / pointsInTriang);
        // std::cout << "in kf" << kfInd << "total = " << pointsTotal
        // << "inside triang = " << pointsInTriang << std::endl;
        // std::cout << "so we need " << pointsNeeded << std::endl;
        if (i != reselectCount)
          keyFrames[kfInd].selectPointsDenser(pointsNeeded);
      }
      keyFrames[kfInd].setDepthPyrs();
    }

    StdVector<std::pair<Vec2, double>> keyPairs(keyPoints[1].size());
    for (int i = 0; i < int(keyPoints[1].size()); ++i)
      keyPairs[i] = {keyPoints[1][i], depths[1][i]};
    std::sort(keyPairs.begin(), keyPairs.end(),
              [](auto a, auto b) { return a.second < b.second; });

    int paddingLeft = 0;
    int paddingRight = int(0.2 * keyPairs.size());
    minDepth = keyPairs[paddingLeft].second;
    maxDepth = keyPairs[paddingRight].second;

    if (debugOutputType != NO_DEBUG) {
      cv::Mat img = keyFrames[1].frameColored.clone();
      // KpTerrains[1].draw(img, CV_GREEN);

      insertDepths(img, keyPoints[1], depths[1], minDepth, maxDepth, true);

      // cv::circle(img, cv::Point(1268, 173), 7, CV_BLACK, 2);

      StdVector<InterestPoint> &ip = keyFrames[1].interestPoints;
      StdVector<Vec2> pnts(ip.size());
      std::vector<double> d(ip.size());
      for (int i = 0; i < int(ip.size()); ++i) {
        pnts[i] = ip[i].p;
        d[i] = ip[i].depth;
      }

      //    drawCurvedInternal(cam, Vec2(100.0, 100.0), Vec2(1000.0, 500.0),
      //    img,
      //                       CV_BLACK);

      // KpTerrains[1].drawCurved(cam, img, CV_GREEN);

      kpTerrains[1].draw(img, cam, CV_GREEN, minDepth, maxDepth);

      if (debugOutputType == SPARSE_DEPTHS) {
        insertDepths(img, pnts, d, minDepth, maxDepth, false);

        // for (auto ip : keyFrames[1].interestPoints)
        // kpTerrains[1].checkAllSectors(cam->unmap(ip.p.data()), cam, img);

      } else if (debugOutputType == FILLED_DEPTHS) {
        //        if (interpolationType == PLAIN)
        //          kpTerrains[1].drawDensePlainDepths(img, minDepth,
        //          maxDepth);
        // KpTerrains[1].draw(img, CV_BLACK);
      }

      cv::Mat tangImg = kpTerrains[1].drawTangentTri(800, 800);
      //      cv::imwrite("../../../../test/data/maps/badtri/frame505tangentTriang.jpg",
      //                  tangImg);

      //      kpTerrains[1].fillUncovered(img, cam, CV_BLACK);

      //      cv::Mat img3;
      //      cv::Mat maskb = stereoMatcher.getMask();
      //      cv::Mat mask;
      //      cv::cvtColor(maskb, mask, cv::COLOR_GRAY2BGR);
      //      cv::addWeighted(mask, 0.5, img, 0.5, 0, img3, img.depth());
      // cv::imshow("masked", img3);
      //      cv::imwrite("../../../../test/data/maps/badtri_removed/frame5Masked.jpg",
      //                  img3);

      // cv::imshow("first frame", keyFrames[0].frameColored);

      // cv::imwrite("../../../../test/data/maps/uncovered/frame5.jpg", img);
      // cv::imshow("tangent", tangImg);
      cv::imshow("interpolated", img);

      // int pyrDW = frames[1].cols / 2, pyrDH = frames[1].rows / 2;
      // for (int pi = 0; pi < settingPyrLevels; ++pi) {
      // cv::Mat pyrD =
      // keyFrames[1].preKeyFrame->drawDepthedFrame(pi, minDepth, maxDepth);
      // cv::Mat sizedPyrD;
      // cv::resize(pyrD, sizedPyrD, cv::Size(pyrDW, pyrDH), 0, 0,
      // cv::INTER_NEAREST);

      // cv::imshow("pyr " + std::to_string(pi) + " depths", sizedPyrD);

      // cv::Mat img2;
      // cv::resize(img, img2, cv::Size(), 0.5, 0.5);
      // }

      cv::waitKey();

      cv::destroyWindow("interpolated");
    }
  }

  // cv::imshow("f1", frames[0]);
  // cv::imshow("f2", frames[1]);
  // cv::waitKey();

  return keyFrames;
}

} // namespace fishdso