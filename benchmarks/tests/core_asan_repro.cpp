#include "ofMain.h"
#include "ofAppNoWindow.h"
int main() {
    std::vector<glm::vec3> vertices;
    for (int i=0; i<20; ++i) vertices.emplace_back(float(i));
    vertices.insert(vertices.end(), {glm::vec3(1), glm::vec3(2), glm::vec3(3)});
    auto window = std::make_shared<ofAppNoWindow>();
    return vertices.size() == 23 ? 0 : 1;
}
