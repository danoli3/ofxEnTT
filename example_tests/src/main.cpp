#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofxUnitTests.h"
#include "ofxEnTT.h"
#include <algorithm>
#include <stdexcept>

using namespace ofxEnTT;

namespace {
bool near(const glm::vec3 & a, const glm::vec3 & b, float epsilon = 1e-4f) {
	return glm::all(glm::lessThan(glm::abs(a - b), glm::vec3(epsilon)));
}
} // namespace

class ofApp : public ofxUnitTestsApp {
	void run() {
		testCreateDestroy();
		testHierarchy();
		testWorldTransforms();
		testDirectTransforms();
		testIncrementalTransforms();
		testSystems();
		testVelocityGroup();
		testLinearMotionBatch();
		testParallelExecutor();
		testParallelWorld();
		testBatches();
		testBatchReuse();
		testVisibility();
		testClone();
		testJson();
		testParameters();
	}

	void testCreateDestroy() {
		World world;
		auto a = world.create("a");
		auto b = world.create();
		ofxTestEq(world.size(), size_t(2), "create adds entities");
		ofxTest(world.registry.all_of<Transform>(a), "create adds a Transform");
		ofxTest(world.registry.all_of<Name>(a) && !world.registry.all_of<Name>(b), "Name only when given");
		ofxTestEq(world.getName(a), std::string("a"), "getName");
		ofxTest(world.findByName("a") == a, "findByName");
		ofxTest(world.findByName("missing") == Null, "findByName missing");
		world.destroy(a);
		ofxTest(!world.isValid(a) && world.isValid(b), "destroy");
		ofxTestEq(world.size(), size_t(1), "size after destroy");
		world.clear();
		ofxTestEq(world.size(), size_t(0), "clear");
	}

	void testHierarchy() {
		World world;
		auto root = world.create("root");
		auto child = world.create("child");
		auto grandchild = world.create("grandchild");
		ofxTest(world.setParent(child, root), "setParent");
		ofxTest(world.setParent(grandchild, child), "setParent grandchild");
		ofxTest(world.getParent(grandchild) == child, "getParent");
		ofxTestEq(world.getChildren(root).size(), size_t(1), "getChildren");
		ofxTest(!world.setParent(root, grandchild), "setParent refuses a cycle");
		ofxTest(!world.setParent(root, root), "setParent refuses itself");
		ofxTestEq(world.getRoots().size(), size_t(1), "one root");

		auto other = world.create("other");
		ofxTest(world.setParent(child, other), "reparent");
		ofxTestEq(world.getChildren(root).size(), size_t(0), "reparent removes from old parent");
		ofxTestEq(world.getChildren(other).size(), size_t(1), "reparent adds to new parent");
		ofxTest(world.setParent(child, Null), "detach");
		ofxTest(world.getParent(child) == Null, "detached");
		ofxTestEq(world.getChildren(other).size(), size_t(0), "detach removes from parent");

		world.setParent(child, root);
		world.destroy(root);
		ofxTest(!world.isValid(root) && !world.isValid(child) && !world.isValid(grandchild), "destroy removes children");
		ofxTest(world.isValid(other), "destroy leaves other entities");

		auto p = world.create("p");
		auto c = world.create("c");
		world.setParent(c, p);
		world.destroy(c);
		ofxTestEq(world.getChildren(p).size(), size_t(0), "destroying a child removes it from its parent");
	}

	void testWorldTransforms() {
		World world;
		auto parent = world.create("parent");
		auto child = world.create("child");
		world.setParent(child, parent);
		auto & pt = world.registry.get<Transform>(parent);
		pt.position = { 10, 0, 0 };
		pt.setOrientationEulerDeg({ 0, 90, 0 });
		pt.scale = glm::vec3(2);
		world.registry.get<Transform>(child).position = { 1, 0, 0 };
		world.update(0.f);
		auto p = world.registry.get<WorldTransform>(child).getPosition();
		// child at local x=1, scaled by 2, rotated 90 deg about y -> -z, then moved by +10 x
		ofxTest(near(p, { 10, 0, -2 }), "child world position follows parent " + ofToString(p));
		ofxTest(near(world.registry.get<WorldTransform>(parent).getPosition(), { 10, 0, 0 }), "parent world position");

		auto euler = glm::vec3(10, 20, 30);
		Transform t;
		t.setOrientationEulerDeg(euler);
		ofxTest(near(t.getOrientationEulerDeg(), euler, 1e-3f), "Euler round trip");
	}

	void testDirectTransforms() {
		bool matches = true;
		for (int i = 0; i < 128; ++i) {
			Transform t;
			t.position = { float(i - 64), 2.5f, -3.f };
			t.orientation = glm::quat(glm::vec3(i * .031f, i * -.017f, i * .009f));
			t.scale = { i % 2 ? -2.f : 0.f, .5f, 3.f };
			auto reference = glm::translate(glm::mat4(1.f), t.position)
				* glm::mat4_cast(t.orientation) * glm::scale(glm::mat4(1.f), t.scale);
			auto matrix = t.localMatrix();
			for (int c = 0; c < 4; ++c) {
				matches &= glm::all(glm::lessThanEqual(glm::abs(matrix[c] - reference[c]), glm::vec4(1e-4f)));
			}
		}
		ofxTest(matches, "direct TRS matches matrix products, including zero and negative scales");
	}

	void testIncrementalTransforms() {
		World world;
		auto root = world.create(), child = world.create(), leaf = world.create(), other = world.create();
		world.setParent(child, root);
		world.setParent(leaf, child);
		world.transforms.setIncremental(true);
		world.update(0.f);
		ofxTestEq(world.transforms.getStats().updated, size_t(4), "incremental first update computes everything");
		world.update(0.f);
		ofxTest(world.transforms.getStats().updated == 0 && world.transforms.getStats().skipped == 4, "unchanged transforms skip matrix writes");
		world.registry.get<Transform>(root).position = { 2, 0, 0 };
		world.update(0.f);
		ofxTest(world.transforms.getStats().updated == 3
			&& near(world.registry.get<WorldTransform>(leaf).getPosition(), { 2, 0, 0 }), "direct field edits propagate through descendants");
		world.registry.get<Transform>(child).scale = { -2, 3, 0 };
		world.registry.get<Transform>(leaf).position = { 1, 1, 1 };
		world.update(0.f);
		ofxTest(world.transforms.getStats().updated == 2
			&& near(world.registry.get<WorldTransform>(leaf).getPosition(), { 0, 3, 0 }), "incremental scale edits update descendants");
		world.setParent(child, other);
		world.update(0.f);
		ofxTest(near(world.registry.get<WorldTransform>(leaf).getPosition(), { -2, 3, 0 }), "incremental reparent refreshes world transforms");
		world.registry.remove<WorldTransform>(child);
		world.update(0.f);
		ofxTest(world.registry.all_of<WorldTransform>(child) && world.transforms.getStats().updated == 2, "removed world matrices are regenerated");
		world.transforms.markDirty(root);
		world.update(0.f);
		ofxTestEq(world.transforms.getStats().updated, size_t(1), "explicit dirty mark forces a refresh");
		world.transforms.invalidate();
		world.update(0.f);
		ofxTestEq(world.transforms.getStats().updated, size_t(4), "invalidation refreshes all cached matrices");
		world.destroy(child);
		world.update(0.f);
		auto replacement = world.create();
		world.registry.get<Transform>(replacement).position = { 8, 0, 0 };
		world.update(0.f);
		ofxTest(near(world.registry.get<WorldTransform>(replacement).getPosition(), { 8, 0, 0 }), "entity reuse does not reuse stale transform state");
		world.transforms.setIncremental(false);
		world.update(0.f);
		ofxTestEq(world.transforms.getStats().updated, world.size(), "disabling the cache restores full updates");
		Registry separate;
		auto e = separate.create();
		separate.emplace<Transform>(e).position = { 9, 0, 0 };
		world.transforms.setIncremental(true);
		world.transforms.update(separate);
		ofxTest(near(separate.get<WorldTransform>(e).getPosition(), { 9, 0, 0 }), "cache handles a different registry");
	}

	void testSystems() {
		World world;
		auto mover = world.create("mover");
		world.registry.emplace<Velocity>(mover, glm::vec3(2, 0, 0), glm::vec3(0));
		auto temp = world.create("temp");
		world.registry.emplace<Lifetime>(temp, 0.5f);
		std::vector<std::string> order;
		world.addSystem("first", [&](World &, float) { order.push_back("first"); });
		world.addSystem("second", [&](World &, float) { order.push_back("second"); });
		world.update(0.25f);
		ofxTest(near(world.registry.get<Transform>(mover).position, { 0.5f, 0, 0 }), "Velocity moves the Transform");
		ofxTest(world.isValid(temp), "Lifetime not yet expired");
		world.update(0.25f);
		ofxTest(!world.isValid(temp), "Lifetime destroys the entity");
		ofxTest(order == std::vector<std::string>({ "first", "second", "first", "second" }), "systems run in order");
		world.addSystem("first", [&](World &, float) { order.push_back("replaced"); });
		world.removeSystem("second");
		order.clear();
		world.update(0.f);
		ofxTest(order == std::vector<std::string>({ "replaced" }), "addSystem replaces, removeSystem removes");
		ofxTest(world.hasSystem("first") && !world.hasSystem("second"), "hasSystem");
		world.builtInSystems = false;
		world.update(1.f);
		ofxTest(near(world.registry.get<Transform>(mover).position, { 1.0f, 0, 0 }), "builtInSystems off stops Velocity");
	}

	void testVelocityGroup() {
		World plain, grouped;
		ofxTest(!grouped.isVelocityGroupEnabled(), "velocity group is opt in");
		grouped.enableVelocityGroup();
		grouped.enableVelocityGroup();
		for (World * world : { &plain, &grouped }) {
			for (int i = 0; i < 24; ++i) {
				auto e = world->create();
				world->registry.emplace<Velocity>(e, glm::vec3(i, .5f, -1.f), glm::vec3(.1f, .2f, .3f));
				if (i == 5) world->registry.remove<Velocity>(e);
				if (i == 9) world->registry.emplace<Lifetime>(e, .1f);
			}
			world->update(.25f);
			world->update(.25f);
		}
		bool matches = plain.size() == grouped.size();
		for (auto e : plain.getEntities()) {
			const auto & a = plain.registry.get<Transform>(e);
			const auto & b = grouped.registry.get<Transform>(e);
			matches &= a.position == b.position && a.orientation == b.orientation;
		}
		ofxTest(matches && grouped.isVelocityGroupEnabled(), "grouped velocity matches views after membership changes and expiry");
	}

	void testLinearMotionBatch() {
		World world;
		world.transforms.setIncremental(true);
		auto e = world.create(), gone = world.create();
		LinearMotionBatch batch;
		batch.reserve(32);
		batch.add({ 1, 2, 3 }, { 2, -1, .5f }, e);
		batch.add({ 9, 9, 9 }, { 1, 1, 1 }, gone);
		batch.add({ 4, 5, 6 }, { 0, 0, 0 });
		world.destroy(gone);
		auto replacement = world.create();
		batch.update(.5f);
		batch.syncTo(world);
		world.update(0.f);
		ofxTest(near(batch.position(0), { 2, 1.5f, 3.25f }), "SoA integrates three axes");
		ofxTest(near(world.registry.get<WorldTransform>(e).getPosition(), batch.position(0)), "SoA scatter works with incremental transforms");
		ofxTest(near(world.registry.get<Transform>(replacement).position, { 0, 0, 0 }), "SoA skips deleted entity versions");
		batch.positionX()[0] = 10.f;
		batch.setVelocity(0, { 1, 2, 3 });
		batch.update(-1.f);
		ofxTest(near(batch.position(0), { 9, -.5f, .25f }), "SoA spans and negative time step");
		batch.remove(1);
		ofxTest(batch.size() == 2 && batch.entity(1) == Null && near(batch.position(1), { 4, 5, 6 }), "SoA swap removal keeps arrays and bindings aligned");
		batch.clear();
		batch.update(1.f);
		batch.add({ 0, 0, 0 }, { 1, 1, 1 });
		batch.update(0.f);
		ofxTest(batch.size() == 1 && near(batch.position(0), { 0, 0, 0 }), "SoA empty/reused batches and zero time step");
		bool rejected = false;
		try { batch.remove(10); } catch (const std::out_of_range &) { rejected = true; }
		ofxTest(rejected && batch.size() == 1, "SoA removal validates the index");
	}

	void testParallelExecutor() {
		ParallelExecutor executor;
		ofxTestEq(executor.getThreadCount(), size_t(1), "workers are opt in");
		executor.setThreadCount(4);
		std::vector<int> visits(10003);
		for (int frame = 0; frame < 4; ++frame) executor.forEachRange(visits.size(), [&](size_t begin, size_t end) {
			for (size_t i = begin; i < end; ++i) visits[i]++;
		}, 1);
		ofxTest(std::all_of(visits.begin(), visits.end(), [](int v) { return v == 4; }), "parallel ranges cover uneven input exactly once");
		bool emptyCalled = false;
		executor.forEachRange(0, [&](size_t, size_t) { emptyCalled = true; });
		ofxTest(!emptyCalled, "empty parallel jobs do not invoke a callback");
		bool caught = false;
		try {
			executor.forEachRange(100, [](size_t begin, size_t end) {
				if (begin <= 75 && end > 75) throw std::runtime_error("expected worker error");
			}, 1);
		} catch (const std::runtime_error &) { caught = true; }
		ofxTest(caught, "worker exceptions return to the calling thread");
		executor.setThreadCount(2);
		executor.forEachRange(visits.size(), [&](size_t begin, size_t end) {
			for (size_t i = begin; i < end; ++i) visits[i]++;
		}, 1);
		ofxTest(std::all_of(visits.begin(), visits.end(), [](int v) { return v == 5; }), "worker pool survives exceptions and resizing");
		caught = false;
		try { executor.forEachRange(1, [&](size_t, size_t) { executor.forEachRange(1, [](size_t, size_t) {}); }); }
		catch (const std::logic_error &) { caught = true; }
		ofxTest(caught, "nested jobs are rejected instead of deadlocking");
		executor.setThreadCount(0);
		ofxTestEq(executor.getThreadCount(), size_t(1), "zero thread count restores serial execution");
		LinearMotionBatch serial, parallel;
		for (int i = 0; i < 10003; ++i) { serial.add(glm::vec3(i), glm::vec3(.25f)); parallel.add(glm::vec3(i), glm::vec3(.25f)); }
		executor.setThreadCount(4);
		serial.update(.5f); parallel.update(.5f, executor, 1);
		bool matches = true;
		for (size_t i = 0; i < serial.size(); ++i) matches &= near(serial.position(i), parallel.position(i));
		ofxTest(matches, "parallel SoA motion matches serial motion");
	}

	void testParallelWorld() {
		World serial, parallel;
		parallel.workers.setThreadCount(4);
		parallel.velocityMinItemsPerThread = 1;
		parallel.enableVelocityGroup();
		for (World * world : { &serial, &parallel }) {
			world->transforms.setIncremental(true);
			auto entities = world->create(size_t(1000));
			for (size_t i = 0; i < entities.size(); ++i) {
				world->registry.emplace<Velocity>(entities[i], glm::vec3(.1f, i % 5, -.3f), glm::vec3(.1f, .2f, .3f));
				if (i % 8) world->setParent(entities[i], entities[i - 1]);
				if (i == 16) world->registry.emplace<Lifetime>(entities[i], .1f);
			}
			world->update(.25f); world->update(.25f);
		}
		bool matches = serial.size() == parallel.size();
		for (auto e : serial.getEntities()) {
			matches &= near(serial.registry.get<Transform>(e).position, parallel.registry.get<Transform>(e).position);
			matches &= near(serial.registry.get<WorldTransform>(e).getPosition(), parallel.registry.get<WorldTransform>(e).getPosition());
		}
		ofxTest(matches, "parallel velocity joins before expiry and incremental hierarchy updates");
		auto entities = parallel.getEntities();
		bool deferred = false;
		parallel.addSystem("queue", [&](World & w, float) {
			w.workers.forEachRange(entities.size(), [&](size_t begin, size_t end) {
				for (size_t i = begin; i < end; ++i) w.defer([e = entities[i]](World & target) {
					if (target.isValid(e)) target.registry.emplace_or_replace<Hidden>(e);
				});
			}, 1);
			deferred = w.registry.view<Hidden>().size() == 0;
		});
		parallel.update(0.f);
		ofxTest(deferred && parallel.registry.view<Hidden>().size() == parallel.size(), "worker structural commands apply after systems join");
		parallel.removeSystem("queue");
		bool first = false, next = false;
		parallel.defer([&](World & w) { first = true; w.defer([&](World &) { next = true; }); });
		parallel.update(0.f);
		ofxTest(first && !next, "commands queued during draining wait for the next update");
		parallel.update(0.f);
		ofxTest(next, "deferred commands survive until the next update");
	}

	void testBatches() {
		World world;
		auto box = std::make_shared<ofVboMesh>(ofMesh::box(1, 1, 1));
		auto sphere = std::make_shared<ofVboMesh>(ofMesh::sphere(1));
		auto material = std::make_shared<ofMaterial>();
		for (int i = 0; i < 10; i++) {
			auto e = world.create();
			world.registry.emplace<MeshRenderer>(e, box);
		}
		for (int i = 0; i < 3; i++) {
			auto e = world.create();
			world.registry.emplace<MeshRenderer>(e, sphere);
		}
		auto hidden = world.create();
		world.registry.emplace<MeshRenderer>(hidden, box);
		world.registry.emplace<Hidden>(hidden);
		auto withMaterial = world.create();
		world.registry.emplace<MeshRenderer>(withMaterial, box, material);
		world.registry.emplace<MeshRenderer>(world.create()); // no mesh
		world.update(0.f);

		auto batches = RenderSystem::collectBatches(world.registry);
		ofxTestEq(batches.size(), size_t(3), "batches: box, sphere, box+material");
		size_t boxes = 0, spheres = 0, materials = 0;
		for (auto & b : batches) {
			if (b.mesh == box.get() && !b.material) boxes = b.entities.size();
			if (b.mesh == sphere.get()) spheres = b.entities.size();
			if (b.material) materials = b.entities.size();
		}
		ofxTestEq(boxes, size_t(10), "box batch skips Hidden");
		ofxTestEq(spheres, size_t(3), "sphere batch");
		ofxTestEq(materials, size_t(1), "material in its own batch");
	}

	void testBatchReuse() {
		World world;
		auto mesh = std::make_shared<ofVboMesh>(ofMesh::box(1, 1, 1));
		auto a = world.create(), b = world.create();
		world.registry.emplace<MeshRenderer>(a, mesh);
		world.registry.emplace<MeshRenderer>(b, mesh);
		world.update(0.f);
		std::vector<detail::DrawBatch> batches;
		detail::BatchIndex index;
		ofxTestEq(detail::collectBatches(world.registry, batches, index), size_t(1), "reusable batch initial scene");
		world.clear();
		ofxTestEq(detail::collectBatches(world.registry, batches, index), size_t(0), "reusable batches skip stale slots after clear");
		auto c = world.create();
		world.registry.emplace<MeshRenderer>(c, mesh);
		world.update(0.f);
		auto count = detail::collectBatches(world.registry, batches, index);
		ofxTest(count == 1 && batches[0].items.size() == 1
			&& batches[0].items[0].renderer == &world.registry.get<MeshRenderer>(c)
			&& batches[0].items[0].world == &world.registry.get<WorldTransform>(c), "reused batches refresh component pointers");
	}

	void testVisibility() {
		World world;
		auto mesh = std::make_shared<ofVboMesh>(ofMesh::box(1, 1, 1));
		auto many = world.create(size_t(4));
		ofxTestEq(many.size(), size_t(4), "create(count)");
		ofxTestEq(world.size(), size_t(4), "size after create(count)");
		for (auto e : many) {
			world.registry.emplace<MeshRenderer>(e, mesh);
		}
		world.update(0.f);
		ofxTestEq(RenderSystem::collectBatches(world.registry).at(0).entities.size(), size_t(4), "all visible");

		world.setVisible(many[0], false);
		world.setHidden(many[1], true);
		world.update(0.f);
		ofxTest(!world.isVisible(many[0]), "MeshRenderer.visible false");
		ofxTest(world.isHidden(many[1]) && !world.isVisible(many[1]), "Hidden tag");
		ofxTestEq(RenderSystem::collectBatches(world.registry).at(0).entities.size(), size_t(2), "visible skips Hidden and visible=false");

		world.setVisible(many[0], true);
		world.setHidden(many[1], false);
		world.update(0.f);
		ofxTest(world.isVisible(many[0]) && world.isVisible(many[1]), "shown again");
		ofxTestEq(RenderSystem::collectBatches(world.registry).at(0).entities.size(), size_t(4), "all visible after show");

		auto quad = makeQuad(6);
		auto disk = makeDisk(4, 8);
		auto box = makeBox(2, 2, 2);
		ofxTest(quad && quad->getNumVertices() > 0, "makeQuad");
		ofxTest(disk && disk->getNumVertices() > 0, "makeDisk");
		ofxTest(box && box->getNumVertices() > 0, "makeBox");
	}

	void testClone() {
		World world;
		auto mesh = std::make_shared<ofVboMesh>(ofMesh::box(1, 1, 1));
		auto root = world.create("root");
		auto child = world.create("child");
		world.setParent(child, root);
		world.registry.emplace<MeshRenderer>(child, mesh);
		world.registry.get<Transform>(child).position = { 3, 0, 0 };
		auto copy = world.clone(root);
		ofxTest(copy != root && world.isValid(copy), "clone creates an entity");
		auto copyChildren = world.getChildren(copy);
		ofxTestEq(copyChildren.size(), size_t(1), "clone copies children");
		auto copyChild = copyChildren.at(0);
		ofxTest(world.registry.get<MeshRenderer>(copyChild).mesh == mesh, "clone shares the mesh");
		ofxTest(near(world.registry.get<Transform>(copyChild).position, { 3, 0, 0 }), "clone copies the Transform");
		ofxTestEq(world.size(), size_t(4), "clone count");
		world.update(0.f);
		ofxTestEq(RenderSystem::collectBatches(world.registry).at(0).entities.size(), size_t(2), "clones batch together");
	}

	void testJson() {
		World world;
		auto root = world.create("root");
		auto child = world.create("child");
		world.setParent(child, root);
		world.registry.get<Transform>(root).position = { 1, 2, 3 };
		world.registry.get<Transform>(child).setOrientationEulerDeg({ 0, 45, 0 });
		world.registry.emplace<Velocity>(child, glm::vec3(1, 0, 0), glm::vec3(0, 1, 0));
		world.registry.emplace<MeshRenderer>(child).color = ofFloatColor(1, 0, 0, 0.5f);
		world.registry.emplace<Hidden>(child);

		auto json = toJson(world);
		World loaded;
		auto entities = fromJson(loaded, json);
		ofxTestEq(entities.size(), size_t(2), "fromJson creates the entities");
		auto lroot = loaded.findByName("root");
		auto lchild = loaded.findByName("child");
		ofxTest(lroot != Null && lchild != Null, "names survive");
		ofxTest(loaded.getParent(lchild) == lroot, "parent survives");
		ofxTest(near(loaded.registry.get<Transform>(lroot).position, { 1, 2, 3 }), "position survives");
		ofxTest(near(loaded.registry.get<Transform>(lchild).getOrientationEulerDeg(), { 0, 45, 0 }, 1e-3f), "orientation survives");
		ofxTest(near(loaded.registry.get<Velocity>(lchild).angular, { 0, 1, 0 }), "velocity survives");
		ofxTest(loaded.registry.all_of<Hidden>(lchild), "hidden survives");
		ofxTest(loaded.registry.get<MeshRenderer>(lchild).color == ofFloatColor(1, 0, 0, 0.5f), "color survives");

		auto path = ofToDataPath("world.json", true);
		ofxTest(saveJson(world, path), "saveJson");
		World fromFile;
		ofxTestEq(loadJson(fromFile, path).size(), size_t(2), "loadJson");
		ofFile::removeFile(path);
	}

	void testParameters() {
		World world;
		auto e = world.create("thing");
		world.registry.emplace<MeshRenderer>(e);
		EntityParameters params;
		int changes = 0;
		auto listener = params.groupChanged.newListener([&]() { changes++; });
		params.setup(world);
		params.setEntity(e);
		ofxTest(params.group.contains("position") && params.group.contains("color"), "group has Transform and MeshRenderer");
		ofxTest(!params.group.contains("linear velocity"), "no Velocity parameters without the component");

		params.position = glm::vec3(5, 6, 7);
		ofxTest(near(world.registry.get<Transform>(e).position, { 5, 6, 7 }), "parameter writes the component");
		params.visible = false;
		ofxTest(world.registry.all_of<Hidden>(e), "visible=false adds Hidden");
		params.name = "renamed";
		ofxTestEq(world.registry.get<Name>(e).value, std::string("renamed"), "name writes Name");

		world.registry.get<Transform>(e).position = { 1, 1, 1 };
		params.pull();
		ofxTest(near(params.position.get(), { 1, 1, 1 }), "pull reads the component");

		int before = changes;
		world.registry.emplace<Velocity>(e);
		params.pull();
		ofxTest(params.group.contains("linear velocity") && changes == before + 1, "new component rebuilds the group");

		world.destroy(e);
		params.pull();
		ofxTestEq(params.group.size(), size_t(0), "destroyed entity empties the group");
	}
};

int main() {
	ofInit();
	auto window = std::make_shared<ofAppNoWindow>();
	auto app = std::make_shared<ofApp>();
	ofRunApp(window, app);
	return ofRunMainLoop();
}
