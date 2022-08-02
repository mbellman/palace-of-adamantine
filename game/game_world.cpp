#include <functional>
#include <string>
#include <vector>

#include "Gamma.h"

#include "game_world.h"
#include "game_state.h"
#include "build_flags.h"

#define CREATE(m) []() { return m; }
#define CONFIGURE [](Mesh& m)

using namespace Gamma;

struct MeshBuilder {
  std::string meshName;
  u16 maxInstances;
  std::function<Mesh*()> create;
  std::function<void(Mesh&)> configure = nullptr;
};

const static std::vector<MeshBuilder> meshBuilders = {
  {
    "dirt-floor", 0xffff,
    CREATE(Mesh::Plane(2)),
    CONFIGURE {
      m.texture = "./game/textures/dirt-floor.png";
      m.normalMap = "./game/textures/dirt-normals.png";
    }
  },
  {
    "dirt-wall", 0xffff,
    CREATE(Mesh::Model("./game/models/wall.obj")),
    CONFIGURE {
      m.texture = "./game/textures/dirt-wall.png";
      m.normalMap = "./game/textures/dirt-normals.png";
    }
  },
  {
    "water", 1000,
    CREATE(Mesh::Plane(2)),
    CONFIGURE {
      m.type = MeshType::WATER;
    }
  },
  {
    "rock", 1000,
    CREATE(Mesh::Model("./game/models/rock.obj"))
  },
  {
    "arch", 1000,
    CREATE(Mesh::Model("./game/models/arch.obj")),
    CONFIGURE {
      m.texture = "./game/textures/wood.png";
      m.normalMap = "./game/textures/wood-normals.png";
    }
  },
  {
    "arch-vines", 1000,
    CREATE(Mesh::Model("./game/models/arch-vines.obj")),
    CONFIGURE {
      m.type = MeshType::FOLIAGE;
      m.foliage.type = FoliageType::LEAF;
      m.foliage.speed = 3.f;
    }
  },
  {
    "tulips", 1000,
    CREATE(Mesh::Model("./game/models/tulips.obj")),
    CONFIGURE {
      m.type = MeshType::FOLIAGE;
      m.foliage.type = FoliageType::FLOWER;
    }
  },
  {
    "tulip-petals", 1000,
    CREATE(Mesh::Model("./game/models/tulip-petals.obj")),
    CONFIGURE {
      m.type = MeshType::FOLIAGE;
      m.foliage.type = FoliageType::FLOWER;
    }
  },
  {
    "hedge", 1000,
    CREATE(Mesh::Model("./game/models/hedge.obj")),
    CONFIGURE {
      m.texture = "./game/textures/hedge.png";
      m.normalMap = "./game/textures/hedge-normals.png";
    }
  },
  {
    "stone-tile", 1000,
    CREATE(Mesh::Model("./game/models/stone-tile.obj"))
  },
  {
    "rosebush", 1000,
    CREATE(Mesh::Model("./game/models/rosebush-leaves.obj")),
    CONFIGURE {
      m.type = MeshType::FOLIAGE;
      m.foliage.type = FoliageType::FLOWER;
      m.texture = "./game/textures/rose-leaves.png";
      m.normalMap = "./game/textures/rose-leaves-normals.png";
    }
  },
  {
    "rosebush-flowers", 1000,
    CREATE(Mesh::Model("./game/models/rosebush-flowers.obj")),
    CONFIGURE {
      m.type = MeshType::FOLIAGE;
      m.foliage.type = FoliageType::FLOWER;
      m.texture = "./game/textures/rose-petals.png";
      m.normalMap = "./game/textures/rose-petals-normals.png";
      m.emissivity = 0.25f;
    }
  },
  {
    "gate-column", 250,
    CREATE(Mesh::Model("./game/models/gate-column.obj")),
    CONFIGURE {
      m.texture = "./game/textures/brick.png";
      m.normalMap = "./game/textures/brick-normals.png";
    }
  },
  {
    "gate", 250,
    CREATE(Mesh::Model("./game/models/gate.obj"))
  },
  {
    "tile-1", 0xffff,
    CREATE(Mesh::Plane(2)),
    CONFIGURE {
      m.texture = "./game/textures/tile-1.png";
      m.normalMap = "./game/textures/tile-1-normals.png";
    }
  },
  {
    "column", 1000,
    CREATE(Mesh::Model("./game/models/column.obj"))
  }
};

void addMeshes(Globals) {
  // Grid entity objects
  addMesh("ground", 0xffff, Mesh::Cube());
  mesh("ground")->canCastShadows = false;
  addMesh("staircase", 0xffff, Mesh::Model("./game/models/staircase.obj"));
  addMesh("switch", 1000, Mesh::Model("./game/models/switch.obj"));

  // Placeable meshes
  for (auto& builder : meshBuilders) {
    addMesh(builder.meshName, builder.maxInstances, builder.create());

    if (builder.configure != nullptr) {
      builder.configure(*mesh(builder.meshName));
    }
  }

  // Static world structures
  addMesh("potm-facade", 1, Mesh::Model("./game/models/potm-facade.obj"));

  #if DEVELOPMENT == 1
    // Trigger entity indicators
    addMesh("trigger-indicator", 0xffff, Mesh::Cube());
    mesh("trigger-indicator")->disabled = true;

    // Light indicators
    addMesh("light-indicator", 1000, Mesh::Cube());  // @todo Mesh::Sphere()
    mesh("light-indicator")->type = MeshType::EMISSIVE;
    mesh("light-indicator")->disabled = true;

    // Ranged placement preview
    addMesh("range-preview", 0xffff, Mesh::Cube());
  #endif
}

void addZones(Globals) {
  Zone z1 = {
    "Lunar Garden",
    { -5, -5, -20 },
    { 10, 7, 26 },
    {
      "dirt-floor",
      "dirt-wall",
      "water",
      "rock",
      "arch",
      "arch-vines",
      "tulips",
      "tulip-petals",
      "hedge",
      "stone-tile",
      "rosebush",
      "rosebush-flowers",
      "gate-column",
      "gate",
      "potm-facade"
    }
  };

  Zone z2 = {
    "Palace of the Moon",
    { -17, -32, 6 },
    { 17, 27, 67 },
    {
      "tile-1",
      "column"
    }
  };

  state.world.zones.push_back(z1);
  state.world.zones.push_back(z2);
}