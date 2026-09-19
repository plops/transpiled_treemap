// PGE3 GUI shell for the parallel treemap. Needs olcPixelGameEngine3.h,
// which is NOT vendored here (absent in this container); it compiles where
// the header exists, e.g. next to pge_treemap's build. The parallel core
// (010/015/020) is PGE-free and covered by ctest.
//
// Wiring (mirrors pge_treemap/source0/src/090_main.cpp):
//   treemap::pge::TreemapApp app(targetDir);
//   PGEConfig config{...}; app.Construct(config); app.Start();
//
// Scan/layout run on a background std::thread (parallel core); the render
// thread only swaps a ready snapshot under a mutex, like SharedScanContext.
#pragma once

#ifdef TREEMAP_HAS_PGE3
#include "olcPixelGameEngine3.h"

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "010_scanner.hpp"
#include "015_color.hpp"
#include "020_layout.hpp"

namespace treemap::pge {

class TreemapApp : public PixelGameEngine {
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
            Rect canvas{0.0f, 36.0f, float(ScreenWidth()), float(ScreenHeight() - 36)};
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
        Clear(Pixel(20, 24, 30));
        if (!m_tree.children.empty()) {
            draw_tree(m_tree);
        } else {
            DrawString({14, 40}, "Scanning filesystem...", Pixel(160, 160, 160), 2);
        }
        DrawRect({0, 0}, {ScreenWidth(), 36}, Pixel(13, 15, 20));
        DrawString({14, 24}, m_tree.path.string() + ": " + format_bytes(m_tree.size),
                   Pixel::WHITE, 1);
        return !m_stop;
    }

  private:
    void draw_tree(const Node& n) {
        if (n.children.empty()) {
            const auto& c = n.color;
            FillRect({int(n.rect.x), int(n.rect.y)}, {int(n.rect.w), int(n.rect.h)},
                     Pixel(uint8_t(c.r * 255), uint8_t(c.g * 255), uint8_t(c.b * 255)));
        } else {
            for (const auto& ch : n.children) {
                draw_tree(ch);
            }
        }
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

#endif // TREEMAP_HAS_PGE3
