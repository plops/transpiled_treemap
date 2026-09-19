// Parallel treemap core: scanner. Mirrors the Rust prototype's ownership
// discipline: threads work on owned subtrees, errors are reported once via
// report_skip (no exceptions on the hot path), joins are checked.
#pragma once

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
    fs::path path;
    uintmax_t size = 0;
    bool isDir = true;
    std::vector<Node> children;
    Rect rect;
    Color color;
};

inline std::mutex& GetStderrMutex() {
    static std::mutex m;
    return m;
}

inline void report_skip(const std::string& context, const fs::path& path,
                        const std::error_code& ec) {
    std::lock_guard lock(GetStderrMutex());
    std::cerr << "skip [" << context << "]: " << path.string() << ": " << ec.message()
              << "\n";
}

inline bool IsVirtualRoot(const fs::path& p) {
    const std::string s = p.string();
    return s.starts_with("/proc") || s.starts_with("/sys") || s.starts_with("/dev");
}

// Defined in 015_color.hpp (included after this header in the TU).
inline Color color_for_path(const fs::path& path);

// Serial reference scan.
inline void scan_entry(Node& node, const fs::directory_entry& entry);
inline Node scan_tree(const fs::path& path) {
    Node node;
    node.path = path;
    if (IsVirtualRoot(path)) {
        return node;
    }
    std::error_code ec;
    fs::directory_iterator it(path, ec);
    if (ec) {
        report_skip("read_dir", path, ec);
        return node;
    }
    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) {
            report_skip("entry", path, ec);
            break;
        }
        scan_entry(node, *it);
    }
    return node;
}

inline void scan_entry(Node& node, const fs::directory_entry& entry) {
    std::error_code ec;
    const fs::file_status st = entry.status(ec);
    if (ec) {
        report_skip("file_type", entry.path(), ec);
        return;
    }
    if (fs::is_symlink(entry.symlink_status(ec))) {
        return;
    }
    const fs::path p = entry.path();
    if (st.type() == fs::file_type::directory) {
        Node child = scan_tree(p);
        if (child.size > 0) {
            node.size += child.size;
            node.children.push_back(std::move(child));
        }
    } else if (st.type() == fs::file_type::regular) {
        const uintmax_t size = entry.file_size(ec);
        if (ec) {
            report_skip("metadata", p, ec);
            return;
        }
        if (size > 0 && size < (uintmax_t{1} << 48)) {
            Node child;
            child.path = p;
            child.size = size;
            child.isDir = false;
            child.color = color_for_path(p);
            node.size += size;
            node.children.push_back(std::move(child));
        }
    }
}

// Parallel scan: files inline, one thread per subdirectory (owned subtrees).
inline Node scan_tree_parallel(const fs::path& path) {
    Node node;
    node.path = path;
    if (IsVirtualRoot(path)) {
        return node;
    }
    std::vector<fs::path> subdirs;
    std::error_code ec;
    fs::directory_iterator it(path, ec);
    if (ec) {
        report_skip("read_dir", path, ec);
        return node;
    }
    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) {
            report_skip("entry", path, ec);
            break;
        }
        const fs::directory_entry& entry = *it;
        const fs::file_status st = entry.status(ec);
        if (ec) {
            report_skip("file_type", entry.path(), ec);
            continue;
        }
        if (fs::is_symlink(entry.symlink_status(ec))) {
            continue;
        }
        const fs::path p = entry.path();
        if (st.type() == fs::file_type::directory) {
            subdirs.push_back(p);
        } else if (st.type() == fs::file_type::regular) {
            const uintmax_t size = entry.file_size(ec);
            if (ec) {
                report_skip("metadata", p, ec);
                continue;
            }
            if (size > 0 && size < (uintmax_t{1} << 48)) {
                Node child;
                child.path = p;
                child.size = size;
                child.isDir = false;
                child.color = color_for_path(p);
                node.size += size;
                node.children.push_back(std::move(child));
            }
        }
    }

    std::vector<Node> results(subdirs.size());
    std::vector<std::thread> workers;
    workers.reserve(subdirs.size());
    for (size_t i = 0; i < subdirs.size(); ++i) {
        workers.emplace_back([&, i] {
            results[i] = scan_tree_parallel(subdirs[i]);
        });
    }
    for (auto& t : workers) {
        t.join();
    }
    for (auto& child : results) {
        if (child.size > 0) {
            node.size += child.size;
            node.children.push_back(std::move(child));
        }
    }
    return node;
}

} // namespace treemap
