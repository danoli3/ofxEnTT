#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
std::vector<float> read(const char * path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    auto size = stream.tellg();
    if (!stream || size < 0 || size % sizeof(float)) throw 1;
    std::vector<float> values(static_cast<size_t>(size) / sizeof(float));
    stream.seekg(0); stream.read(reinterpret_cast<char *>(values.data()), size);
    if (!stream) throw 2;
    return values;
}
int main(int argc, char ** argv) {
    if (argc != 3) return 2;
    auto a = read(argv[1]), b = read(argv[2]);
    if (a.size() != b.size()) return 3;
    size_t failures = 0;
    double absolute = 0, scaled = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        auto delta = std::abs(double(a[i]) - b[i]);
        auto relative = delta / std::max(1.0, std::abs(double(a[i])));
        absolute = std::max(absolute, delta); scaled = std::max(scaled, relative);
        if (!std::isfinite(a[i]) || !std::isfinite(b[i]) || relative > 1e-4) ++failures;
    }
    std::cout << std::setprecision(17) << "{\"floats\":" << a.size() << ",\"failures\":" << failures
        << ",\"max_absolute_error\":" << absolute << ",\"max_scaled_error\":" << scaled << "}\n";
    return failures ? 1 : 0;
}
