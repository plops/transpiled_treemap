// Extension palette + deterministic hash fallback. Mirrors the iter2 Rust
// color_for_path (same groups, same green/blue/purple/red families).
#pragma once

#include <string>

#include "010_scanner.hpp"

namespace treemap {

inline Color hash_color(const fs::path& path)
{
    const std::string name = path.filename().string();
    uint32_t          h    = 0;
    for (const unsigned char b: name)
    {
        h += b;
    }
    auto ch = [&](uint32_t m) -> float {
        return float((h * m) % 160 + 80) / 255.0f;
    };
    return Color{ch(37), ch(59), ch(83), 1.0f};
}

inline Color color_for_path(const fs::path& path)
{
    const std::string ext = path.extension().string();
    if (ext == ".rs" || ext == ".c" || ext == ".cpp" || ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".txt" || ext == ".md")
    {
        return Color{0.2f, 0.75f, 0.45f, 1.0f};
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".svg" || ext == ".webp")
    {
        return Color{0.2f, 0.65f, 0.95f, 1.0f};
    }
    if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".mp3" || ext == ".flac")
    {
        return Color{0.75f, 0.35f, 0.85f, 1.0f};
    }
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".7z")
    {
        return Color{0.9f, 0.3f, 0.25f, 1.0f};
    }
    return hash_color(path);
}

inline std::string format_bytes(uintmax_t b)
{
    static const char* const units[] = {"B", "KB", "MB", "GB", "TB"};
    auto                     d       = double(b);
    int                      i       = 0;
    while (d >= 1024.0 && i < 4)
    {
        d /= 1024.0;
        ++i;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %s", d, units[i]);
    return buf;
}

} // namespace treemap
