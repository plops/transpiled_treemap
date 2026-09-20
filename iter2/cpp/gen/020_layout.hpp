#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <vector> 
#include "010_scanner.hpp" 
namespace treemap {
            inline void squarify_parallel_depth (std::vector<Node>&, Rect, int); 
    
double worst_aspect (const std::vector<double>& areas, const std::vector<size_t>& row, double sum, float side)        {
                        auto s2 = (double) side*(double) side; 
        auto sum2 = sum*sum; 
        auto worst = sum2-sum2;  
                for ( auto& i : row ) {
                                    auto r1 = (s2*areas[i])/sum2; 
            auto r2 = sum2/(s2*areas[i]); 
            worst=std::max(worst, std::max(r1, r2)); 
};
                return worst;
}
 
    
void layout_row (std::vector<Node>& nodes, const std::vector<double>& areas, const std::vector<size_t>& row, double sum, Rect& r)        {
                        auto side = std::min(r.w, r.h); 
        auto thickness = (float) (sum/((double) side)); 
        auto isHoriz = r.w<r.h; 
        auto offset = isHoriz ? r.x : r.y;  
                for ( auto& i : row ) {
                                    auto itemLen = (float) (((areas[i])/sum)*(double) side); 
            if ( isHoriz ) {
                                nodes[i].rect={offset, r.y, itemLen, thickness};
} else {
                                nodes[i].rect={r.x, offset, thickness, itemLen};
} 
            offset+=itemLen; 
};
                if ( isHoriz ) {
                        {
                                r.y+=thickness;
                                r.h-=thickness;
} 
} else {
                        {
                                r.x+=thickness;
                                r.w-=thickness;
} 
} 
}
 
    
void squarify_level (std::vector<Node>& nodes, Rect rect)        {
                        auto total = uintmax_t{0}; 
        auto areaMult = 0.0f; 
        auto areas = std::vector<double>(); 
        auto row = std::vector<size_t>(); 
        auto rowSum = 0.0f;  
                for ( auto& n : nodes ) {
                        total+=n.size;
};
                if ( total==0||rect.w<=0.0f||rect.h<=0.0f ) {
                                    return ; 
} 
                std::sort(nodes.begin(), nodes.end(), [&](const Node& a, const Node& b) -> bool {
                        return b.size<a.size;
});
                areaMult=(((double) (rect.w*rect.h))/((double) total));
                areas.reserve(nodes.size());
                for ( auto& n : nodes ) {
                        areas.push_back((double) n.size*areaMult);
};
                for ( decltype(0+areas.size()+1) i = 0;i<areas.size();i+=1 ) {
                                    auto side = std::min(rect.w, rect.h); 
            auto nextRow = row;  
                        nextRow.push_back(i);
                        if ( row.empty()||worst_aspect(areas, nextRow, rowSum+areas[i], side)<=worst_aspect(areas, row, rowSum, side) ) {
                                {
                                        row.push_back(i);
                                        rowSum+=areas[i];
} 
} else {
                                {
                                        layout_row(nodes, areas, row, rowSum, rect);
                                        row={i};
                                        rowSum=areas[i];
} 
} 
} 
                if ( !row.empty() ) {
                                    layout_row(nodes, areas, row, rowSum, rect); 
} 
}
 
    
void squarify (std::vector<Node>& nodes, Rect rect)        {
                squarify_level(nodes, rect);
                for ( auto& node : nodes ) {
                        if ( node.isDir&&4.0f<node.rect.w&&4.0f<node.rect.h ) {
                                                squarify(node.children, node.rect); 
} 
};
}
 
    
bool needs_recursion (const Node& n)        {
                return n.isDir&&!n.children.empty()&&4.0f<n.rect.w&&4.0f<n.rect.h;
}
 
    
void layout_subtree_parallel (Node node, Node& out, int depth)        {
                if ( needs_recursion(node) ) {
                                    squarify_parallel_depth(node.children, node.rect, depth+1); 
} 
                out=std::move(node);
}
 
    
void squarify_parallel (std::vector<Node>& nodes, Rect rect)        {
                squarify_parallel_depth(nodes, rect, 0);
}
 
    
void squarify_parallel_depth (std::vector<Node>& nodes, Rect rect, int depth)        {
                squarify_level(nodes, rect);
                if ( 0<depth ) {
                                    {
                                for ( auto& node : nodes ) {
                                        if ( needs_recursion(node) ) {
                                                                        squarify_parallel_depth(node.children, node.rect, depth+1); 
} 
};
                                return ;
}  
} 
                        auto taken = std::vector<Node>(); 
        auto dirIdx = std::vector<size_t>();  
                taken.swap(nodes);
                for ( decltype(0+taken.size()+1) i = 0;i<taken.size();i+=1 ) {
                        if ( needs_recursion(taken[i]) ) {
                                                dirIdx.push_back(i); 
} 
} 
                std::vector<std::thread> workers; 
                workers.reserve (dirIdx.size ()); 
                for (size_t k = 0; k < dirIdx.size (); ++k) {
                workers.emplace_back ([&, k] { Node done; layout_subtree_parallel (std::move (taken[dirIdx[k]]), done, depth); taken[dirIdx[k]] = std::move (done); }); 
                }
                for (auto& t : workers) { t.join (); }
                std::sort(taken.begin(), taken.end(), [&](const Node& a, const Node& b) -> bool {
                        return b.size<a.size;
});
                nodes=std::move(taken);
}
  
} 