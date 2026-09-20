#pragma once
#include <string> 
#include "010_scanner.hpp" 
namespace treemap {
            
Color hash_color (const fs::path& path)        {
                        auto name = path.filename ().string (); 
        auto h = uint32_t{0};  
                for ( auto& b : name ) {
                        h+=b;
};
                return {float(((h*37)%160)+80)/255.0f, float(((h*59)%160)+80)/255.0f, float(((h*83)%160)+80)/255.0f, 1.0f};
}
 
    
Color color_for_path (const fs::path& path)        {
                        auto ext = path.extension ().string ();  
                if ( ext==".rs"||ext==".c"||ext==".cpp"||ext==".py"||ext==".js"||ext==".ts"||ext==".txt"||ext==".md" ) {
                        return {0.2f, 0.75f, 0.45f, 1.0f};
} else {
                        if ( ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".svg"||ext==".webp" ) {
                                return {0.2f, 0.65f, 0.95f, 1.0f};
} else {
                                if ( ext==".mp4"||ext==".mkv"||ext==".mov"||ext==".mp3"||ext==".flac" ) {
                                        return {0.75f, 0.35f, 0.85f, 1.0f};
} else {
                                        if ( ext==".zip"||ext==".tar"||ext==".gz"||ext==".7z" ) {
                                                return {0.9f, 0.3f, 0.25f, 1.0f};
} else {
                                                return hash_color(path);
} 
} 
} 
} 
}
 
    
std::string format_bytes (uintmax_t b)        {
                        auto d = float(b); 
        auto i = 0;  
                const char* units[] = {"B", "KB", "MB", "GB", "TB"}; 
                while ( 1024.F<=d&&i<4 ) {
                        d/=1024.F;
                        i++;
} 
                char buf[32]; 
                snprintf (buf, sizeof (buf), "%.1f %s", d, units[i]); 
                return buf;
}
  
} 