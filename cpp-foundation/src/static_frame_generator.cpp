#include "static_frame_generator.hpp"

namespace cortex {
namespace frames {

// PixelRGB implementations
PixelRGB::PixelRGB() : red("0"), green("0"), blue("0"), alpha("1") {}

PixelRGB::PixelRGB(const CosmicPrecision& r, const CosmicPrecision& g,
                   const CosmicPrecision& b, const CosmicPrecision& a)
    : red(r), green(g), blue(b), alpha(a) {}

// StaticFrame implementations
StaticFrame::StaticFrame() : width(0), height(0), total_energy("0"), spectrum_range("380-750nm") {}

void StaticFrame::reserve_pixels() {
    pixels.reserve(width * height);
}

PixelRGB& StaticFrame::get_pixel(int x, int y) {
    return pixels[y * width + x];
}

const PixelRGB& StaticFrame::get_pixel(int x, int y) const {
    return pixels[y * width + x];
}

// StaticFrameGenerator implementations
StaticFrameGenerator::StaticFrameGenerator()
    : term_delegator(std::thread::hardware_concurrency()),
      precision_threading(),
      overflow_guard(),
      gpu_delegator() {

    std::cout << "🖼️ Static Frame Generator initialized\n";
    std::cout << "   Using optimization foundations\n";
    std::cout << "   Electromagnetic spectrum processing ready\n";
}

StaticFrame StaticFrameGenerator::generate_em_spectrum_frame(int width, int height) {
    std::cout << "🌈 Generating electromagnetic spectrum frame: " << width << "x" << height << "\n";

    StaticFrame frame;
    frame.width = width;
    frame.height = height;
    frame.spectrum_range = "380-750nm visible light";
    frame.reserve_pixels();

    // Generate wavelength data for entire frame
    std::vector<CosmicPrecision> wavelength_data;
    wavelength_data.reserve(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Map x coordinate to wavelength (380-750nm visible spectrum)
            CosmicPrecision progress = CosmicPrecision(x) / CosmicPrecision(width);
            CosmicPrecision wavelength_nm = CosmicPrecision("380") +
                (CosmicPrecision("370") * progress);
            wavelength_data.push_back(wavelength_nm);
        }
    }

    // Use intelligent complexity detection (from your Python treasures!)
    auto complexity = term_delegator.detect_input_complexity(wavelength_data);

    // Delegate wavelength processing based on complexity (Python → C++)
    auto delegated_terms = term_delegator.delegate_terms(wavelength_data, complexity);

    // Process with zero-drift precision threading (Python → C++)
    auto [total_energy, thread_results] = precision_threading.precision_safe_map(
        [this](const CosmicPrecision& wavelength_nm) -> CosmicPrecision {
            return wavelength_to_rgb_intensity(wavelength_nm);
        },
        wavelength_data, 4
    );

    // Convert wavelength data to pixels
    for (size_t i = 0; i < wavelength_data.size(); ++i) {
        CosmicPrecision wavelength_nm = wavelength_data[i];
        PixelRGB pixel = wavelength_to_rgb_pixel(wavelength_nm);
        frame.pixels.push_back(pixel);
    }

    frame.total_energy = total_energy;

    std::cout << "✅ Electromagnetic spectrum frame generated\n";
    std::cout << "   Total energy: " << total_energy << "\n";
    std::cout << "   Pixels: " << frame.pixels.size() << "\n";

    return frame;
}

PixelRGB StaticFrameGenerator::wavelength_to_rgb_pixel(const CosmicPrecision& wavelength_nm) {
    PixelRGB pixel;

    // Visible light spectrum conversion (380-750nm)
    if (wavelength_nm >= CosmicPrecision("380") && wavelength_nm <= CosmicPrecision("450")) {
        // Violet to Blue
        pixel.blue = CosmicPrecision("1");
        pixel.red = (CosmicPrecision("450") - wavelength_nm) / CosmicPrecision("70");
    }
    else if (wavelength_nm <= CosmicPrecision("495")) {
        // Blue to Green
        pixel.blue = (CosmicPrecision("495") - wavelength_nm) / CosmicPrecision("45");
        pixel.green = (wavelength_nm - CosmicPrecision("450")) / CosmicPrecision("45");
    }
    else if (wavelength_nm <= CosmicPrecision("570")) {
        // Green to Yellow
        pixel.green = CosmicPrecision("1");
        pixel.red = (wavelength_nm - CosmicPrecision("495")) / CosmicPrecision("75");
    }
    else if (wavelength_nm <= CosmicPrecision("590")) {
        // Yellow to Orange
        pixel.red = CosmicPrecision("1");
        pixel.green = (CosmicPrecision("590") - wavelength_nm) / CosmicPrecision("20");
    }
    else if (wavelength_nm <= CosmicPrecision("750")) {
        // Orange to Red
        pixel.red = CosmicPrecision("1");
    }

    // Ensure alpha channel
    pixel.alpha = CosmicPrecision("1");

    return pixel;
}

CosmicPrecision StaticFrameGenerator::wavelength_to_rgb_intensity(const CosmicPrecision& wavelength_nm) {
    // Simple intensity calculation based on wavelength
    if (wavelength_nm >= CosmicPrecision("380") && wavelength_nm <= CosmicPrecision("750")) {
        // Peak intensity around 555nm (green)
        CosmicPrecision center = CosmicPrecision("555");
        CosmicPrecision distance = abs(wavelength_nm - center);
        CosmicPrecision max_distance = CosmicPrecision("185"); // Half of visible spectrum

        // Gaussian-like intensity distribution
        CosmicPrecision intensity = CosmicPrecision("1") - (distance / max_distance);
        return max(intensity, CosmicPrecision("0.1")); // Minimum 10% intensity
    }

    return CosmicPrecision("0"); // Outside visible spectrum
}

StaticFrame StaticFrameGenerator::generate_test_frame(int width, int height) {
    std::cout << "🧪 Generating test electromagnetic spectrum frame: " << width << "x" << height << "\n";

    StaticFrame frame;
    frame.width = width;
    frame.height = height;
    frame.spectrum_range = "380-750nm test spectrum";
    frame.reserve_pixels();

    // Simple test: horizontal wavelength gradient
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Map x to wavelength
            CosmicPrecision progress = CosmicPrecision(x) / CosmicPrecision(width);
            CosmicPrecision wavelength_nm = CosmicPrecision("380") +
                (CosmicPrecision("370") * progress);

            PixelRGB pixel = wavelength_to_rgb_pixel(wavelength_nm);
            frame.pixels.push_back(pixel);
        }
    }

    frame.total_energy = CosmicPrecision(frame.pixels.size());

    std::cout << "✅ Test frame generated with " << frame.pixels.size() << " pixels\n";
    return frame;
}

void StaticFrameGenerator::save_frame_data(const StaticFrame& frame, const std::string& filename) {
    std::cout << "💾 Saving frame data to: " << filename << "\n";
    std::cout << "   Frame size: " << frame.width << "x" << frame.height << "\n";
    std::cout << "   Spectrum range: " << frame.spectrum_range << "\n";
    std::cout << "   Total energy: " << frame.total_energy << "\n";
    std::cout << "   Pixel count: " << frame.pixels.size() << "\n";

    // Future: Implement actual file writing (PPM, PNG, etc.)
}

} // namespace frames
} // namespace cortex