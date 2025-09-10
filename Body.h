#include "Drop.h"
#include "Config.h"
#include <vector>
#include <cstdlib>


class Body {
public:
    std::vector<Drop> children;

    Body() {}


    void fill_children(double n) {
        for (int i = 0; i < n; ++i) {
            children.emplace_back(rand() % static_cast<int>(Config::get().SCREEN_WIDTH), rand() % static_cast<int>(Config::get().SCREEN_HEIGHT), 10, 1, 0.5, Config::get().blue);
        }
    }

    void print_childred() {
        for (auto& child : children) child.print_coord();
    }



};