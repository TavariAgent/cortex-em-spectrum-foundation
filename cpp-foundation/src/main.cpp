#include <fstream>
#include <algorithm>
#include "static_frame_generator.hpp"

int main() {
    cortex::StaticFrameGenerator gen;

    const size_t width = 40, height = 30;
    const size_t spp_x = 4, spp_y = 4; // 16 samples per pixel
    auto frame = gen.generate_supersampled_frame(width, height, spp_x, spp_y, true);

    std::ofstream ppm("electromagnetic_spectrum_supersampled.ppm");
    if (!ppm.is_open()) return 1;

    ppm << "P3\n" << frame.width << " " << frame.height << "\n255\n";
    for (const auto& p : frame.pixels) {
        int r = static_cast<int>(std::min(255.0, 255.0 * static_cast<double>(p.red)));
        int g = static_cast<int>(std::min(255.0, 255.0 * static_cast<double>(p.green)));
        int b = static_cast<int>(std::min(255.0, 255.0 * static_cast<double>(p.blue)));
        ppm << r << ' ' << g << ' ' << b << '\n';
    }
    return 0;
}