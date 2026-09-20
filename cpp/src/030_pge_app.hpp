// PGE3 GUI shell for the parallel treemap core (010/015/020).
// API verified against the vendored header (cpp/third_party/).
// The scan/layout core stays PGE-free; this shell only draws snapshots.
#pragma once

#include "olcPixelGameEngine3.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "010_scanner.hpp"
#include "015_color.hpp"
#include "020_layout.hpp"

namespace treemap::pge {

inline olc::Pixel to_pixel(const Color& c) {
    return olc::Pixel(uint8_t(c.r * 255.0f), uint8_t(c.g * 255.0f), uint8_t(c.b * 255.0f),
                      uint8_t(c.a * 255.0f));
}

class TreemapApp : public olc::PixelGameEngine {
  public:
    explicit TreemapApp(fs::path target) : m_target(std::move(target)) {
        sAppName = "PGE3 - Treemap [" + m_target.string() + "]";
    }

    ~TreemapApp() override {
        m_stop = true;
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    bool OnUserCreate() override {
        m_worker = std::thread([this] {
            Node root = scan_tree_parallel(m_target);
            const olc::vi2d screen = ScreenSize();
            const Rect canvas{0.0f, 36.0f, float(screen.x), float(screen.y - 36)};
            root.rect = canvas;
            squarify_parallel(root.children, canvas);
            std::lock_guard lock(m_mutex);
            m_ready = std::move(root);
            m_hasNew = true;
        });
        return true;
    }

    bool OnUserUpdate(float /*elapsed*/) override {
        {
            std::lock_guard lock(m_mutex);
            if (m_hasNew) {
                m_tree = std::move(m_ready);
                m_hasNew = false;
            }
        }
        draw.Clear(olc::Pixel(20, 24, 30));
        if (!m_tree.children.empty()) {
            draw_tree(m_tree);
            const olc::vi2d mp = GetMouse().GetPosition();
            const Node* hov = find_hover(m_tree, float(mp.x), float(mp.y));
            std::string header = m_tree.path.string() + ": " + format_bytes(m_tree.size);
            if (hov != nullptr) {
                header = hov->path.string() + " (" + format_bytes(hov->size) + ")";
            }
            draw.FilledRect({0.0f, 0.0f}, {float(ScreenSize().x), 36.0f},
                            olc::Pixel(13, 15, 20));
            draw.String({14.0f, 12.0f}, header, olc::Colour::WHITE, {1.0f, 1.0f});
        } else {
            draw.String({14.0f, 40.0f}, "Scanning filesystem...", olc::Colour::GREY,
                        {2.0f, 2.0f});
        }
        return !m_stop;
    }

    // Pure hit-test helper (public for unit tests): deepest node wins.
    static const Node* find_hover(const Node& n, float x, float y) {
        if (x < n.rect.x || y < n.rect.y || x > n.rect.x + n.rect.w ||
            y > n.rect.y + n.rect.h) {
            return nullptr;
        }
        for (const auto& ch : n.children) {
            if (const Node* hov = find_hover(ch, x, y); hov != nullptr) {
                return hov;
            }
        }
        return &n;
    }

  private:
    void draw_tree(const Node& n) {
        if (n.children.empty()) {
            if (n.rect.w >= 1.0f && n.rect.h >= 1.0f) {
                draw.FilledRect({n.rect.x, n.rect.y}, {n.rect.w, n.rect.h}, to_pixel(n.color));
            }
            return;
        }
        for (const auto& ch : n.children) {
            draw_tree(ch);
        }
        draw.Rect({n.rect.x, n.rect.y}, {n.rect.w, n.rect.h}, olc::Pixel(0, 0, 0, 160));
    }

    fs::path m_target;
    std::thread m_worker;
    std::mutex m_mutex;
    Node m_tree;
    Node m_ready;
    bool m_hasNew = false;
    bool m_stop = false;
};

} // namespace treemap::pge
