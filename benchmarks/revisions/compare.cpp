#include "ofxEnTT.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace ofxEnTT;

namespace {
volatile float sink = 0;
struct Particle { glm::vec3 position, velocity; };
struct Benchmark {
    std::string kind, mode;
    size_t count;
    int meshes;
    World world;
    std::vector<Entity> entities;
    std::vector<Particle> particles;
    std::vector<std::shared_ptr<ofVboMesh>> geometry;
    std::array<std::vector<glm::vec4>, 4> columns;
    std::vector<glm::vec4> colors;
#ifdef OFXENTT_MERGED
    LinearMotionBatch bulk;
    std::vector<detail::DrawBatch> batches;
    detail::BatchIndex index;
#endif

    Benchmark(std::string scenario, size_t n, std::string configuration, int meshCount)
        : kind(std::move(scenario)), mode(std::move(configuration)), count(n), meshes(meshCount) {
#ifdef OFXENTT_MERGED
        if (mode == "parallel") world.workers.setThreadCount(4);
#endif
        if (kind == "bulk") {
#ifdef OFXENTT_MERGED
            bulk.reserve(count);
#else
            particles.reserve(count);
#endif
            for (size_t i = 0; i < count; ++i) {
                glm::vec3 p(float(i % 97), .5f, -1.f), v(.3f, float(i % 17) * .02f, -.1f);
#ifdef OFXENTT_MERGED
                bulk.add(p, v);
#else
                particles.push_back({p, v});
#endif
            }
        } else {
            const bool moving = kind == "linear" || kind == "rotating" || kind == "hierarchy" || kind == "scatter" || kind.starts_with("frame_");
            const bool hierarchy = kind == "hierarchy" || kind == "static_hierarchy";
            world.builtInSystems = moving;
            entities = world.create(count);
            if (kind == "render" || kind.starts_with("frame_")) {
                for (int i = 0; i < meshes; ++i) geometry.push_back(std::make_shared<ofVboMesh>(ofMesh::box(1, 1, 1)));
            }
            for (size_t i = 0; i < count; ++i) {
                auto & t = world.registry.get<Transform>(entities[i]);
                t.position = { float(i % 97) * .03f, float(i % 17) * -.02f, float(i % 11) * .01f };
                t.orientation = glm::quat(glm::vec3(float(i % 7) * .11f, float(i % 5) * -.09f, float(i % 3) * .07f));
                t.scale = { i % 3 ? 1.1f : -.9f, .8f, 1.2f };
                if (moving) world.registry.emplace<Velocity>(entities[i], glm::vec3(.1f, float(i % 17) * .02f, -.3f),
                    kind == "rotating" || kind == "hierarchy" || kind == "frame_rotating" ? glm::vec3(.1f, .2f, .3f) : glm::vec3(0.f));
                if (hierarchy && i % 8) world.setParent(entities[i], entities[i - 1]);
                if (kind == "render" || kind.starts_with("frame_")) world.registry.emplace<MeshRenderer>(entities[i], geometry[i % meshes]).color
                    = ofFloatColor(float(i % 255) / 255.f, .2f, .4f, 1.f);
            }
            if (kind == "render" || kind.starts_with("frame_")) {
                // Independently churn the renderer pool to prevent matching storage order.
                for (size_t i = 0; i < count; i += 7) world.registry.remove<MeshRenderer>(entities[i]);
                for (size_t i = 0; i < count; i += 7) world.registry.emplace<MeshRenderer>(entities[i], geometry[i % meshes]);
            }
#ifdef OFXENTT_MERGED
            if (mode == "group" || mode == "parallel") world.enableVelocityGroup();
            if (mode == "incremental") world.transforms.setIncremental(true);
            if (mode == "bulk-sync") {
                bulk.reserve(count);
                for (auto e : entities) {
                    bulk.add(world.registry.get<Transform>(e).position, world.registry.get<Velocity>(e).linear, e);
                    world.registry.remove<Velocity>(e);
                }
            }
#endif
            world.update(0.f);
        }
        for (int i = 0; i < 4; ++i) frame();
    }

    template <typename Access>
    void pack(size_t n, Access access, std::ofstream * dump) {
        for (auto & column : columns) column.resize(n);
        colors.resize(n);
        for (size_t i = 0; i < n; ++i) {
            auto [renderer, transform] = access(i);
            for (int c = 0; c < 4; ++c) columns[c][i] = transform->matrix[c];
            const auto & color = renderer->color;
            colors[i] = { color.r, color.g, color.b, color.a };
        }
        if (n) sink = columns[0][0].x;
        if (dump) {
            for (const auto & column : columns) dump->write(reinterpret_cast<const char *>(column.data()), n * sizeof(glm::vec4));
            dump->write(reinterpret_cast<const char *>(colors.data()), n * sizeof(glm::vec4));
        }
    }

    void render(std::ofstream * dump = nullptr) {
#ifdef OFXENTT_MERGED
        auto active = detail::collectBatches(world.registry, batches, index);
        for (size_t b = 0; b < active; ++b) {
            auto & batch = batches[b];
            pack(batch.items.size(), [&](size_t i) {
                return std::pair{batch.items[i].renderer, batch.items[i].world};
            }, dump);
            batch.items.clear();
        }
#else
        for (const auto & batch : RenderSystem::collectBatches(world.registry)) {
            pack(batch.entities.size(), [&](size_t i) {
                return std::pair{&world.registry.get<MeshRenderer>(batch.entities[i]), &world.registry.get<WorldTransform>(batch.entities[i])};
            }, dump);
        }
#endif
    }

    void frame() {
        constexpr float dt = 1.f / 60.f;
        if (kind == "bulk") {
#ifdef OFXENTT_MERGED
            if (mode == "parallel") bulk.update(dt, world.workers);
            else bulk.update(dt);
            sink = bulk.position(count - 1).x;
#else
            for (auto & p : particles) p.position += p.velocity * dt;
            sink = particles.back().position.x;
#endif
        } else if (kind == "render") render();
        else {
#ifdef OFXENTT_MERGED
            if (mode == "bulk-sync") { bulk.update(dt); bulk.syncTo(world); }
#endif
            world.update(kind.starts_with("static_") ? 0.f : dt);
            if (kind.starts_with("frame_")) render();
            sink = world.registry.get<WorldTransform>(entities.back()).matrix[3].x;
        }
    }

    void dump(const std::string & path) {
        std::ofstream out(path, std::ios::binary);
        if (!out) throw std::runtime_error("cannot open state dump");
        if (kind == "render") { render(&out); return; }
        if (kind == "bulk") {
            for (size_t i = 0; i < count; ++i) {
#ifdef OFXENTT_MERGED
                auto p = bulk.position(i);
#else
                auto p = particles[i].position;
#endif
                float values[] = {p.x, p.y, p.z};
                out.write(reinterpret_cast<const char *>(values), sizeof(values));
            }
        } else {
            for (auto e : entities) {
                const auto & t = world.registry.get<Transform>(e);
                const auto & w = world.registry.get<WorldTransform>(e);
                float local[] = {t.position.x, t.position.y, t.position.z, t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w,
                    t.scale.x, t.scale.y, t.scale.z};
                out.write(reinterpret_cast<const char *>(local), sizeof(local));
                for (int c = 0; c < 4; ++c) {
                    float column[] = {w.matrix[c].x, w.matrix[c].y, w.matrix[c].z, w.matrix[c].w};
                    out.write(reinterpret_cast<const char *>(column), sizeof(column));
                }
            }
            if (kind.starts_with("frame_")) render(&out);
        }
    }
};
}

int main(int argc, char ** argv) {
    try {
        if (argc != 5) throw std::runtime_error("usage: bench kind count mode meshCount");
        Benchmark benchmark(argv[1], std::stoull(argv[2]), argv[3], std::stoi(argv[4]));
        std::cout << "{\"ready\":true}" << std::endl;
        std::string command;
        while (std::cin >> command) {
            if (command == "measure") {
                int iterations; std::cin >> iterations;
                auto start = std::chrono::steady_clock::now();
                for (int i = 0; i < iterations; ++i) benchmark.frame();
                auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / iterations;
                std::cout << std::setprecision(17) << "{\"us\":" << us << "}" << std::endl;
            } else if (command == "dump") {
                std::string path; std::getline(std::cin >> std::ws, path); benchmark.dump(path);
                std::cout << "{\"dumped\":true}" << std::endl;
            } else if (command == "quit") break;
            else throw std::runtime_error("unknown command");
        }
    } catch (const std::exception & error) { std::cerr << error.what() << std::endl; return 1; }
}
