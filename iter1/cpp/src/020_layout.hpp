// Squarified treemap layout (Bruls, Huizing, van Wijk), serial reference +
// parallel driver. Parallelism mirrors the Rust prototype: serial row pass,
// then one thread per owned subdirectory subtree; size order restored after
// join because threads rejoin out of order.
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

#include "010_scanner.hpp"

namespace treemap {

// Parameter order mirrors the Rust reference fn worst(areas, row, sum, side).
inline double worst_aspect(const std::vector<double>& areas, const std::vector<size_t>& row,
                           double sum, float side) // NOLINT(bugprone-easily-swappable-parameters)
{
    const double s2    = double(side) * double(side);
    const double sum2  = sum * sum;
    double       worst = 0.0;
    for (const size_t i: row)
    {
        const double r1 = (s2 * areas[i]) / sum2;
        const double r2 = sum2 / (s2 * areas[i]);
        worst           = std::max(worst, std::max(r1, r2));
    }
    return worst;
}

inline void layout_row(std::vector<Node>& nodes, const std::vector<double>& areas,
                       const std::vector<size_t>& row, double sum, Rect& r)
{
    const float side      = std::min(r.w, r.h);
    const auto  thickness = float(sum / double(side));
    const bool  isHoriz   = r.w < r.h;
    float       offset    = isHoriz ? r.x : r.y;

    for (const size_t i: row)
    {
        const auto itemLen = float(areas[i] / sum * double(side));
        nodes[i].rect      = isHoriz ? Rect{offset, r.y, itemLen, thickness} : Rect{r.x, offset, thickness, itemLen};
        offset += itemLen;
    }
    if (isHoriz)
    {
        r.y += thickness;
        r.h -= thickness;
    }
    else
    {
        r.x += thickness;
        r.w -= thickness;
    }
}

inline void squarify_level(std::vector<Node>& nodes, Rect rect)
{
    uintmax_t total = 0;
    for (const auto& n: nodes)
    {
        total += n.size;
    }
    if (total == 0 || rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }
    std::sort(nodes.begin(), nodes.end(),
              [](const Node& a, const Node& b) { return a.size > b.size; });
    const double        areaMult = double(rect.w * rect.h) / double(total);
    std::vector<double> areas;
    areas.reserve(nodes.size());
    for (const auto& n: nodes)
    {
        areas.push_back(double(n.size) * areaMult);
    }

    std::vector<size_t> row;
    double              rowSum = 0.0;
    for (size_t i = 0; i < areas.size(); ++i)
    {
        const float         side    = std::min(rect.w, rect.h);
        std::vector<size_t> nextRow = row;
        nextRow.push_back(i);
        if (row.empty() || worst_aspect(areas, nextRow, rowSum + areas[i], side) <= worst_aspect(areas, row, rowSum, side))
        {
            row.push_back(i);
            rowSum += areas[i];
        }
        else
        {
            layout_row(nodes, areas, row, rowSum, rect);
            row    = {i};
            rowSum = areas[i];
        }
    }
    if (!row.empty())
    {
        layout_row(nodes, areas, row, rowSum, rect);
    }
}

inline void squarify(std::vector<Node>& nodes, Rect rect)
{
    squarify_level(nodes, rect);
    for (auto& node: nodes)
    {
        if (node.isDir && node.rect.w > 4.0f && node.rect.h > 4.0f)
        {
            squarify(node.children, node.rect);
        }
    }
}

inline void layout_subtree_parallel(Node node, Node& out, int depth);

inline bool needs_recursion(const Node& n)
{
    return n.isDir && !n.children.empty() && n.rect.w > 4.0f && n.rect.h > 4.0f;
}

inline void squarify_parallel_depth(std::vector<Node>& nodes, Rect rect, int depth);

inline void squarify_parallel(std::vector<Node>& nodes, Rect rect)
{
    squarify_parallel_depth(nodes, rect, 0);
}

// Fan-out only at the top level: unbounded per-level threads cost more
// than they saved on large trees (bench: 5x slower than serial).
inline void squarify_parallel_depth(std::vector<Node>& nodes, Rect rect, int depth)
{
    squarify_level(nodes, rect);
    if (depth > 0)
    {
        for (auto& node: nodes)
        {
            if (needs_recursion(node))
            {
                squarify_parallel_depth(node.children, node.rect, depth + 1);
            }
        }
        return;
    }
    std::vector<Node> taken;
    taken.swap(nodes);
    // Leaves need no recursion: only subtrees get a thread. Disjoint indices
    // make the join mutex-free; order is restored by sort afterwards.
    std::vector<size_t> dirIdx;
    for (size_t i = 0; i < taken.size(); ++i)
    {
        if (needs_recursion(taken[i]))
        {
            dirIdx.push_back(i);
        }
    }
    std::vector<std::thread> workers;
    workers.reserve(dirIdx.size());
    // Index remap needs k; threads write disjoint slots, joined before reuse.
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (size_t k = 0; k < dirIdx.size(); ++k)
    {
        workers.emplace_back([&, k] {
            Node done;
            layout_subtree_parallel(std::move(taken[dirIdx[k]]), done, depth);
            taken[dirIdx[k]] = std::move(done);
        });
    }
    for (auto& t: workers)
    {
        t.join();
    }
    // Threads finish out of order: restore size order (rects travel along).
    std::sort(taken.begin(), taken.end(),
              [](const Node& a, const Node& b) { return a.size > b.size; });
    nodes = std::move(taken);
}

inline void layout_subtree_parallel(Node node, Node& out, int depth)
{
    if (node.isDir && !node.children.empty() && node.rect.w > 4.0f && node.rect.h > 4.0f)
    {
        squarify_parallel_depth(node.children, node.rect, depth + 1);
    }
    out = std::move(node);
}

} // namespace treemap
