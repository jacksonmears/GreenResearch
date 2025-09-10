#pragma once

class Config {
public:
    double SCREEN_WIDTH = 1400, SCREEN_HEIGHT = 1000;
    double gravity = 9.81;
    double deltaTime = 1.0 / 20000;
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
