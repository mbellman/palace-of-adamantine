#include "Gamma.h"

#include "movement_system.h"
#include "orientation_system.h"
#include "move_queue.h"
#include "easing_utilities.h"
#include "grid_utilities.h"
#include "game_state.h"
#include "game_macros.h"
#include "build_flags.h"

using namespace Gamma;

#define abs(n) (n < 0.f ? -n : n)
#define typeOfEntity(entity) entity != nullptr && entity->type

static inline bool isMoving(Globals) {
  return state.currentMove.startTime != 0.f;
}

static Vec3f worldDirectionToGridDirection(Globals, const Vec3f& worldDirection) {
  Vec3f movementPlaneAlignedWorldDirection = worldDirection * state.worldOrientationState.movementPlane;

  auto absX = abs(movementPlaneAlignedWorldDirection.x);
  auto absY = abs(movementPlaneAlignedWorldDirection.y);
  auto absZ = abs(movementPlaneAlignedWorldDirection.z);

  if (absX > absY && absX > absZ) {
    return Vec3f(worldDirection.x, 0, 0).unit();
  } else if (absY > absX && absY > absZ) {
    return Vec3f(0, worldDirection.y, 0).unit();
  } else {
    return Vec3f(0, 0, worldDirection.z).unit();
  }
}

static MoveDirection gridDirectionToMoveDirection(const Vec3f& gridDirection) {
  if (gridDirection.x > 0.f) {
    return MoveDirection::X_RIGHT;
  } else if (gridDirection.x < 0.f) {
    return MoveDirection::X_LEFT;
  } else if (gridDirection.z > 0.f) {
    return MoveDirection::Z_FORWARD;
  } else if (gridDirection.z < 0.f) {
    return MoveDirection::Z_BACKWARD;
  } else if (gridDirection.y > 0.f) {
    return MoveDirection::Y_UP;
  } else if (gridDirection.y < 0.f) {
    return MoveDirection::Y_DOWN;
  }

  return MoveDirection::NONE;
}

static MoveDirection getMoveDirectionFromKeyboardInput(Globals) {
  auto& camera = getCamera();
  auto& input = getInput();
  auto forwardGridDirection = worldDirectionToGridDirection(globals, camera.orientation.getDirection());
  auto leftGridDirection = worldDirectionToGridDirection(globals, camera.orientation.getLeftDirection());
  auto move = MoveDirection::NONE;

  #define keyPressed(key) input.getLastKeyDown() == (u64)key && input.isKeyHeld(key)

  if (keyPressed(Key::W)) {
    move = gridDirectionToMoveDirection(forwardGridDirection);
  } else if (keyPressed(Key::S)) {
    move = gridDirectionToMoveDirection(forwardGridDirection.invert());
  } else if (keyPressed(Key::A)) {
    move = gridDirectionToMoveDirection(leftGridDirection);
  } else if (keyPressed(Key::D)) {
    move = gridDirectionToMoveDirection(leftGridDirection.invert());
  }

  return move;
}

static bool isNextMoveValid(Globals, const GridCoordinates& currentGridCoordinates, Vec3f& targetCameraPosition) {
  auto& grid = state.world.grid;
  auto worldOrientation = state.worldOrientationState.worldOrientation;
  auto downGridCoordinates = getDownGridCoordinates(worldOrientation);
  auto upGridCoordinates = getUpGridCoordinates(worldOrientation);
  auto targetGridCoordinates = worldPositionToGridCoordinates(targetCameraPosition);
  auto targetBelowCoordinates = targetGridCoordinates + downGridCoordinates;
  auto* currentTileBelow = grid.get(currentGridCoordinates + downGridCoordinates);
  auto* targetTile = grid.get(targetGridCoordinates);
  auto* targetTileAbove = grid.get(targetGridCoordinates + upGridCoordinates);
  auto* targetTileBelow = grid.get(targetBelowCoordinates);
  auto* targetTileTwoBelow = grid.get(targetBelowCoordinates + downGridCoordinates);

  if (
    // Walking up a staircase
    (
      typeOfEntity(currentTileBelow) == STAIRCASE &&
      typeOfEntity(targetTile) == STAIRCASE
    ) ||
    // Exiting off of an upward staircase
    (
      typeOfEntity(currentTileBelow) == STAIRCASE &&
      typeOfEntity(targetTileBelow) == GROUND &&
      targetTile == nullptr
    )
  ) {
    targetCameraPosition += Vec3f(upGridCoordinates.x, upGridCoordinates.y, upGridCoordinates.z) * TILE_SIZE;

    return true;
  } else if (
    // Entering onto a downward staircase
    typeOfEntity(targetTileTwoBelow) == STAIRCASE ||
    // Walking down a staircase
    (
      typeOfEntity(currentTileBelow) == STAIRCASE &&
      typeOfEntity(targetTileTwoBelow) == STAIRCASE ||
      // Allow world orientation changes placed at the end
      // of a downward staircase to be navigated to, even
      // if the tile isn't technically on a staircase in
      // the current world orientation.
      typeOfEntity(targetTileBelow) == WORLD_ORIENTATION_CHANGE
    )
  ) {
    targetCameraPosition += Vec3f(downGridCoordinates.x, downGridCoordinates.y, downGridCoordinates.z) * TILE_SIZE;

    return true;
  } else if (
    // Allow world orientation changes to be moved into,
    // operating on the assumption that the changed orientation
    // will result in a valid standing position
    typeOfEntity(targetTile) == WORLD_ORIENTATION_CHANGE
  ) {
    return true;
  } else if (
    // Walking on a regular ground tile
    typeOfEntity(targetTileTwoBelow) == GROUND &&
    // Ground tiles are only walkable if the tile
    // immediately below the camera is empty or
    // passable, and the target tile is non-solid
    (
      (
        targetTileBelow == nullptr ||
        targetTileBelow->type == STAIRCASE ||
        targetTileBelow->type == SWITCH
      ) &&
      (
        targetTile == nullptr ||
        targetTile->type == TELEPORTER
      )
    )
  ) {
    if (typeOfEntity(targetTileBelow) == SWITCH) {
      targetCameraPosition += Vec3f(upGridCoordinates.x, upGridCoordinates.y, upGridCoordinates.z) * TILE_SIZE * 0.25f;
    }

    return true;
  }

  return false;
}

static Vec3f getImmediateTeleportationPosition(Globals, const Teleporter* teleporter, const Vec3f& teleporterPosition) {
  auto& camera = getCamera();
  auto toPosition = gridCoordinatesToWorldPosition(teleporter->toCoordinates);
  auto teleporterToCameraDistance = (camera.position - teleporterPosition).magnitude();
  auto direction = Vec3f(1, 0, 0);

  const static std::initializer_list<GridCoordinates> offsets = {
    { 1, 0, 0 },
    { -1, 0, 0 },
    { 0, 1, 0 },
    { 0, -1, 0 },
    { 0, 0, 1 },
    { 0, 0, -1 }
  };

  for (auto& offset : offsets) {
    auto tile = teleporter->toCoordinates + offset;
    auto* targetEntity = state.world.grid.get(tile);

    if (typeOfEntity(targetEntity) == TELEPORTER) {
      direction.x = (float)offset.x;
      direction.y = (float)offset.y;
      direction.z = (float)offset.z;

      break;
    }
  }

  return toPosition + direction * (teleporterToCameraDistance + TILE_SIZE);
}

static void handleTriggerEntitiesBeforeMove(Globals, const GridCoordinates& targetGridCoordinates, Vec3f& targetCameraPosition) {
  auto* targetEntity = state.world.grid.get(targetGridCoordinates);

  if (targetEntity == nullptr) {
    return;
  }

  switch (targetEntity->type) {
    case WORLD_ORIENTATION_CHANGE:
      setWorldOrientation(globals, ((WorldOrientationChange*)targetEntity)->targetWorldOrientation);
      break;
    case TELEPORTER: {
      auto& camera = getCamera();
      auto* teleporter = (Teleporter*)targetEntity;

      camera.position = getImmediateTeleportationPosition(globals, teleporter, targetCameraPosition);
      targetCameraPosition = gridCoordinatesToWorldPosition(teleporter->toCoordinates);

      immediatelySetWorldOrientation(globals, teleporter->toOrientation);
      resetMoveQueue(state.moves);

      // Teleporters are designed to transport the camera from one
      // world position and orientation to a different one in a seamless
      // fashion. Both ends of a teleporter may have identical layouts
      // to ensure visual coherence, but the camera view matrix will
      // differ dramatically between the two points and orientations.
      // Thus, we use stable temporal sampling (for SSAO/SSGI) on this
      // frame to prevent erroneous temporal sampling artifacts.
      context->renderer->frameFlags.useStableTemporalSampling = true;

      break;
    }
  }
}

static void handleNextMove(Globals) {
  auto& camera = getCamera();
  auto runningTime = getRunningTime();
  auto nextMove = takeNextMove(state.moves);
  auto& currentMove = state.currentMove;
  auto currentGridCoordinates = worldPositionToGridCoordinates(camera.position);
  // This will be mutated in the process of handling the move
  auto targetCameraPosition = gridCoordinatesToWorldPosition(currentGridCoordinates);
  auto timeSinceLastMoveInput = runningTime - state.lastMoveInputTime;
  auto timeSinceCurrentMoveBegan = runningTime - currentMove.startTime;
  Vec3f moveDelta = Vec3f(0);

  // Only advance the target world position along the
  // next move direction if the last move input was
  // sufficiently recent in time. This slightly reduces
  // the incidence of overshot.
  if (timeSinceLastMoveInput < 0.15f) {
    switch (nextMove) {
      case Z_FORWARD:
        moveDelta.z += TILE_SIZE;
        break;
      case Z_BACKWARD:
        moveDelta.z -= TILE_SIZE;
        break;
      case X_LEFT:
        moveDelta.x -= TILE_SIZE;
        break;
      case X_RIGHT:
        moveDelta.x += TILE_SIZE;
        break;
      case Y_UP:
        moveDelta.y += TILE_SIZE;
        break;
      case Y_DOWN:
        moveDelta.y -= TILE_SIZE;
        break;
    }
  }

  // Ensure that moves queued just before a world orientation change
  // don't produce movement off the new world orientation's movement plane
  moveDelta *= state.worldOrientationState.movementPlane;
  targetCameraPosition += moveDelta;

  if (!isNextMoveValid(globals, currentGridCoordinates, targetCameraPosition)) {
    // @todo animate the camera slightly toward the target and back to indicate that we cannot move here
    return;
  }

  auto& targetGridCoordinates = worldPositionToGridCoordinates(targetCameraPosition);

  handleTriggerEntitiesBeforeMove(globals, targetGridCoordinates, targetCameraPosition);

  if (!isMoving(globals) || timeSinceCurrentMoveBegan > 0.4f) {
    // The move was either entered while standing still,
    // or after having sufficiently slowed down from a
    // prior move sequence. An in-out easing works best
    // for single tile moves or the beginning of a move
    // sequence, so we apply it in these cases.
    currentMove.easing = EasingType::EASE_IN_OUT;
  } else if (state.moves.size == 0) {
    // Last move in a sequence, so slow down and stop
    currentMove.easing = EasingType::EASE_OUT;
  } else if (state.moves.size >= 1) {
    // More moves ahead in the sequence, so move
    // at a constant rate
    currentMove.easing = EasingType::LINEAR;
  }

  currentMove.startTime = runningTime;
  currentMove.from = camera.position;
  currentMove.to = targetCameraPosition;

  // Determine the current move duration as a function of
  // how far the move travels. Longer moves should take
  // closer to a full second, and shorter moves should
  // complete sooner.
  float moveDistanceRatio = (currentMove.from - currentMove.to).magnitude() / TILE_SIZE;

  currentMove.duration = powf(moveDistanceRatio, 1.f / 3.f);
}

static void movePlayerCamera(Globals, float dt) {
  auto& camera = getCamera();
  auto& move = state.currentMove;
  auto& from = move.from;
  auto& to = move.to;

  // Add dt to the easing alpha value to ensure that
  // the camera is always in motion while executing
  // movement. Since the 'from' position of the current
  // move represents where the camera was when the
  // move started, and since we run this routine on
  // the same frame as updating the current move,
  // we don't want to wait a frame to start moving
  // the camera, as this produces a jarring stutter.
  auto alpha = ((getRunningTime() - move.startTime) + dt) / move.duration;

  if (move.easing == EasingType::EASE_IN_OUT) {
    // Initial movement action
    alpha *= 3.f;
  } else if (move.easing == EasingType::LINEAR) {
    // Continuous movement action
    alpha *= 4.f;
  } else if (move.easing == EasingType::EASE_OUT) {
    // Slowdown movement action
    alpha *= 2.f;
  }

  if (alpha >= 1.f) {
    camera.position = to;
    move.startTime = 0.f;
  } else {
    switch (move.easing) {
      case EasingType::EASE_IN_OUT:
        camera.position = Vec3f::lerp(from, to, easeInOut(0.f, 1.f, alpha));
        break;
      case EasingType::LINEAR: {
        auto deltaX = Gm_Absf(to.x - from.x);
        auto deltaY = Gm_Absf(to.y - from.y);
        auto deltaZ = Gm_Absf(to.z - from.z);

        // Interpolate the position components independently.
        // We want 90-degree changes in direction to swivel
        // the camera more naturally, rather than instantly
        // beginning a lerp to the new target coordinate.
        // We use linear interpolation for the dominant
        // component and a gradual ease-out for the less
        // dominant component, imparting more of a curving
        // motion to the movement.
        if (deltaX > deltaY && deltaX > deltaZ) {
          camera.position.x = Gm_Lerpf(from.x, to.x, alpha);
          camera.position.y = easeOut(from.y, to.y, alpha);
          camera.position.z = easeOut(from.z, to.z, alpha);
        } else if (deltaY > deltaX && deltaY > deltaZ) {
          camera.position.y = Gm_Lerpf(from.y, to.y, alpha);
          camera.position.x = easeOut(from.x, to.x, alpha);
          camera.position.z = easeOut(from.z, to.z, alpha);
        } else {
          camera.position.z = Gm_Lerpf(from.z, to.z, alpha);
          camera.position.x = easeOut(from.x, to.x, alpha);
          camera.position.y = easeOut(from.y, to.y, alpha);
        }

        break;
      }
      case EasingType::EASE_OUT:
        camera.position = Vec3f::lerp(from, to, easeOut(0.f, 1.f, alpha));
        break;
    }
  }
}

void handlePlayerCameraMovementOnUpdate(Globals, float dt) {
  auto& camera = getCamera();
  auto& input = getInput();
  auto runningTime = getRunningTime();
  auto move = getMoveDirectionFromKeyboardInput(globals);
  auto timeSinceCurrentMoveBegan = runningTime - state.currentMove.startTime;
  auto commitMoveTimeDelay = checkNextMove(state.moves) == move ? 0.175f : 0.1f;

  if (move != MoveDirection::NONE) {
    state.lastMoveInputTime = runningTime;
  }

  if (
    move != MoveDirection::NONE &&
    timeSinceCurrentMoveBegan > commitMoveTimeDelay &&
    checkNextMove(state.moves, 1) != move
  ) {
    commitMove(state.moves, move);
  }

  if (
    state.moves.size > 0 &&
    timeSinceCurrentMoveBegan > 0.2f
  ) {
    handleNextMove(globals);
  }

  if (isMoving(globals)) {
    movePlayerCamera(globals, dt);
  }
}