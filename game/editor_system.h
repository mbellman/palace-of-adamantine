#pragma once

#include <string>
#include <vector>

#include "Gamma.h"

#include "game_entities.h"
#include "grid_utilities.h"
#include "build_flags.h"

#define MAX_EDIT_ACTIONS 5

struct GmContext;
struct GameState;

const static std::vector<u8> editorEntityCycle = {
  GROUND,
  STAIRCASE,
  SWITCH,
  WORLD_ORIENTATION_CHANGE
};

struct ReplacedEntityRecord {
  GridCoordinates coordinates;
  GridEntity* oldEntity = nullptr;
};

struct EditAction : ReplacedEntityRecord {
  // Ranged actions
  bool isRangedPlacementAction = false;
  GridCoordinates rangeFrom;
  GridCoordinates rangeTo;
  std::vector<ReplacedEntityRecord> replacedEntityRecords;
};

struct WorldEditor {
  // Flags
  bool enabled = false;
  bool useRange = false;
  bool rangeFromSelected = false;
  bool deleting = false;

  // Placeable entity selection
  EntityType currentSelectedEntityType = GROUND;  // @todo deprecate
  u8 currentSelectedEntityIndex = 0;              // @todo use as new value
  float lastEntityChangeTime = 0;

  // Placeable mesh selection
  bool isPlacingMesh = false;
  std::string currentMeshName = "";
  // @todo handle light placement

  // Default orientation for placed entities
  Gamma::Orientation currentEntityOrientation;

  // Default world orientation target for World Orientation Change triggers
  WorldOrientation currentSelectedWorldOrientation = POSITIVE_Y_UP;

  // Ranged actions
  GridCoordinates rangeFrom;
  GridCoordinates rangeTo;

  // Edit action history/undo management
  EditAction editActions[MAX_EDIT_ACTIONS];
  u8 totalEditActions = 0;
};

#if DEVELOPMENT == 1
  void setCurrentSelectedEntityType(GmContext* context, GameState& state, EntityType type);
  void setCurrentMeshName(GmContext* context, GameState& state, const std::string& meshName);
  void adjustCurrentEntityOrientation(GmContext* context, GameState& state, const Gamma::Orientation& adjustment);
  void selectRangeFrom(GmContext* context, GameState& state);
  void showGridEntityPlacementPreview(GmContext* context, GameState& state);
  void showRangeFromSelectionPreview(GmContext* context, GameState& state);
  void showRangedEntityPlacementPreview(GmContext* context, GameState& state);
  void showMeshPlacementPreview(GmContext* context, GameState& state);
  void handleEditorSingleTileClickAction(GmContext* context, GameState& state);
  void handleEditorRangedClickAction(GmContext* context, GameState& state);
  void handleEditorMeshPlacementAction(GmContext* context, GameState& state);
  void undoPreviousEditAction(GmContext* context, GameState& state);
  void placeCameraAtClosestWalkableTile(GmContext* context, GameState& state);
#endif