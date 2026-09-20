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
                float x; 
                float y; 
                float w; 
                float h; 
}; ;
        struct Color {
                float r; 
                float g; 
                float b; 
                float a; 
}; ;
        struct Node {
                std::filesystem::path path; 
                uintmax_t size; 
                bool isDir; 
                std::vector<Node> children; 
                Rect rect; 
                Color color; 
}; ; 
            inline std::mutex& GetStderrMutex () { static std::mutex m; return m; }
    
void report_skip (const std::string& context, const fs::path& path, const std::error_code& ec)        {
                std::lock_guard lock (GetStderrMutex ()); 
                std::cerr<<"skip ["<<context<<"]: "<<path.string ()<<": "<<ec.message()<<std::endl;
}
 
    
bool IsVirtualRoot (const fs::path& p)        {
                        const std::string s = p.string (); 
        return s.starts_with("/proc")||s.starts_with("/sys")||s.starts_with("/dev"); 
}
 
    inline Node scan_tree (const fs::path&); 
    inline Node scan_tree_parallel_impl (const fs::path&, int); 
    inline std::atomic<unsigned>& ScanWorkersActive () { static std::atomic<unsigned> active{0}; return active; }
    inline unsigned ScanWorkerLimit () { const unsigned hw = std::thread::hardware_concurrency (); static const unsigned limit = 4u * (hw == 0 ? 2u : std::max (2u, hw)); return limit; }
    inline Color color_for_path (const fs::path&);  
            
void scan_entry (Node& node, const fs::directory_entry& entry)        {
                std::error_code ec; 
                if ( entry.is_symlink(ec) ) {
                                    return ; 
} 
                if ( ec ) {
                                    {
                                report_skip("file_type", entry.path(), ec);
                                return ;
}  
} 
                        const fs::path p = entry.path(); 
        if ( entry.is_directory(ec) ) {
                                    {
                                if ( ec ) {
                                                            {
                                                report_skip("file_type", p, ec);
                                                return ;
}  
} 
                                                auto child = scan_tree(p); 
                if ( 0<child.size ) {
                                                            node.size+=child.size;
                    node.children.push_back(std::move(child)); 
}  
                                return ;
}  
} 
        if ( ec ) {
                                    {
                                report_skip("file_type", p, ec);
                                return ;
}  
} 
        if ( !entry.is_regular_file(ec) ) {
                                    return ; 
} 
        if ( ec ) {
                                    {
                                report_skip("file_type", p, ec);
                                return ;
}  
} 
                auto size = entry.file_size(ec); 
        if ( ec ) {
                                    {
                                report_skip("metadata", p, ec);
                                return ;
}  
} 
        if ( 0<size&&size<uintmax_t{1}<<48 ) {
                                                auto child = Node(); 
            child.path=p;
            child.size=size;
            child.isDir=false;
            child.color=color_for_path(p);
            node.size+=size;
            node.children.push_back(std::move(child));  
}   
}
 
    
Node scan_tree (const fs::path& path)        {
                        auto node = Node(); 
        node.path=path;
        node.isDir=true;
        if ( IsVirtualRoot(path) ) {
                                    return node; 
} 
        std::error_code ec; 
        fs::directory_iterator it (path, ec); 
        if ( ec ) {
                                    {
                                report_skip("read_dir", path, ec);
                                return node;
}  
} 
        for (; it != fs::directory_iterator (); it.increment (ec)) {
        if (ec) { report_skip ("entry", path, ec); break; }
        scan_entry(node, *it);
        }
        return node; 
}
  
            
Node scan_tree_parallel (const fs::path& path)        {
                return scan_tree_parallel_impl(path, 0);
}
 
    
Node scan_tree_parallel_impl (const fs::path& path, int depth)        {
                        auto node = Node(); 
        auto subdirs = std::vector<fs::path>(); 
        node.path=path;
        node.isDir=true;
        if ( IsVirtualRoot(path) ) {
                                    return node; 
} 
        std::error_code ec; 
        fs::directory_iterator it (path, ec); 
        if ( ec ) {
                                    {
                                report_skip("read_dir", path, ec);
                                return node;
}  
} 
        for (; it != fs::directory_iterator (); it.increment (ec)) {
        if (ec) { report_skip ("entry", path, ec); break; }
                auto entry = *it; 
        std::error_code ec2; 
        if ( entry.is_symlink(ec2) ) {
                                    continue;  
} 
        if ( ec2 ) {
                                    {
                                report_skip("file_type", entry.path(), ec2);
                                continue; 
}  
} 
                const fs::path p = entry.path(); 
        if ( entry.is_directory(ec2) ) {
                                    {
                                if ( ec2 ) {
                                                            {
                                                report_skip("file_type", p, ec2);
                                                continue; 
}  
} 
                                subdirs.push_back(p);
                                continue; 
}  
} 
        if ( ec2 ) {
                                    {
                                report_skip("file_type", p, ec2);
                                continue; 
}  
} 
        if ( !entry.is_regular_file(ec2) ) {
                                    continue;  
} 
        if ( ec2 ) {
                                    {
                                report_skip("file_type", p, ec2);
                                continue; 
}  
} 
                auto size = entry.file_size(ec2); 
        if ( ec2 ) {
                                    {
                                report_skip("metadata", p, ec2);
                                continue; 
}  
} 
        if ( 0<size&&size<uintmax_t{1}<<48 ) {
                                                auto child = Node(); 
            child.path=p;
            child.size=size;
            child.isDir=false;
            child.color=color_for_path(p);
            node.size+=size;
            node.children.push_back(std::move(child));  
}    
        }
        std::vector<Node> results (subdirs.size ()); 
        bool fanout = !subdirs.empty () && depth < 64; 
        if (fanout) {
        const unsigned was = ScanWorkersActive ().fetch_add (1); 
        fanout = was < ScanWorkerLimit (); 
        if (!fanout) { ScanWorkersActive ().fetch_sub (1); }
        }
        if (fanout) {
        std::vector<std::thread> workers; 
        workers.reserve (subdirs.size ()); 
        for (size_t i = 0; i < subdirs.size (); ++i) {
        workers.emplace_back ([&, i] { results[i] = scan_tree_parallel_impl (subdirs[i], depth + 1); }); 
        }
        for (auto& t : workers) { t.join (); }
        ScanWorkersActive ().fetch_sub (1); 
        } else {
        for (size_t i = 0; i < subdirs.size (); ++i) {
        results[i] = scan_tree_parallel_impl (subdirs[i], depth + 1); 
        }
        }
        for ( auto& child : results ) {
                        if ( 0<child.size ) {
                                                node.size+=child.size;
                node.children.push_back(std::move(child)); 
} 
};
        return node; 
}
  
} 