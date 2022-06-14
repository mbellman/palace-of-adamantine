#include "Gamma.h"

#include "movement_system.h"
#include "orientation_system.h"
#include "move_queue.h"
#include "easing_utilities.h"
#include "grid_utilities.h"
#include "game_state.h"
#include "game_macros.h"

using namespace Gamma;

#define abs(n) (n < 0.f ? -n : n)

static void movePlayer(args(), float dt) {
  auto& camera = getCamera();
  auto& from = state.currentMove.from;
  auto& to = state.currentMove.to;

  // Add dt to the easing alpha value to ensure that
  // the camera is always in motion while executing
  // movement. Since the 'from' position of the current
  // move represents where the camera was when the
  // move started, and since we run this routine on
  // the same frame as updating the current move,
  // we don't want to wait a frame to start moving
  // the camera, as this produces a jarring stutter.
  auto alpha = (getRunningTime() - state.currentMove.startTime) + dt;

  if (state.currentMove.easing == EasingType::EASE_IN_OUT) {
    alpha *= 3.f;
  } else if (state.currentMove.easing == EasingType::LINEAR) {
    alpha *= 4.f;
  } else if (state.currentMove.easing == EasingType::EASE_OUT) {
    alpha *= 2.f;
  }

  if (alpha >= 1.f) {
    camera.position = to;
    state.moving = false;
  } else {
    #define easeCamera(easingFn)\
      camera.position.x = easingFn(from.x, to.x, alpha);\
      camera.position.y = easingFn(from.y, to.y, alpha);\
      camera.position.z = easingFn(from.z, to.z, alpha);

    switch (state.currentMove.easing) {
      case EasingType::EASE_IN_OUT:
        easeCamera(easeInOut);
        break;
      case EasingType::LINEAR:
        easeCamera(Gm_Lerpf);
        break;
      case EasingType::EASE_OUT:
        easeCamera(easeOut);
        break;
    }
  }
}

static Vec3f worldDirectionToGridDirection(args(), const Vec3f& worldDirection) {
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

static MoveDirection getMoveDirectionFromKeyboardInput(args()) {
  auto& camera = getCamera();
  auto& input = getInput();
  auto forwardGridDirection = worldDirectionToGridDirection(params(), camera.orientation.getDirection());
  auto leftGridDirection = worldDirectionToGridDirection(params(), camera.orientation.getLeftDirection());
  auto move = MoveDirection::NONE;

  #define keyPressed(key) input.getLastKeyDown() == (uint64)key && input.isKeyHeld(key)

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

static void updateCurrentMoveAction(args()) {
  auto& camera = getCamera();
  auto runningTime = getRunningTime();
  auto nextMove = takeNextMove(state.moves);
  auto currentGridCoordinates = worldPositionToGridCoordinates(camera.position);
  auto targetCameraPosition = gridCoordinatesToWorldPosition(currentGridCoordinates);
  auto timeSinceLastMoveInput = runningTime - state.lastMoveInputTime;
  auto timeSinceCurrentMoveBegan = runningTime - state.currentMove.startTime;
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

  bool canMove = false;
  auto worldOrientation = state.worldOrientationState.worldOrientation;
  auto downGridCoordinates = getDownGridCoordinates(worldOrientation);
  auto upGridCoordinates = getUpGridCoordinates(worldOrientation);
  auto targetGridCoordinates = worldPositionToGridCoordinates(targetCameraPosition);
  auto* currentEntity = state.world.grid.get(currentGridCoordinates);
  auto* targetEntity = state.world.grid.get(targetGridCoordinates);
  auto* targetEntityAbove = state.world.grid.get(targetGridCoordinates + upGridCoordinates);
  auto* targetEntityBelow = state.world.grid.get(targetGridCoordinates + downGridCoordinates);

  // @todo create a static entity/movement handler
  #define getEntityType(entity) entity != nullptr && entity->type

  if (getEntityType(targetEntityBelow) == GROUND) {
    canMove = true;
  } else if (
    getEntityType(targetEntityBelow) == STAIRCASE_MOVER ||
    (getEntityType(currentEntity) == STAIRCASE_MOVER && getEntityType(targetEntityBelow) == GROUND)
  ) {
    canMove = true;
    targetCameraPosition += Vec3f(downGridCoordinates.x, downGridCoordinates.y, downGridCoordinates.z) * TILE_SIZE;      
  } else if (
    getEntityType(targetEntityAbove) == STAIRCASE_MOVER ||
    (getEntityType(currentEntity) == STAIRCASE_MOVER && getEntityType(targetEntity) == GROUND)
  ) {
    canMove = true;
    targetCameraPosition += Vec3f(upGridCoordinates.x, upGridCoordinates.y, upGridCoordinates.z) * TILE_SIZE;
  }

  if (!canMove) {
    return;
  }

  targetGridCoordinates = worldPositionToGridCoordinates(targetCameraPosition);

  // @todo create a trigger entity handler
  auto* targetTrigger = state.world.triggers.get(targetGridCoordinates);

  if (getEntityType(targetTrigger) == WORLD_ORIENTATION_CHANGE) {
    setWorldOrientation(params(), ((WorldOrientationChange*)targetTrigger)->targetWorldOrientation);
  }

  if (!state.moving || timeSinceCurrentMoveBegan > 0.4f) {
    // The move was either entered while standing still,
    // or after having sufficiently slowed down from a
    // prior move sequence. An in-out easing works best
    // for single tile moves or the beginning of a move
    // sequence, so we apply it in these cases.
    state.currentMove.easing = EasingType::EASE_IN_OUT;
  } else if (state.moves.size == 0) {
    // Last move in a sequence, so slow down and stop
    state.currentMove.easing = EasingType::EASE_OUT;
  } else if (state.moves.size >= 1) {
    // More moves ahead in the sequence, so move
    // at a constant rate
    state.currentMove.easing = EasingType::LINEAR;
  }

  state.moving = true;
  state.currentMove.startTime = runningTime;
  state.currentMove.from = camera.position;
  state.currentMove.to = targetCameraPosition;
  // @todo reduce tween time based on proximity to the target position
}

void handlePlayerMovement(args(), float dt) {
  auto& camera = getCamera();
  auto& input = getInput();
  auto runningTime = getRunningTime();
  auto move = getMoveDirectionFromKeyboardInput(params());
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
    updateCurrentMoveAction(params());
  }

  if (state.moving) {
    movePlayer(params(), dt);
  }
}