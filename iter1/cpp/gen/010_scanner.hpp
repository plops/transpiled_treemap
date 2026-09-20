#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector> 
namespace fs = std::filesystem; 
namespace treemap {
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
 
    inline Color color_for_path (const fs::path&);  
            
void scan_entry (Node& node, const fs::directory_entry& entry)        {
                std::error_code ec; 
                        auto st = entry.status(ec); 
        if ( ec ) {
                                    {
                                report_skip("file_type", entry.path(), ec);
                                return ;
}  
} 
        if ( fs::is_symlink(entry.symlink_status(ec)) ) {
                                    return ; 
} 
                auto p = entry.path(); 
        if ( st.type()==fs::file_type::directory ) {
                                    auto child = scan_tree(p); 
            if ( 0<child.size ) {
                                                node.size+=child.size;
                node.children.push_back(std::move(child)); 
}  
} else {
                        if ( st.type()==fs::file_type::regular ) {
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
}   
}
 
    
Node scan_tree (const fs::path& path)        {
                        auto node = Node(); 
        node.path=path;
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
                        auto node = Node; 
        auto subdirs = std::vector<fs::path>; 
        node.path=path;
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
                auto st = entry.status(ec2); 
        if ( ec2 ) {
                                    {
                                report_skip("file_type", entry.path(), ec2);
                                continue; 
}  
} 
        if ( fs::is_symlink(entry.symlink_status(ec2)) ) {
                                    continue;  
} 
                auto p = entry.path(); 
        if ( st.type()==fs::file_type::directory ) {
                        subdirs.push_back(p);
} else {
                        if ( st.type()==fs::file_type::regular ) {
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
}    
        }
        std::vector<Node> results (subdirs.size ()); 
        std::vector<std::thread> workers; 
        workers.reserve (subdirs.size ()); 
        for (size_t i = 0; i < subdirs.size (); ++i) {
        workers.emplace_back ([&, i] { results[i] = scan_tree_parallel (subdirs[i]); }); 
        }
        for (auto& t : workers) { t.join (); }
        for ( auto& child : results ) {
                        if ( 0<child.size ) {
                                                node.size+=child.size;
                node.children.push_back(std::move(child)); 
} 
};
        return node; 
}
  
} 