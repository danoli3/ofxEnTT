#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofxUnitTests.h"
#include "ofxEnTT.h"

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
		testSystems();
		testBatches();
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
