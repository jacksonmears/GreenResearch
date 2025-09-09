#pragma once

class Config {
public:
    double SCREEN_WIDTH = 1400, SCREEN_HEIGHT = 1000;
    double gravity = 980;
    double deltaTime = 1.0 / 5000;
    double pixelsPerMeter = 100;
    double massScale = 1.0;
    const double PI = 3.141592653589793;

    static Config& get() {
        static Config instance;
        return instance;
    }

private:
    Config() = default; // private constructor
};
