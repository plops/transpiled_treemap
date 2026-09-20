// PGE3 GUI entry point: draws the iter2 parallel scan as a live treemap.
#define OLC_PGE3_APPLICATION

#include "030_pge_app.hpp"

int main(int argc, char** argv)
{
    treemap::fs::path target = ".";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (!a.empty() && a[0] != '-')
        {
            target = a;
        }
    }
    treemap::pge::TreemapApp app(target);
    if (app.Construct({1280, 720}, {1, 1}))
    {
        app.Start();
    }
    return 0;
}
