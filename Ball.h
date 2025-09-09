#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include "Config.h"
#include "Ground.h"
#include <iostream>
#include <cmath>
#include <algorithm>

class Ball {
public:
    int radius;
    double mass, I, x, y, restitution;
    double g = Config::get().gravity * Config::get().pixelsPerMeter;
    double accelerationY = g, accelerationX = 0;
    double alpha, w = -10; //angular acceleration and angular velocity             
    double velocityY = 0, velocityX = 0;
    double mu_b = 0.01, mu_k, mu_r; // coefficient of friction for ball kinietic friction and rolling resistence
    bool testSlide = false, testRoll = false;
    SDL_Color color;

    Ball(int radius_, double mass_, double x_, double y_, double restitution_, SDL_Color color_)
        : radius(radius_), mass(mass_), x(x_), y(y_), restitution(restitution_), color(color_) 
        {
            double radius_m = radius / Config::get().pixelsPerMeter;
            I = (2.0 / 5.0) * mass * radius_m * radius_m * 
                Config::get().pixelsPerMeter * Config::get().pixelsPerMeter;
        }


    void resetVelocity() { velocityY = 0; velocityX = 0; w = 0; }

    void update(Ground* ground) {
            double dt = Config::get().deltaTime;

            velocityY += accelerationY * dt; // apply gravity to vertical velocity
            y += velocityY * dt;    // change y according to velocity Y

            // velocityX += accelerationX * dt; // eventually apply wind forces
            x += velocityX * dt; // apply manual changes from SDL_-> arrow key

            // if x is out of bounds, move it back in and reflect velo and reduce velo via coef of restitution
            if (x + radius >= Config::get().SCREEN_WIDTH) {
                x = Config::get().SCREEN_WIDTH - radius;
                velocityX = -velocityX * restitution;
            }

            if (x - radius <= 0) {
                x = radius;
                velocityX = -velocityX * restitution;
            }
            
            // if y is below our artificial ground we need to reflect it via restituion
            if (y + radius >= Config::get().SCREEN_HEIGHT - ground->height) {

                y = Config::get().SCREEN_HEIGHT - radius - ground->height;  // reset y to ground level to ensure it never "goes below the ground"

                // threshold where ball is firmly on ground (no more bouncing impulses)
                if (velocityY <= accelerationY*dt*restitution) { 

                    double tr = (2*(velocityX - w*radius)) / (7*mu_k*g); // time to transition
                    double velocity_rolling_threshold = velocityX - mu_k*g*tr;  // linear velocity at the moment rolling should start
                    double angular_rolling_threshold = velocity_rolling_threshold/radius;  // angular velocity at the moment rolling should start

                    if (velocityX <= velocity_rolling_threshold) {
                        if (!testRoll) {
                            std::cout << "rolling" << std::endl;
                            testRoll = true;
                        }
                        double a_rolling = mu_r*g;  // linear acceleration due to rolling resistance
                        velocityX -= mu_r*g*(dt-tr); // linear velocity while rolling
                    }
                    else {
                        if (!testSlide) {
                            std::cout << "slipping" << std::endl;
                            testSlide = true;
                        }
                        double a_linear = mu_k*g; // linear acceleration defined
                        velocityX -= a_linear *dt; // linear velocity as function of time while sliding
                        alpha = (mu_k*mass*g*radius)/I; // angular acceleration due to friction torque
                        w += alpha*dt; // angular veolocity as function of time
                    }

                }

                // still bouncing
                else {
                    // need to reduce veloX from friction
                    // velocityX -= (velocityX>0 ? 1 : -1)*accelerationY*mu_b*dt; // very simply calcuation assuming no forces acting on ball besides mu per second thus dt (using accelerationY for simplicity as it's gravity)

                    double effective_restitution = sqrt(restitution * ground->restitution);
                    velocityY = -velocityY * effective_restitution; // partially reflect Y's velocity after contact with ground via restitution
                }

            }
            
           
    }

    void render(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        for (int w = 0; w < radius * 2; ++w) {
            for (int h = 0; h < radius * 2; ++h) {
                int dx = radius - w;
                int dy = radius - h;
                if (dx * dx + dy * dy <= radius * radius) {
                    SDL_RenderPoint(renderer, static_cast<int>(x) + dx, static_cast<int>(y) + dy);
                }
            }
        }
    }


protected:

};

