/**
 * @file OdometryDataPreviewProvider
 * This file implements the provider for the preview of the odometryData
 */

#include "OdometryDataPreviewProvider.h"
#include "Math/Rotation.h"

MAKE_MODULE(OdometryDataPreviewProvider, sensing);

void OdometryDataPreviewProvider::update(OdometryDataPreview& odometryDataPreview)
{
  static_cast<OdometryData&>(odometryDataPreview) = theOdometryData;
  Pose2f odometryOffset;

  if(!theMotionInfo.isMotion(MotionPhase::walk))
    odometryOffset = Pose2f(0_deg, 0.f, 0.f);
  else
  {
    bool useIsLeftPhase = theMotionInfo.walkPhaseIsLeftPhase;
    if(theFrameInfo.getTimeSince(theMotionInfo.lastMotionPhaseStarted) < useMeasuredSwingAfterStepStartedTime
       && theFootSupport.lastSwitch < usePlannedIsLeftPhaseAfterThisSwitchTime)
      useIsLeftPhase = theFootSupport.support < 0.f;
    const Pose3f currentLeftSole3D = theTorsoMatrix * theRobotModel.soleLeft;
    const Pose3f currentRightSole3D = theTorsoMatrix * theRobotModel.soleRight;
    const Pose2f currentSoleLeft(currentLeftSole3D.rotation.getZAngle(), currentLeftSole3D.translation.head<2>());
    const Pose2f currentSoleRight(currentRightSole3D.rotation.getZAngle(), currentRightSole3D.translation.head<2>());

    const Pose2f& lastSupportFoot = useIsLeftPhase ? lastSoleRight : lastSoleLeft;
    const Pose2f& lastSwingFoot = useIsLeftPhase ? lastSoleLeft : lastSoleRight;

    const Pose2f& currentSupportFoot = useIsLeftPhase ? currentSoleRight : currentSoleLeft;
    const Pose2f& currentSwingFoot = useIsLeftPhase ? currentSoleLeft : currentSoleRight;

    const Pose2f lastRef = lastSupportFoot.inverse() * lastSwingFoot;
    const Pose2f currentRef = currentSupportFoot.inverse() * currentSwingFoot;

    odometryOffset = lastRef.inverse() * currentRef;
    odometryOffset.translation *= 0.5f;
    odometryOffset.rotation *= 0.5f;
    odometryOffset.translation.rotate(odometryOffset.rotation);

    odometryOffset.translation.x() *= odometryWalkScaling.x();
    odometryOffset.translation.y() *= odometryWalkScaling.y();

    lastSoleLeft = currentSoleLeft;
    lastSoleRight = currentSoleRight;
  }
  // Construct odometry update for this frame.
  if(theFallDownState.state == FallDownState::falling || theFallDownState.state == FallDownState::fallen)
    odometryOffset.rotation = 0_deg; // Postpone rotation change until being upright again to avoid huge variances in the self locator.
  else
    odometryOffset.rotation = Angle::normalize(Rotation::Euler::getZAngle(theInertialData.orientation3D) - odometryDataPreview.rotation);

  odometryDataPreview += odometryOffset;
  odometryDataPreview.odometryChange = odometryOffset;
}
