#pragma once

class Config {
public:
    double SCREEN_WIDTH = 800, SCREEN_HEIGHT = 600;
    double gravity = 980;

    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; // private constructor
};
