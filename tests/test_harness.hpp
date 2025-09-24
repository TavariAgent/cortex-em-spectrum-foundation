#pragma once
#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <cstdlib>

namespace mini_test {

    struct TestCase {
        std::string name;
        std::function<void()> fn;
    };

    inline std::vector<TestCase>& registry() {
        static std::vector<TestCase> R;
        return R;
    }

    struct Registrar {
        Registrar(const char* name, std::function<void()> fn) {
            registry().push_back({name, std::move(fn)});
        }
    };

#define TEST_CASE(name) \
static void testfn_##name(); \
static ::mini_test::Registrar reg_##name(#name, testfn_##name); \
static void testfn_##name()

#define CHECK(cond) do { \
if (!(cond)) { \
std::cerr << "[CHECK FAILED] " << __FILE__ << ":" << __LINE__ \
<< " -> " #cond << std::endl; \
} \
} while (0)

#define REQUIRE(cond) do { \
if (!(cond)) { \
std::cerr << "[REQUIRE FAILED] " << __FILE__ << ":" << __LINE__ \
<< " -> " #cond << std::endl; \
std::exit(1); \
} \
} while (0)

    inline int run_all() {
        int failed = 0;
        std::cout << "Running " << registry().size() << " test(s)..." << std::endl;
        for (auto& t : registry()) {
            try {
                t.fn();
                std::cout << "  [OK] " << t.name << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "  [EXCEPTION] " << t.name << " -> " << e.what() << std::endl;
                ++failed;
            } catch (...) {
                std::cerr << "  [EXCEPTION] " << t.name << " -> unknown" << std::endl;
                ++failed;
            }
        }
        if (failed) {
            std::cerr << failed << " test(s) failed." << std::endl;
            return 1;
        }
        std::cout << "All tests passed." << std::endl;
        return 0;
    }

} // namespace mini_test