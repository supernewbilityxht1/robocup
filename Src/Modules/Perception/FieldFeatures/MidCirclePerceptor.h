/**
 * @file MidCirclePerceptor.h
 * Provides MidCircle.
 * @author <a href="mailto:jesse@tzi.de">Jesse Richter-Klug</a>
 */

#pragma once

#include "Representations/Configuration/FieldDimensions.h"
#include "Representations/Infrastructure/CameraInfo.h"
#include "Representations/Infrastructure/FrameInfo.h"
#include "Representations/Infrastructure/GameState.h"
#include "Representations/Infrastructure/JointAngles.h"
#include "Representations/Modeling/Odometer.h"
#include "Representations/Perception/FieldFeatures/MidCircle.h"
#include "Representations/Perception/FieldPercepts/CirclePercept.h"
#include "Representations/Perception/FieldPercepts/FieldLineIntersections.h"
#include "Representations/Perception/FieldPercepts/FieldLines.h"
#include "Representations/Perception/ImagePreprocessing/CameraMatrix.h"
#include "Framework/Module.h"

MODULE(MidCirclePerceptor,
{,
  REQUIRES(JointAngles),
  REQUIRES(FrameInfo),
  REQUIRES(FieldLineIntersections),
  REQUIRES(FieldLines),
  USES(GameState),
  REQUIRES(CameraInfo),
  REQUIRES(CameraMatrix),
  REQUIRES(CirclePercept),
  REQUIRES(Odometer),
  REQUIRES(FieldDimensions),
  PROVIDES(MidCircle),
  DEFINES_PARAMETERS(
  {,
    (int)(30) maxTimeOffset,
    (float)(150) maxLineDistanceToCircleCenter,
    (float)(-500) allowedOffsetOfMidLineEndToCircleCenter,
    (float)(sqr(200)) squaredMinLineLength,
  }),
});

class MidCirclePerceptor : public MidCirclePerceptorBase
{
  void update(MidCircle& midCircle) override;
private:
  unsigned int lastFrameTime = 1;
  CirclePercept theLastCirclePercept;

  bool searchCircleWithLine(MidCircle& midCircle) const;
};
