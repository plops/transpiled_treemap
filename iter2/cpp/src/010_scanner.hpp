// Parallel treemap core, iteration 2: scanner. Mirrors the iter2 Rust
// prototype's ownership discipline: threads work on owned subtrees,
// errors are reported once via report_skip (no exceptions on the hot
// path), joins are checked.
//
// iter2 change vs iter1: entry paths are taken BY VALUE, never as
// `const fs::path&` into directory_iterator internals (dangling-handle
// hazard class from rust_cpp_vergleich.md, item 5).
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace treemap {

namespace fs = std::filesystem;

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct Color {
    float r = 0.15f;
    float g = 0.17f;
    float b = 0.22f;
    float a = 1.0f;
};

struct Node {
    fs::path          path;
    uintmax_t         size  = 0;
    bool              isDir = true;
    std::vector<Node> children;
    Rect              rect;
    Color             color;
};

inline std::mutex& GetStderrMutex()
{
    static std::mutex m;
    return m;
}

inline void report_skip(const std::string& context, const fs::path& path,
                        const std::error_code& ec)
{
    const std::lock_guard lock(GetStderrMutex());
    std::cerr << "skip [" << context << "]: " << path.string() << ": " << ec.message()
              << "\n";
}

inline bool IsVirtualRoot(const fs::path& p)
{
    const std::string s = p.string();
    return s.starts_with("/proc") || s.starts_with("/sys") || s.starts_with("/dev");
}

// Defined in 015_color.hpp (included after this header in the TU).
inline Color color_for_path(const fs::path& path);

// Serial reference scan.
inline void scan_entry(Node& node, const fs::directory_entry& entry);
inline Node scan_tree(const fs::path& path)
{
    Node node;
    node.path = path;
    if (IsVirtualRoot(path))
    {
        return node;
    }
    std::error_code        ec;
    fs::directory_iterator it(path, ec);
    if (ec)
    {
        report_skip("read_dir", path, ec);
        return node;
    }
    for (; it != fs::directory_iterator(); it.increment(ec))
    {
        if (ec)
        {
            report_skip("entry", path, ec);
            break;
        }
        scan_entry(node, *it);
    }
    return node;
}

inline void scan_entry(Node& node, const fs::directory_entry& entry)
{
    // d_type cache: answers from readdir, no extra stat (see parallel path).
    std::error_code ec;
    if (entry.is_symlink(ec))
    {
        return;
    }
    if (ec)
    {
        report_skip("file_type", entry.path(), ec);
        return;
    }
    // By value: entry.path() may reference iterator internals.
    const fs::path p     = entry.path();
    const bool     isdir = entry.is_directory(ec);
    if (ec)
    {
        report_skip("file_type", p, ec);
        return;
    }
    if (isdir)
    {
        Node child = scan_tree(p);
        if (child.size > 0)
        {
            node.size += child.size;
            node.children.push_back(std::move(child));
        }
        return;
    }
    const bool isfile = entry.is_regular_file(ec);
    if (ec)
    {
        report_skip("file_type", p, ec);
        return;
    }
    if (!isfile)
    {
        return;
    }
    const uintmax_t size = entry.file_size(ec);
    if (ec)
    {
        report_skip("metadata", p, ec);
        return;
    }
    if (size > 0 && size < (uintmax_t{1} << 48))
    {
        Node child;
        child.path  = p;
        child.size  = size;
        child.isDir = false;
        child.color = color_for_path(p);
        node.size += size;
        node.children.push_back(std::move(child));
    }
}

// Parallel scan: files inline, one thread per subdirectory (owned subtrees).
// Fan-out is capped: at most ~4x hardware threads scan concurrently, deeper
// levels recurse inline. Uncapped spawning drowned large trees in threads.
inline std::atomic<unsigned>& ScanWorkersActive()
{
    static std::atomic<unsigned> active{0};
    return active;
}

inline unsigned ScanWorkerLimit()
{
    const unsigned        hw    = std::thread::hardware_concurrency();
    static const unsigned limit = 4u * (hw == 0 ? 2u : std::max(2u, hw));
    return limit;
}

inline Node scan_tree_parallel_impl(const fs::path& path, int depth);

inline Node scan_tree_parallel(const fs::path& path)
{
    return scan_tree_parallel_impl(path, 0);
}

inline Node scan_tree_parallel_impl(const fs::path& path, int depth)
{
    Node node;
    node.path = path;
    if (IsVirtualRoot(path))
    {
        return node;
    }
    std::vector<fs::path>  subdirs;
    std::error_code        ec;
    fs::directory_iterator it(path, ec);
    if (ec)
    {
        report_skip("read_dir", path, ec);
        return node;
    }
    for (; it != fs::directory_iterator(); it.increment(ec))
    {
        if (ec)
        {
            report_skip("entry", path, ec);
            break;
        }
        const fs::directory_entry& entry = *it;
        // d_type cache: is_symlink/is_directory/is_regular_file answer from
        // readdir on most filesystems (no extra stat; symlinks are skipped
        // before any target stat, like Rust's file_type()).
        const bool islink = entry.is_symlink(ec);
        if (ec)
        {
            report_skip("file_type", entry.path(), ec);
            continue;
        }
        if (islink)
        {
            continue;
        }
        // By value: entry.path() may reference iterator internals.
        const fs::path p     = entry.path();
        const bool     isdir = entry.is_directory(ec);
        if (ec)
        {
            report_skip("file_type", p, ec);
            continue;
        }
        if (isdir)
        {
            subdirs.push_back(p);
            continue;
        }
        const bool isfile = entry.is_regular_file(ec);
        if (ec)
        {
            report_skip("file_type", p, ec);
            continue;
        }
        if (!isfile)
        {
            continue;
        }
        const uintmax_t size = entry.file_size(ec);
        if (ec)
        {
            report_skip("metadata", p, ec);
            continue;
        }
        if (size > 0 && size < (uintmax_t{1} << 48))
        {
            Node child;
            child.path  = p;
            child.size  = size;
            child.isDir = false;
            child.color = color_for_path(p);
            node.size += size;
            node.children.push_back(std::move(child));
        }
    }

    std::vector<Node> results(subdirs.size());
    bool              fanout = !subdirs.empty() && depth < 64;
    if (fanout)
    {
        const unsigned was = ScanWorkersActive().fetch_add(1);
        fanout             = was < ScanWorkerLimit();
        if (!fanout)
        {
            ScanWorkersActive().fetch_sub(1);
        }
    }
    if (fanout)
    {
        std::vector<std::thread> workers;
        workers.reserve(subdirs.size());
        for (size_t i = 0; i < subdirs.size(); ++i)
        {
            workers.emplace_back([&, i] {
                results[i] = scan_tree_parallel_impl(subdirs[i], depth + 1);
            });
        }
        for (auto& t: workers)
        {
            t.join();
        }
        ScanWorkersActive().fetch_sub(1);
    }
    else
    {
        for (size_t i = 0; i < subdirs.size(); ++i)
        {
            results[i] = scan_tree_parallel_impl(subdirs[i], depth + 1);
        }
    }
    for (auto& child: results)
    {
        if (child.size > 0)
        {
            node.size += child.size;
            node.children.push_back(std::move(child));
        }
    }
    return node;
}

} // namespace treemap
