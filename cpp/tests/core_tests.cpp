// Equivalence + edge tests for the C++ parallel core (no PGE needed).
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "010_scanner.hpp"
#include "015_color.hpp"
#include "020_layout.hpp"
#include "030_pge_app.hpp"

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& p, size_t n) {
    std::ofstream o(p, std::ios::binary);
    for (size_t i = 0; i < n; ++i) {
        o.put('x');
    }
}

fs::path fixture() {
    const fs::path base = fs::temp_directory_path() / "treemap_cpp_eq";
    fs::remove_all(base);
    fs::create_directories(base / "sub");
    write_file(base / "a.bin", 100);
    write_file(base / "sub" / "b.bin", 200);
    write_file(base / "empty", 0);
    return base;
}

void check(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        std::exit(1);
    }
    std::cout << "ok: " << what << "\n";
}

} // namespace

int main() {
    const fs::path base = fixture();
    const treemap::Node s = treemap::scan_tree(base);
    const treemap::Node p = treemap::scan_tree_parallel(base);
    check(s.size == 300, "serial size 300");
    check(p.size == 300, "parallel size 300");
    check(s.children.size() == p.children.size(), "child count equal");

    constexpr treemap::Rect canvas{0.0f, 0.0f, 800.0f, 600.0f};
    treemap::Node sl = treemap::scan_tree(base);
    treemap::Node pl = treemap::scan_tree_parallel(base);
    treemap::squarify(sl.children, canvas);
    treemap::squarify_parallel(pl.children, canvas);
    for (const treemap::Node* root : {&sl, &pl}) {
        float area = 0.0f;
        for (const auto& n : root->children) {
            area += n.rect.w * n.rect.h;
        }
        check(std::abs(area - 800.0f * 600.0f) / (800.0f * 600.0f) < 0.005f, "area preserved");
        for (size_t i = 1; i < root->children.size(); ++i) {
            check(root->children[i - 1].size >= root->children[i].size, "size sorted");
        }
    }

    const treemap::Node missing = treemap::scan_tree_parallel("/nonexistent-treemap-dir-xyz");
    check(missing.size == 0 && missing.children.empty(), "missing dir empty, no crash");

    check(treemap::format_bytes(2048) == "2.0 KB", "format_bytes");

    // Hover hit-test (030 shell): root rect assigned, deepest child wins.
    // Regression: root.rect was never set, so every hover missed.
    {
        treemap::Node root;
        root.rect = {0.0f, 36.0f, 800.0f, 564.0f};
        treemap::Node a;
        a.path = "a";
        a.rect = {0.0f, 36.0f, 400.0f, 564.0f};
        a.size = 1;
        treemap::Node b;
        b.path = "b";
        b.rect = {400.0f, 36.0f, 400.0f, 564.0f};
        b.size = 1;
        root.children.push_back(a);
        root.children.push_back(b);
        const treemap::Node* h1 = treemap::pge::TreemapApp::find_hover(root, 600.0f, 300.0f);
        check(h1 != nullptr && h1->path == "b", "hover hits right child");
        const treemap::Node* h2 = treemap::pge::TreemapApp::find_hover(root, 10.0f, 10.0f);
        check(h2 == nullptr, "hover outside root misses");
        const treemap::Node* h3 = treemap::pge::TreemapApp::find_hover(root, 100.0f, 100.0f);
        check(h3 != nullptr && h3->path == "a", "hover hits left child");
    }

    fs::remove_all(base);
    std::cout << "all c++ core tests passed\n";
    return 0;
}
