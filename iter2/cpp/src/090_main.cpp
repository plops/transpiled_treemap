// treemap_iter2_cpp: --scan prints one line, --bench compares serial vs
// parallel scan/layout. No GUI here; the PGE3 shell lives in 030_pge_app.hpp.
#include <chrono>
#include <cstdio>
#include <iostream>

// Angle brackets on purpose (see 030_pge_app.hpp): the USE_GENERATED
// build must resolve these via -I to cpp/gen/, not to this directory.
#include <010_scanner.hpp>
#include <015_color.hpp>
#include <020_layout.hpp>

namespace {

uintmax_t count_files(const treemap::Node& n)
{
    if (n.children.empty())
    {
        return n.isDir ? 0 : 1;
    }
    uintmax_t c = 0;
    for (const auto& ch: n.children)
    {
        c += count_files(ch);
    }
    return c;
}

int run_scan(const treemap::fs::path& target)
{
    const treemap::Node root = treemap::scan_tree_parallel(target);
    std::cout << root.path.string() << ": " << treemap::format_bytes(root.size) << " in "
              << count_files(root) << " files\n";
    return 0;
}

void bench_path(const treemap::fs::path& target)
{
    constexpr treemap::Rect canvas{0.0f, 0.0f, 1920.0f, 1044.0f};
    for (int round = 1; round <= 5; ++round)
    {
        const auto    t0       = std::chrono::steady_clock::now();
        treemap::Node s        = treemap::scan_tree(target);
        const double  tScanSer = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const auto    t1       = std::chrono::steady_clock::now();
        treemap::squarify(s.children, canvas);
        const double tLayoutSer = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();

        const auto    t2       = std::chrono::steady_clock::now();
        treemap::Node p        = treemap::scan_tree_parallel(target);
        const double  tScanPar = std::chrono::duration<double>(std::chrono::steady_clock::now() - t2).count();
        const auto    t3       = std::chrono::steady_clock::now();
        treemap::squarify_parallel(p.children, canvas);
        const double tLayoutPar = std::chrono::duration<double>(std::chrono::steady_clock::now() - t3).count();

        if (s.size != p.size)
        {
            std::cerr << "MISMATCH serial=" << s.size << " parallel=" << p.size << "\n";
            std::exit(1);
        }
        std::cerr << "[bench] round " << round << ": serial scan=" << tScanSer
                  << "s layout=" << tLayoutSer << "s | parallel scan=" << tScanPar
                  << "s layout=" << tLayoutPar
                  << "s | size=" << treemap::format_bytes(s.size)
                  << " scan_speedup=" << (tScanSer / std::max(tScanPar, 1e-9)) << "x\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string mode = "--scan";
    std::string dir  = ".";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--scan" || a == "--bench")
        {
            mode = a;
        }
        else
        {
            dir = a;
        }
    }
    if (mode == "--bench")
    {
        bench_path(dir);
        return 0;
    }
    return run_scan(dir);
}
