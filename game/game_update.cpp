#include "Gamma.h"

#include "game_update.h"
#include "movement_system.h"
#include "orientation_system.h"
#include "game_macros.h"

using namespace Gamma;

static void addDebugMessages(args()) {
  auto& camera = getCamera();
  auto& coordinates = worldPositionToGridCoordinates(camera.position);

  std::string positionLabel = "Grid position: "
    + std::to_string(coordinates.x) + ","
    + std::to_string(coordinates.y) + ","
    + std::to_string(coordinates.z);

  std::string orientationLabel = std::string("Orientation: ")
    + std::to_string(camera.orientation.pitch) + " (pitch), "
    + std::to_string(camera.orientation.yaw) + " (yaw), "
    + std::to_string(camera.orientation.roll) + " (roll)";

  addDebugMessage(positionLabel);
  addDebugMessage(orientationLabel);
}

void updateGame(args(), float dt) {
  // handlePlayerMovement(params(), dt);
  Gm_HandleFreeCameraMode(context, dt);
  handleWorldOrientation(params(), dt);

  addDebugMessages(params());
}