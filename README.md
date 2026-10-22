# ofxEnTT

![ofxEnTT](ofxaddons_thumbnail.png)

[EnTT](https://github.com/skypjack/entt) for openFrameworks: a fast entity component system, with
a world that runs its systems every frame, a transform hierarchy, instanced rendering, JSON save /
load, and bridges to `ofParameter`, ofxGui, ofxImGui and ofxAssimpModelLoader.

EnTT 4.0.0 (header-only, MIT, by Michele Caini) is included in `libs/entt`.

## Requirements

- openFrameworks 0.12.1 or later (master), any build system: make, projectGenerator (Xcode, Visual
  Studio, VS Code), or CMake
- C++20 (EnTT 4). macOS, iOS, Visual Studio 2022 / 2026, Emscripten and Linux already build with
  C++20 or later; `addon_config.mk` raises MSYS2 and Raspberry Pi armv6 / armv7 to C++20. Visual Studio
  2019 (C++17) isn't supported.
- Instanced drawing needs the programmable renderer: `ofGLWindowSettings::setGLVersion(3, 3)`, or
  GLES 3 / WebGL 2. Without it, entities are drawn one by one.

## Quick start

```cpp
#include "ofxEnTT.h"
using namespace ofxEnTT;

World world;
auto cube = std::make_shared<ofVboMesh>(ofMesh::box(10, 10, 10));

// setup
for (int i = 0; i < 10000; i++) {
	auto e = world.create("cube");
	world.registry.get<Transform>(e).position = glm::sphericalRand(500.f);
	world.registry.emplace<Velocity>(e, glm::vec3(0), glm::vec3(0, 1, 0)); // spins
	world.registry.emplace<MeshRenderer>(e, cube).color = ofFloatColor::orange;
}

// update
world.update();

// draw
cam.begin();
world.draw();   // 10000 cubes, one instanced draw call
cam.end();
```

`world.registry` is a plain `entt::registry`, so the whole EnTT API is there: views, groups, sorting,
signals, snapshots, and your own components. Add your own systems with
`world.addSystem("name", [](World & w, float dt) { ... })`.

## Components

| Component | |
|---|---|
| `Name` | display name (inspectors, JSON, `findByName`) |
| `Transform` | local position, orientation (quaternion; Euler helpers in degrees), scale |
| `WorldTransform` | world matrix, written by the TransformSystem each update |
| `Parent` / `Children` | hierarchy; use `World::setParent` (refuses cycles) |
| `MeshRenderer` | shared `ofVboMesh`, optional `ofTexture` / `ofMaterial`, colour, draw mode |
| `Hidden` | tag: not drawn |
| `Velocity` | linear (units/s) and angular (rad/s) motion |
| `Lifetime` | destroyed when it reaches zero |

## World

- `create(name)`, `destroy(e)` (with its children), `clear()`, `size()`, `isValid(e)`
- `setParent(child, parent)`, `getParent`, `getChildren`, `getRoots`, `getEntities`
- `findByName`, `getName`, `getBounds(e, min, max)` (world-space bounds of e and its children)
- `clone(e)`: copies an entity and its children, sharing meshes, so the copies draw instanced
- `addSystem` / `removeSystem` / `hasSystem`: run in the order added, after Velocity and Lifetime and
  before the TransformSystem
- `update()` / `draw()`, or `setAutoUpdate(true)` / `setAutoDraw(true)` to run them from OF's events

## Rendering

The RenderSystem groups visible `MeshRenderer`s by mesh, texture and draw mode. Each group is one
`drawInstanced` call: per-instance matrices and colours go into vertex attributes 5-9 of the mesh's
VBO, and a built-in shader draws them with simple directional lighting (`renderer.lighting`,
`renderer.lightDirection`). Entities with an `ofMaterial` are drawn one at a time (ofMaterial has its
own shader). `renderer.instancing = false` draws everything one at a time. `renderer.getStats()` gives
draw calls and entity counts.

Textures are sampled as `sampler2D`: call `ofDisableArbTex()` before loading them.

## JSON

```cpp
ofxEnTT::saveJson(world, "scene.json");
ofxEnTT::loadJson(world, "scene.json");   // Name, Transform, Parent, Velocity, Lifetime, Hidden, colour
```

Meshes, textures and materials aren't saved: give them back to the loaded entities, e.g. by name.

## Bridges

The ofParameter bridge is part of the addon. The others are header-only: include them, and add their
addon to the project, only where you use them.

### ofParameter

`ofxEnTT::EntityParameters` turns one entity's components into an `ofParameterGroup`. Changing a
parameter writes the component, and `pull()` (each frame) follows systems that move the entity. The
group only lists the components the entity has, and `groupChanged` fires when that changes.

### ofxGui (`ofxEnTTGui.h`, ofxGui)

```cpp
ofxEnTT::GuiInspector inspector;
inspector.setup(world);     // setup
inspector.update();         // update, after world.update()
inspector.draw();           // draw, outside the camera
inspector.select(entity);
```

### ofxImGui (`ofxEnTTImGui.h`, [ofxImGui](https://github.com/jvcleave/ofxImGui) `develop` branch)

```cpp
ofxEnTT::Entity selected = ofxEnTT::Null;
gui.begin();
ofxEnTT::imgui::inspector(world, selected);   // tree, create / clone / destroy, component editors
gui.end();
```

`ofxEnTT::imgui::entityTree` and `entityEditor` are there for your own windows.

### ofxAssimpModelLoader (`ofxEnTTAssimp.h`, ofxAssimpModelLoader)

```cpp
auto model = std::make_shared<ofxAssimpModelLoader>();
model->load("druid.gltf");
auto druid = ofxEnTT::addModel(world, model, "druid");   // root + a child per mesh
auto another = world.clone(druid);                      // shares the meshes: instanced
ofxEnTT::updateModelAnimations(world);                  // in update, for animated models
```

`ModelOptions`: `materials` (keep `ofMaterial`s instead of using the diffuse colour), `textures`, and
`yUp` (on by default: removes the loader's 180 degree turn for OF's y-down screen, so models stand
upright under `ofEasyCam`).

## Examples

| Example | Addons | |
|---|---|---|
| `example_instancing` | ofxGui | 20,000 spinning cubes in one draw call; space bursts particles with a Lifetime |
| `example_hierarchy` | ofxGui | a solar system of parent / child transforms; s / l save and load JSON |
| `example_assimp` | ofxAssimpModelLoader, ofxGui | a model as entities, cloned into an instanced grid; drop a model file to swap it |
| `example_imgui` | [ofxImGui](https://github.com/jvcleave/ofxImGui) (`develop`) | the ImGui inspector on a spinning ring of groups |
| `example_tests` | ofxUnitTests | the addon's unit tests (headless, 63 checks) |

Generate the project files with projectGenerator, or copy `scripts/templates/<platform>/Makefile`
and `config.make` (or the CMake template) into the example folder.

## License

ofxEnTT: MIT, Dan Rosser. EnTT: MIT, Michele Caini (`libs/entt/license/LICENSE`).
