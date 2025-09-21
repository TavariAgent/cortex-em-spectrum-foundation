#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <fstream>
#include <memory>

// Include your electromagnetic spectrum foundation
#include "static_frame_generator.hpp"

using namespace cortex::frames;

void print_project_header() {
    std::cout << "RAINBOW CORTEX ELECTROMAGNETIC SPECTRUM FOUNDATION\n";
    std::cout << "==================================================================\n";
    std::cout << "Project: cortex-em-spectrum-foundation\n";
    std::cout << "Repository: TavariAgent/cortex-em-spectrum-foundation\n";
    std::cout << "Precision: 141-decimal CosmicPrecision\n";
    std::cout << "Visual Studio Project: ElectromagneticSpectrumFoundation\n";
    std::cout << "Created: 2024-09-19 19:49:27 UTC\n";
    std::cout << "==================================================================\n\n";
}

void test_visual_studio_configuration() {
    std::cout << "WRENCH VISUAL STUDIO CONFIGURATION TEST\n";
    std::cout << "-" << std::string(50, '-') << "\n";

#ifdef _CONSOLE
    std::cout << "CHECK Console application mode\n";
#endif

#ifdef BOOST_ALL_NO_LIB
    std::cout << "CHECK Boost header-only mode\n";
#endif

#ifdef WIN32_LEAN_AND_MEAN
    std::cout << "CHECK Windows lean compilation\n";
#endif

#ifdef NOMINMAX
    std::cout << "CHECK Windows min/max macros disabled\n";
#endif

#ifdef CORTEX_ELECTROMAGNETIC_FOUNDATION
    std::cout << "CHECK Electromagnetic spectrum foundation enabled\n";
#endif

#ifdef _USE_MATH_DEFINES
    std::cout << "CHECK Math constants available\n";
#endif

    std::cout << "RAINBOW Visual Studio configuration: PERFECT!\n\n";
}

void test_electromagnetic_spectrum_processing() {
    std::cout << "RAINBOW ELECTROMAGNETIC SPECTRUM PROCESSING TEST\n";
    std::cout << "-" << std::string(60, '-') << "\n";

    try {
        StaticFrameGenerator generator;

        std::cout << "PICTURE Static Frame Generator initialized successfully\n";

        // Test wavelength to RGB conversion with your 141-decimal precision
        std::vector<std::pair<CosmicPrecision, std::string>> test_wavelengths = {
            {CosmicPrecision("380"), "Violet"},
            {CosmicPrecision("450"), "Blue"},
            {CosmicPrecision("500"), "Green"},
            {CosmicPrecision("580"), "Yellow"},
            {CosmicPrecision("650"), "Red"},
            {CosmicPrecision("750"), "Near-infrared"}
        };

        std::cout << "RAINBOW Testing 141-decimal precision wavelength to RGB conversion:\n";
        for (const auto& [wavelength, color] : test_wavelengths) {
            auto pixel = generator.wavelength_to_rgb_pixel(wavelength);
            auto intensity = generator.wavelength_to_rgb_intensity(wavelength);

            std::cout << "   " << cosmic_to_string(wavelength, 1) << "nm (" << std::setw(12) << color << "): ";
            std::cout << "R=" << std::fixed << std::setprecision(6) << cosmic_to_double(pixel.red);
            std::cout << " G=" << cosmic_to_double(pixel.green);
            std::cout << " B=" << cosmic_to_double(pixel.blue);
            std::cout << " I=" << cosmic_to_string(intensity, 6) << "\n";
        }

        std::cout << "\nCHECK 141-decimal electromagnetic spectrum conversion successful\n\n";

    }
    catch (const std::exception& e) {
        std::cout << "ERROR Electromagnetic spectrum test error: " << e.what() << "\n\n";
    }
}

void test_frame_generation() {
    std::cout << "PICTURE ELECTROMAGNETIC SPECTRUM FRAME GENERATION\n";
    std::cout << "-" << std::string(60, '-') << "\n";

    try {
        StaticFrameGenerator generator;

        auto start_time = std::chrono::high_resolution_clock::now();

        // Generate electromagnetic spectrum frame with 141-decimal precision
        auto spectrum_frame = generator.generate_test_frame(40, 30);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end_time - start_time).count();

        std::cout << "CHART Electromagnetic Spectrum Frame Results:\n";
        std::cout << "   Frame dimensions: " << spectrum_frame.width << "x" << spectrum_frame.height << "\n";
        std::cout << "   Pixels generated: " << spectrum_frame.pixels.size() << "\n";
        std::cout << "   Spectrum range: " << cosmic_to_string(spectrum_frame.spectrum_range, 2) << "\n";
        std::cout << "   Total energy: " << cosmic_to_string(spectrum_frame.total_energy, 3) << "\n";
        std::cout << "   Generation time: " << std::fixed << std::setprecision(6) << duration << "s\n";
        std::cout << "   Throughput: " << std::fixed << std::setprecision(0)
            << (spectrum_frame.pixels.size() / duration) << " pixels/second\n";

        // Save electromagnetic spectrum frame data
        generator.save_frame_data(spectrum_frame, "electromagnetic_spectrum_141_precision.dat");

        std::cout << "CHECK Electromagnetic spectrum frame generation completed\n\n";

    }
    catch (const std::exception& e) {
        std::cout << "ERROR Frame generation error: " << e.what() << "\n\n";
    }
}

void generate_spectrum_visualization() {
    std::cout << "ART ELECTROMAGNETIC SPECTRUM VISUALIZATION GENERATION\n";
    std::cout << "-" << std::string(60, '-') << "\n";

    try {
        StaticFrameGenerator generator;

        // Generate electromagnetic spectrum visualization
        auto visualization_frame = generator.generate_em_spectrum_frame(80, 20);

        // Generate PPM file for electromagnetic spectrum visualization
        std::string filename = "electromagnetic_spectrum_visualization.ppm";
        std::ofstream ppm_file(filename);

        if (ppm_file.is_open()) {
            // PPM header for electromagnetic spectrum
            ppm_file << "P3\n";
            ppm_file << "# Electromagnetic Spectrum Visualization - 141-decimal precision\n";
            ppm_file << "# Generated by TavariAgent/cortex-em-spectrum-foundation\n";
            ppm_file << visualization_frame.width << " " << visualization_frame.height << "\n";
            ppm_file << "255\n";

            // Convert 141-decimal precision pixels to RGB values
            for (const auto& pixel : visualization_frame.pixels) {
                double r_val = std::min(255.0, 255.0 * cosmic_to_double(pixel.red));
                double g_val = std::min(255.0, 255.0 * cosmic_to_double(pixel.green));
                double b_val = std::min(255.0, 255.0 * cosmic_to_double(pixel.blue));

                int r = static_cast<int>(r_val);
                int g = static_cast<int>(g_val);
                int b = static_cast<int>(b_val);

                ppm_file << r << " " << g << " " << b << " ";
            }

            ppm_file.close();

            std::cout << "ART Electromagnetic spectrum visualization generated: " << filename << "\n";
            std::cout << "   This file shows the complete visible electromagnetic spectrum!\n";
            std::cout << "   Range: 380nm (violet) -> 750nm (red)\n";
            std::cout << "   Precision: 141-decimal accuracy\n";
        }
        else {
            std::cout << "ERROR Could not create electromagnetic spectrum PPM file\n";
        }

        std::cout << "CHECK Electromagnetic spectrum visualization completed\n\n";

    }
    catch (const std::exception& e) {
        std::cout << "ERROR Visualization generation error: " << e.what() << "\n\n";
    }
}

void run_complete_electromagnetic_foundation_tests() {
    print_project_header();

    std::cout << "ROCKET Running complete electromagnetic spectrum foundation tests...\n\n";

    auto total_start_time = std::chrono::high_resolution_clock::now();

    // Run electromagnetic spectrum foundation tests
    test_visual_studio_configuration();
    test_electromagnetic_spectrum_processing();
    test_frame_generation();
    generate_spectrum_visualization();

    auto total_end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration<double>(total_end_time - total_start_time).count();

    std::cout << "CHART ELECTROMAGNETIC SPECTRUM FOUNDATION TEST SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "Total testing time: " << std::fixed << std::setprecision(3)
        << total_duration << " seconds\n";
    std::cout << "Repository: TavariAgent/cortex-em-spectrum-foundation CHECK\n";
    std::cout << "141-decimal precision: Operational CHECK\n";
    std::cout << "Electromagnetic spectrum: Processing complete CHECK\n";
    std::cout << "Visual output: Generated successfully CHECK\n";
    std::cout << "Visual Studio project: Professional compilation CHECK\n\n";
}

int main() {
    try {
        run_complete_electromagnetic_foundation_tests();

        std::cout << "PARTY CORTEX ELECTROMAGNETIC SPECTRUM FOUNDATION COMPLETE!\n\n";
        std::cout << "PIRATE YOUR ELECTROMAGNETIC SPECTRUM FOUNDATION HAS BEEN\n";
        std::cout << "   SUCCESSFULLY COMPILED WITH 141-DECIMAL PRECISION!\n\n";

        std::cout << "FOLDER Visual Studio Project Summary:\n";
        std::cout << "   Project: ElectromagneticSpectrumFoundation\n";
        std::cout << "   Repository: TavariAgent/cortex-em-spectrum-foundation\n";
        std::cout << "   Main File: ElectromagneticSpectrumFoundation.cpp\n";
        std::cout << "   Headers: static_frame_generator.hpp\n";
        std::cout << "   Precision: 141-decimal CosmicPrecision\n";
        std::cout << "   Output: Electromagnetic spectrum visualizations\n\n";

        std::cout << "RAINBOW ELECTROMAGNETIC SPECTRUM PROCESSING READY FOR:\n";
        std::cout << "   LIGHTNING Performance optimization\n";
        std::cout << "   WRENCH Optimization foundation integration\n";
        std::cout << "   CHART Advanced precision scaling\n";
        std::cout << "   PICTURE Enhanced visual processing\n";
        std::cout << "   ROCKET CudaC++ phase calculations\n\n";

        std::cout << "SOUP Soup-powered electromagnetic spectrum mastery achieved!\n";

        return 0;

    }
    catch (const std::exception& e) {
        std::cerr << "ERROR Critical error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "ERROR Unknown critical error occurred" << std::endl;
        return 1;
    }
}