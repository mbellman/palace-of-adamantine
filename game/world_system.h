#pragma once

#include <string>
#include <unordered_map>

#include "grid_utilities.h"
#include "entities.h"

struct GridCoordinatesHasher {
  std::size_t operator()(const GridCoordinates& coordinates) const {
    u32 ux = u32(coordinates.x + 32768) & 0xFFFF;
    u32 uy = u32(coordinates.y + 32768) & 0xFFFF;
    u32 uz = u32(coordinates.z + 32768) & 0xFFFF;

    // @todo check the frequency of hash collisions in actual level layouts
    return (ux << 16 | uy) + uz;
  }
};

template<typename T>
struct GridMap : std::unordered_map<GridCoordinates, T*, GridCoordinatesHasher> {
  void operator[](const GridCoordinates& coordinates) = delete;

  template<typename E = T>
  E* get(const GridCoordinates& coordinates) {
    if (find(coordinates) != end()) {
      return (E*)at(coordinates);
    }

    return nullptr; 
  }

  template<typename E>
  u32 count() {
    u32 total = 0;

    for (auto& [ coordinates, entity ] : *this) {
      if (dynamic_cast<E*>(entity) != nullptr) {
        total++;
      }
    }

    return total;
  }

  void remove(const GridCoordinates& coordinates) {
    if (find(coordinates) != end()) {
      T* t = at(coordinates);

      delete t;

      erase(coordinates);
    }
  }

  void set(const GridCoordinates& coordinates, T* value) {
    remove(coordinates);
    insert({ coordinates, value });
  }
};

struct DynamicEntityManager {
  DynamicEntity* entities = nullptr;
};

struct Area {
  // @todo
};

struct World {
  GridMap<StaticEntity> grid;
  GridMap<TriggerEntity> triggers;
  DynamicEntityManager entities;
};