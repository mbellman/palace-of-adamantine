#include <map>

#include "orientation_system.h"
#include "easing_utilities.h"
#include "game_state.h"
#include "game_macros.h"

using namespace Gamma;

typedef std::tuple<WorldOrientation, WorldOrientation> OrientationTransition;
typedef std::function<void(Orientation&, Orientation&)> OrientationHandler;

#define transition() [](Orientation& to, Orientation& view)

static void defaultOrientationHandler(Orientation& to, Orientation& view) {}

static void rollFromYaw(Orientation& to, Orientation& view) {
  to.roll = view.yaw;
}

static void rollFromInverseYaw(Orientation& to, Orientation& view) {
  to.roll = -view.yaw;
}

static void yawFromYaw(Orientation& to, Orientation& view) {
  to.yaw = view.yaw;
}

static void yawFromRoll(Orientation& to, Orientation& view) {
  to.yaw = view.roll;
}

static void yawFromInverseRoll(Orientation& to, Orientation& view) {
  to.yaw = -view.roll;
}

// @todo allow reverse mappings to be implicit
const static std::map<OrientationTransition, OrientationHandler> orientationHandlerMap = {
  // +Y Up ->
  { {POSITIVE_Y_UP, POSITIVE_Z_UP}, rollFromInverseYaw },
  { {POSITIVE_Y_UP, NEGATIVE_Z_UP}, rollFromYaw },
  { {POSITIVE_Y_UP, POSITIVE_X_UP}, yawFromYaw },
  { {POSITIVE_Y_UP, NEGATIVE_X_UP}, yawFromYaw },

  // -Y Up ->
  { {NEGATIVE_Y_UP, POSITIVE_Z_UP}, rollFromYaw },
  { {NEGATIVE_Y_UP, NEGATIVE_Z_UP}, rollFromInverseYaw },
  { {NEGATIVE_Y_UP, POSITIVE_X_UP}, transition() {
    to.yaw = -view.yaw + Gm_PI;
  }},
  { {NEGATIVE_Y_UP, NEGATIVE_X_UP}, transition() {
    to.yaw = -view.yaw + Gm_PI;
  }},

  // +X Up ->
  { {POSITIVE_X_UP, POSITIVE_Y_UP}, yawFromYaw },
  { {POSITIVE_X_UP, NEGATIVE_Y_UP}, transition() {
    to.yaw = -view.yaw + Gm_PI;
  }},
  { {POSITIVE_X_UP, POSITIVE_Z_UP}, transition() {
    to.roll = -view.yaw + Gm_PI / 2.f;
  }},
  { {POSITIVE_X_UP, NEGATIVE_Z_UP}, transition() {
    to.roll = view.yaw + Gm_PI / 2.f;
  }},

  // -X Up ->
  { {NEGATIVE_X_UP, POSITIVE_Y_UP}, yawFromYaw },
  { {NEGATIVE_X_UP, NEGATIVE_Y_UP}, transition() {
    to.yaw = -view.yaw + Gm_PI;
  }},
  { {NEGATIVE_X_UP, POSITIVE_Z_UP}, transition() {
    to.roll = -view.yaw - Gm_PI / 2.f;
  }},
  { {NEGATIVE_X_UP, NEGATIVE_Z_UP}, transition() {
    to.roll = view.yaw - Gm_PI / 2.f;
  }},

  // +Z Up ->
  { {POSITIVE_Z_UP, POSITIVE_Y_UP}, yawFromInverseRoll },
  { {POSITIVE_Z_UP, NEGATIVE_Y_UP}, yawFromRoll },
  { {POSITIVE_Z_UP, POSITIVE_X_UP}, transition() {
    to.yaw = -view.roll + Gm_PI / 2.f;
  }},
  { {POSITIVE_Z_UP, NEGATIVE_X_UP}, transition() {
    to.yaw = -view.roll - Gm_PI / 2.f;
  }},

  // -Z Up ->
  { {NEGATIVE_Z_UP, POSITIVE_Y_UP}, yawFromRoll },
  { {NEGATIVE_Z_UP, NEGATIVE_Y_UP}, yawFromInverseRoll },
  { {NEGATIVE_Z_UP, POSITIVE_X_UP}, transition() {
    to.yaw = view.roll - Gm_PI / 2.f;
  }},
  { {NEGATIVE_Z_UP, NEGATIVE_X_UP}, transition() {
    to.yaw = view.roll + Gm_PI / 2.f;
  }}
};

void updateCameraFromMouseMoveEvent(Globals, const MouseMoveEvent& event) {
  auto& camera = getCamera();
  auto mDeltaY = event.deltaY / 1000.f;
  auto mDeltaX = event.deltaX / 1000.f;
  auto isFreeCameraModeEnabled = Gm_IsFlagEnabled(FREE_CAMERA_MODE);

  auto& orientation = isFreeCameraModeEnabled
    // Update the camera orientation directly in free camera mode
    ? camera.orientation
    // Update the target orientation otherwise, so we can animate to it
    // (@see game_update -> animateCameraToTargetOrientation)
    : state.cameraState.orientationTo;

  switch (state.worldOrientationState.worldOrientation) {
    case POSITIVE_Y_UP:
    case POSITIVE_X_UP:
    case NEGATIVE_X_UP:
      orientation.pitch += mDeltaY;
      orientation.yaw += mDeltaX;
      break;
    case NEGATIVE_Y_UP:
      orientation.pitch += mDeltaY;
      orientation.yaw -= mDeltaX;
      break;
    case POSITIVE_Z_UP:
      orientation.pitch += mDeltaY;
      orientation.roll -= mDeltaX;
      break;
    case NEGATIVE_Z_UP:
      orientation.pitch += mDeltaY;
      orientation.roll += mDeltaX;
      break;
  }

  // Wrap a value to within the [0, TAU] range
  #define wrap(n) n = n > Gm_TAU ? n - Gm_TAU : n < 0.f ? n + Gm_TAU : n

  wrap(orientation.pitch);
  wrap(orientation.yaw);
  wrap(orientation.roll);

  if (isFreeCameraModeEnabled) {
    // In free camera mode, set the target orientation
    // to the immediate camera orientation to avoid a
    // mismatch when reverting to standard camera mode
    state.cameraState.orientationTo = camera.orientation;
  }
}

void setWorldOrientation(Globals, WorldOrientation targetWorldOrientation) {
  auto& camera = getCamera();
  auto currentWorldOrientation = state.worldOrientationState.worldOrientation;

  if (targetWorldOrientation == currentWorldOrientation) {
    return;
  }

  Orientation to;
  auto orientationTransition = std::pair(currentWorldOrientation, targetWorldOrientation);

  auto orientationHandler =
    orientationHandlerMap.find(orientationTransition) != orientationHandlerMap.end()
      ? orientationHandlerMap.at(orientationTransition)
      : defaultOrientationHandler;

  switch (targetWorldOrientation) {
    case POSITIVE_Y_UP: {
      to.pitch = 0.f;
      to.roll = 0.f;

      state.worldOrientationState.movementPlane = Vec3f(1.f, 0, 1.f);
      break;
    }
    case NEGATIVE_Y_UP: {
      to.pitch = Gm_PI;
      to.roll = 0.f;

      state.worldOrientationState.movementPlane = Vec3f(1.f, 0, 1.f);
      break;
    }
    case POSITIVE_Z_UP: {
      to.pitch = Gm_PI / 2.f;
      to.yaw = 0.f;

      state.worldOrientationState.movementPlane = Vec3f(1.f, 1.f, 0);
      break;
    }
    case NEGATIVE_Z_UP: {
      to.pitch = Gm_PI + Gm_PI / 2.f;
      to.yaw = 0.f;

      state.worldOrientationState.movementPlane = Vec3f(1.f, 1.f, 0);
      break;
    }
    case POSITIVE_X_UP: {
      to.pitch = 0.f;
      to.roll = Gm_PI / 2.f;

      state.worldOrientationState.movementPlane = Vec3f(0, 1.f, 1.f);
      break;
    }
    case NEGATIVE_X_UP: {
      to.pitch = 0.f;
      to.roll = -Gm_PI / 2.f;

      state.worldOrientationState.movementPlane = Vec3f(0, 1.f, 1.f);
      break;
    }
  }

  orientationHandler(to, camera.orientation);

  state.worldOrientationState.worldOrientation = targetWorldOrientation;

  state.cameraState.tweenStartTime = getRunningTime();
  state.cameraState.orientationFrom = camera.orientation;
  state.cameraState.orientationTo = to;

  // Pre-emptively set the new camera orientation,
  // preventing movement bugs during the orientation
  // transition. The camera's rotation quaternion will
  // represent the apparent orientation in the meantime.
  camera.orientation = state.cameraState.orientationTo;
}

void immediatelySetWorldOrientation(Globals, WorldOrientation worldOrientation) {
  auto& camera = getCamera();
  float pitch = camera.orientation.pitch;

  setWorldOrientation(globals, worldOrientation);

  // @todo flesh this out and determine the proper conversions between orientations
  switch (worldOrientation) {
    case NEGATIVE_X_UP:
      camera.orientation.pitch = pitch + Gm_HALF_PI;
      break;
    case NEGATIVE_Z_UP:
      camera.orientation.pitch = pitch + Gm_PI * 1.5f;
      break;
  }

  state.cameraState.tweenStartTime = 0.f;
  state.cameraState.orientationTo = camera.orientation;
}

void handleCameraOrientationOnUpdate(Globals, float dt) {
  auto& camera = getCamera();
  auto& orientationFrom = state.cameraState.orientationFrom;
  auto& orientationTo = state.cameraState.orientationTo;
  auto alpha = getRunningTime() - state.cameraState.tweenStartTime;

  // @todo change easing time based on rotation proximity
  alpha *= 2.f;

  if (state.cameraState.tweenStartTime == 0.f) {
    // @todo ensure that this doesn't require explicit user action
    camera.rotation = camera.orientation.toQuaternion();

    return;
  }

  if (alpha >= 1.f) {
    camera.orientation = orientationTo;
    state.cameraState.tweenStartTime = 0.f;

    return;
  }

  // @bug this breaks light disc projection, causing lights to occasionally
  // disappear during the world orientation transition
  camera.rotation = Quaternion::slerp(
    orientationFrom.toQuaternion(),
    orientationTo.toQuaternion(),
    easeInOut(0.f, 1.f, alpha)
  );
}

GridCoordinates getUpGridCoordinates(WorldOrientation worldOrientation) {
  switch (worldOrientation) {
    default:
    case POSITIVE_Y_UP:
      return GridCoordinates(0, 1, 0);
    case NEGATIVE_Y_UP:
      return GridCoordinates(0, -1, 0);
    case POSITIVE_Z_UP:
      return GridCoordinates(0, 0, 1);
    case NEGATIVE_Z_UP:
      return GridCoordinates(0, 0, -1);
    case POSITIVE_X_UP:
      return GridCoordinates(1, 0, 0);
    case NEGATIVE_X_UP:
      return GridCoordinates(-1, 0, 0);
  }
}

GridCoordinates getDownGridCoordinates(WorldOrientation worldOrientation) {
  switch (worldOrientation) {
    default:
    case POSITIVE_Y_UP:
      return GridCoordinates(0, -1, 0);
    case NEGATIVE_Y_UP:
      return GridCoordinates(0, 1, 0);
    case POSITIVE_Z_UP:
      return GridCoordinates(0, 0, -1);
    case NEGATIVE_Z_UP:
      return GridCoordinates(0, 0, 1);
    case POSITIVE_X_UP:
      return GridCoordinates(-1, 0, 0);
    case NEGATIVE_X_UP:
      return GridCoordinates(1, 0, 0);
  }
}