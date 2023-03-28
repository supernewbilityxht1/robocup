/*
 * @file KickEngine.cpp
 * This file implements a module that creates motions.
 * @author Judith Müller
 * @author Philip Reichenberg
 */

#include "KickEngine.h"
#include "KickEngineParameters.h"
#include "Platform/File.h"
#include "Representations/MotionControl/KickRequest.h"
#include "Representations/MotionControl/MotionInfo.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <filesystem>

MAKE_MODULE(KickEngine, motionControl);

KickEngine::KickEngine()
{
  params.reserve(10);

  try
  {
    for(const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(File::getBHDir()) / "Config" / "KickEngine"))
    {
      if(!entry.is_regular_file() || entry.path().extension() != ".kmc")
        continue;

      InMapFile stream(entry.path().string());
      ASSERT(stream.exists());

      KickEngineParameters parameters;
      stream >> parameters;

      strcpy(parameters.name, entry.path().stem().string().c_str());

      if(KickRequest::getKickMotionFromName(parameters.name) < KickRequest::newKick)
        params.push_back(parameters);
      else
      {
        OUTPUT_TEXT("Warning: KickRequest is missing the id for " << parameters.name);
        fprintf(stderr, "Warning: KickRequest is missing the id for %s \n", parameters.name);
      }
    }
  }
  catch(...)
  {
    FAIL("Could not open kick directory.");
  }

  for(int i = 0; i < KickRequest::newKick; ++i)
  {
    int id = -1;
    for(unsigned int p = 0; p < params.size(); ++p)
    {
      if(KickRequest::getKickMotionFromName(&params[p].name[0]) == i)
      {
        id = i;
        break;
      }
    }
    if(id == -1)
    {
      OUTPUT_TEXT("Warning: The kick motion file for id " << TypeRegistry::getEnumName(static_cast<KickRequest::KickMotionID>(i)) << " is missing.");
      fprintf(stderr, "Warning: The kick motion file for id %s is missing. \n", TypeRegistry::getEnumName(static_cast<KickRequest::KickMotionID>(i)));
    }
  }

  //This is needed for adding new kicks
  KickEngineParameters newKickMotion;
  strcpy(newKickMotion.name, "newKick");
  params.push_back(newKickMotion);
};

KickPhase::KickPhase(KickEngine& kickEngine, const KickRequest& kickRequest, const MotionPhase& lastPhase, const JointRequest& jointRequest) :
  MotionPhase(MotionPhase::kick),
  engine(kickEngine),
  startTime(kickEngine.theFrameInfo.time)
{
  if(kickRequest.kickMotionType != KickRequest::none)
    currentKickRequest = kickRequest;
  jointRequestOutput = previousRequest = jointRequest;
  if(lastPhase.type == MotionPhase::kick)
  {
    const auto& lastKickPhase = static_cast<const KickPhase&>(lastPhase);
    data.lastTrajetoryOffset = lastKickPhase.data.lastTrajetoryOffset;
  }
  else
    data.lastTrajetoryOffset.fill(0_deg);
  data.currentTrajetoryOffset.fill(0_deg);
}

void KickEngine::update(KickGenerator& kickGenerator)
{
  kickGenerator.createPhase = [this](const KickRequest& kickRequest, const MotionPhase& lastPhase)
  {
    return std::make_unique<KickPhase>(*this, kickRequest, lastPhase, theJointRequest);
  };

  auto& p = params.back();
  MODIFY("module:KickEngine:newKickMotion", p);
  strcpy(p.name, "newKick");
}

bool KickPhase::isDone(const MotionRequest& motionRequest) const
{
  // TODO for ballStopStart, it must be checked if the request changed! Otherwise ballStopEnd will be executed instantly
  // TODO: This is the "kick phase ends if the kick foot hits the ground at the end of the kick" feature. It did not work.
  /*
  if(currentKickRequest.kickMotionType == KickRequest::kickForwardFast && data.phaseNumber > 3 && engine.theFootSupport.switched)
    return true;
   */
  // kick finished or the kick was type none
  // to allow an abort for the kicks, the kick must be checked and the phaseNumber must be below the kicking phase (foot moves forward)
  bool isKickForward = currentKickRequest.kickMotionType == KickRequest::kickForwardFast || currentKickRequest.kickMotionType == KickRequest::kickForwardFastLong;
  const Legs::Leg legOfInterest = engine.theFootSupport.support > 0.f ? Legs::right : Legs::left;
  const bool earlyExitAllowed = (engine.theFootSupport.switched && data.phaseNumber >= data.currentParameters.keyframeEarlyExitAllowedSafe && data.currentParameters.keyframeEarlyExitAllowedSafe != -1// safe check
                                 && ((data.phaseNumber == data.currentParameters.keyframeEarlyExitAllowedSafe && data.phase > 0.8f && currentKickRequest.mirror == (legOfInterest == Legs::left)) || (data.phaseNumber > data.currentParameters.keyframeEarlyExitAllowedSafe))) || // make sure, we do not accept false switches
                                (data.phaseNumber >= data.currentParameters.keyframeEarlyExitAllowedRisky && data.currentParameters.keyframeEarlyExitAllowedRisky != -1 && data.phase > 0.2f && // risky check
                                 ((engine.theFsrData.legInfo[legOfInterest].hasPressure != engine.theFsrData.lastUpdateTimestamp && engine.theFsrData.legInfo[legOfInterest].hasPressure >= data.timestamp && engine.theFrameInfo.getTimeSince(engine.theFsrData.legInfo[legOfInterest].hasPressure) < 100) ||
                                  (engine.theFsrData.legInfo[legOfInterest].hasPressure == engine.theFsrData.lastUpdateTimestamp && engine.theFrameInfo.getTimeSince(engine.theFsrData.legInfo[legOfInterest].hasPressureSince) > 250)));
  return (earlyExitAllowed && (motionRequest.isWalking() || motionRequest.motion == MotionRequest::stand)) || // forwardFast kick and support switch happend
         (!isKickForward && data.phaseNumber == data.currentParameters.numberOfPhases && (data.currentParameters.numberOfPhases != 0 || currentKickRequest.kickMotionType == KickRequest::none)) || // no fowardFast kick and kick is over
         (isKickForward && data.phaseNumber == data.currentParameters.numberOfPhases && data.currentParameters.numberOfPhases != 0 && engine.theFrameInfo.getTimeSince(bothFeetGroundContactTimestamp) > engine.minFootGroundContactTime); // forwardFast kick and both feet have ground contact
}

void KickPhase::calcJoints(const MotionRequest&, JointRequest& jointRequest, Pose2f& odometryOffset, MotionInfo& motionInfo)
{
  jointRequest = jointRequestOutput;
  odometryOffset = data.odometryOutput;
  // while both feet are parallel to each other, the motion is still stable enough to allow perception stuff
  if((currentKickRequest.kickMotionType == KickRequest::kickForwardFast || currentKickRequest.kickMotionType == KickRequest::kickForwardFastLong) && data.phaseNumber < 2)
    motionInfo.isMotionStable = true;
  else
    motionInfo.isMotionStable = false;
}

std::unique_ptr<MotionPhase> KickPhase::createNextPhase(const MotionPhase&) const
{
  return std::unique_ptr<MotionPhase>();
}

unsigned KickPhase::freeLimbs() const
{
  return data.currentParameters.ignoreHead ? bit(MotionPhase::head) : 0;
}

void KickPhase::update()
{
  if(std::min(engine.theFsrData.legInfo[Legs::left].hasPressure, engine.theFsrData.legInfo[Legs::right].hasPressure) != engine.theFrameInfo.time)
    bothFeetGroundContactTimestamp = engine.theFrameInfo.time;
  data.robotModel = engine.theRobotModel;
  // interpolate into default position
  if(isInterpolating)
    interpolationState(jointRequestOutput, previousRequest, engine.interpolationSpeed);
  if(!isInterpolating)
  {
    //Do we need to wait before we can do a kick?
    if(data.sitOutTransitionDisturbance(compensate, compensated, engine.theInertialData, jointRequestOutput, previousRequest, engine.theFrameInfo))
    {
      if(data.activateNewMotion(currentKickRequest))
      {
        data.initData(engine.theFrameInfo, currentKickRequest, engine.params, engine.theJointAngles, jointRequestOutput, engine.theRobotDimensions, engine.theMassCalibration, engine.theDamageConfigurationBody);
        data.currentKickRequest = currentKickRequest;

        data.internalIsLeavingPossible = false;

        data.calcOdometryOffset(engine.theRobotModel); //to init it

        for(int i = Joints::lShoulderPitch; i < Joints::numOfJoints; ++i)
          jointRequestOutput.stiffnessData.stiffnesses[i] = 100;

        //kickEngineOutput.isStable = true;
      }
      //Is our Kick not over?
      if(data.checkPhaseTime(engine.theFrameInfo, engine.theJointAngles))
      {
        data.calcPhaseState();
        data.calcPositions();
        startTime = engine.theFrameInfo.time;
      }

      //Is the current kick id valid, then calculate the jointRequest once for the balanceCom()
      if(data.calcJoints(jointRequestOutput, engine.theRobotDimensions, engine.theDamageConfigurationBody))
      {
        data.balanceCOM(jointRequestOutput, engine.theRobotDimensions, engine.theMassCalibration);
        data.calcJoints(jointRequestOutput, engine.theRobotDimensions, engine.theDamageConfigurationBody);
        data.mirrorIfNecessary(jointRequestOutput);

        data.applyTrajetoryAdjustment(jointRequestOutput, engine.theJointLimits);
        data.calcOdometryOffset(engine.theRobotModel);
      }
      data.addGyroBalance(jointRequestOutput, engine.theJointLimits, engine.theInertialData);
    }
  }
  data.ModifyData(jointRequestOutput);
}

float KickPhase::calcInterpolationIntoStand(JointRequest& request, JointRequest& prevRequest, const Angle& interpolationSpeedFactor)
{
  // When an arm is behind, do an extra interpolation, to ensure that the arm will not get stuck when moving to the side
  if((engine.theJointAngles.angles[Joints::lShoulderRoll] < 0 || engine.theJointAngles.angles[Joints::rShoulderRoll] > 0) && engine.theFrameInfo.getTimeSince(startTime) < 100 && adjustStartArmPosition)
  {
    leftArmInterpolation = leftArmInterpolation || engine.theJointAngles.angles[Joints::lShoulderRoll] < 0;
    rightArmInterpolation = rightArmInterpolation || engine.theJointAngles.angles[Joints::rShoulderRoll] > 0;
    request.angles[Joints::lShoulderPitch] = prevRequest.angles[Joints::lShoulderPitch];
    request.angles[Joints::lShoulderRoll] = engine.theJointAngles.angles[Joints::lShoulderRoll] < 0 ? 20_deg : prevRequest.angles[Joints::lShoulderRoll];
    request.angles[Joints::lElbowYaw] = engine.theJointAngles.angles[Joints::lShoulderRoll] < 0 ? -18_deg : prevRequest.angles[Joints::lElbowYaw];
    request.angles[Joints::rShoulderPitch] = prevRequest.angles[Joints::rShoulderPitch];
    request.angles[Joints::rShoulderRoll] = engine.theJointAngles.angles[Joints::rShoulderRoll] > 0 ? -20_deg : prevRequest.angles[Joints::rShoulderRoll];
    request.angles[Joints::rElbowYaw] = engine.theJointAngles.angles[Joints::rShoulderRoll] < 0 ? 18_deg : prevRequest.angles[Joints::rElbowYaw];
    request.stiffnessData.stiffnesses[Joints::lShoulderRoll] = 20;
    request.stiffnessData.stiffnesses[Joints::lElbowYaw] = 20;
    request.stiffnessData.stiffnesses[Joints::rShoulderRoll] = 20;
    request.stiffnessData.stiffnesses[Joints::rElbowYaw] = 20;
  }
  else if(adjustStartArmPosition)
  {
    adjustStartArmPosition = false;
    startTime = engine.theFrameInfo.time;
    prevRequest.angles[Joints::lShoulderRoll] = leftArmInterpolation ? 20_deg : prevRequest.angles[Joints::lShoulderRoll];
    prevRequest.angles[Joints::lElbowYaw] = leftArmInterpolation ? -18_deg : prevRequest.angles[Joints::lElbowYaw];
    prevRequest.angles[Joints::rShoulderRoll] = rightArmInterpolation ? -20_deg : prevRequest.angles[Joints::rShoulderRoll];
    prevRequest.angles[Joints::rElbowYaw] = rightArmInterpolation ? 18_deg : prevRequest.angles[Joints::rElbowYaw];
  }
  JointRequest dif;
  // ignore joint that are not set
  FOREACH_ENUM(Joints::Joint, joint)
  {
    if(request.angles[joint] == JointAngles::off || request.angles[joint] == JointAngles::ignore)
      dif.angles[joint] = 0_deg;
    else
      dif.angles[joint] = prevRequest.angles[joint] - request.angles[joint];
  }
  Angle maxDiff;
  FOREACH_ENUM(Joints::Joint, joint)
    maxDiff = std::max(std::abs(dif.angles[joint]), std::abs(maxDiff)); // calc max difference
  Angle factorPerFrame = interpolationSpeedFactor / 1000.f; // factor per ms
  float interpolationTime = std::min(engine.maxInterpolationTime, maxDiff / factorPerFrame);

  float ratio = adjustStartArmPosition ? std::min(0.999f, engine.theFrameInfo.getTimeSince(startTime) / 100.f) : std::min(1.f, static_cast<float>(engine.theFrameInfo.getTimeSince(startTime)) / interpolationTime);
  ratio = 0.5f * std::sin(ratio * Constants::pi - Constants::pi / 2.f) + 0.5f;
  FOREACH_ENUM(Joints::Joint, joint)
  {
    if(request.angles[joint] == JointAngles::ignore)
      continue;
    float useRatio = adjustStartArmPosition && joint >= Joints::firstLegJoint ? 0.f : ratio;
    if(prevRequest.angles[joint] == JointAngles::off)
      request.angles[joint] = (engine.theJointRequest.stiffnessData.stiffnesses[joint] == 0 ? engine.theJointAngles.angles[joint] : engine.theJointRequest.angles[joint])
                              * (1 - useRatio) + request.angles[joint] * useRatio;
    else
      request.angles[joint] = prevRequest.angles[joint] * (1 - useRatio) + request.angles[joint] * useRatio;
  }
  return ratio;
}

void KickPhase::interpolationState(JointRequest& jointRequest, JointRequest& prevRequest, const Angle& interpolationSpeedFactor)
{
  MotionUtilities::walkStand(jointRequest, engine.theRobotDimensions); // for the legs
  float ratio = calcInterpolationIntoStand(jointRequest, previousRequest, interpolationSpeedFactor);
  data.startComp = false;
  compensated = true;
  compensate = true;
  data.sitOutTransitionDisturbance(compensate, compensated, engine.theInertialData, jointRequest, prevRequest, engine.theFrameInfo);
  if(ratio >= 1.f)
    isInterpolating = false;
}
