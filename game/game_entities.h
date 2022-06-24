#pragma once

#include "grid_utilities.h"
#include "orientation_system.h"

/**
 * Static entities
 * ---------------
 */
enum StaticEntityType {
  GROUND,
  STAIRCASE
};

struct StaticEntity {
  StaticEntityType type;

  StaticEntity(StaticEntityType type): type(type) {};
  virtual ~StaticEntity() = default;
};

struct Ground : StaticEntity {
  Ground(): StaticEntity(GROUND) {};
};

struct Staircase : StaticEntity {
  Staircase(): StaticEntity(STAIRCASE) {};

  Staircase(const Staircase* staircase): Staircase() {
    orientation = staircase->orientation;
  }

  Gamma::Orientation orientation;
};

/**
 * Trigger Entities
 * ----------------
 */
enum TriggerEntityType {
  WORLD_ORIENTATION_CHANGE
};

struct TriggerEntity {
  TriggerEntityType type;

  TriggerEntity(TriggerEntityType type): type(type) {};
};

struct WorldOrientationChange : TriggerEntity {
  WorldOrientationChange(): TriggerEntity(WORLD_ORIENTATION_CHANGE) {};

  WorldOrientation targetWorldOrientation;
};

/**
 * Dynamic entities
 * ----------------
 */
struct DynamicEntity {};