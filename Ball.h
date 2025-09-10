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
    double radius;
    double mass, I, x, y, restitution;
    double g = Config::get().gravity * Config::get().pixelsPerMeter;
    double accelerationY = g, accelerationX = 0;
    double alpha, w = -600; //angular acceleration and angular velocity             
    double velocityY = 200, velocityX = 1100;
    double mu_b; // coefficient of friction for ball kinietic friction and rolling resistence
    double slipTolerance = 1e-2;
    SDL_Color color;

    Ball(double radius_, double mass_, double x_, double y_, double restitution_, SDL_Color color_, double mu_b_)
        : radius(radius_), mass(mass_), x(x_), y(y_), restitution(restitution_), color(color_), mu_b(mu_b_)
        {
            double radius_m = radius / Config::get().pixelsPerMeter;
            I = (2.0 / 5.0) * mass * radius_m * radius_m * Config::get().pixelsPerMeter * Config::get().pixelsPerMeter;
        }


    void resetVelocity() { velocityY = 0; velocityX = 0; w = 0; }

    inline int fetchSign(double x) {return (x > 0.0) - (x < 0.0); }

    void update(Ground* ground) {
            double dt = Config::get().deltaTime;

            velocityY += accelerationY * dt; // apply gravity to vertical velocity
            y += velocityY * dt;    // change y according to velocity Y

            // velocityX += accelerationX * dt; // eventually apply wind forces
            x += velocityX * dt; // apply manual changes from SDL_-> arrow key

            // if x is out of bounds, move it back in and reflect velo and reduce velo via coef of restitution
            if (x + radius >= Config::get().SCREEN_WIDTH) {
                std::cout << velocityX << std::endl;
                x = Config::get().SCREEN_WIDTH - radius;
                velocityX = -velocityX * restitution;
            }

            if (x - radius <= 0) {
                std::cout << velocityX << std::endl;
                x = radius;
                velocityX = -velocityX * restitution;
            }
            
            // if y is below our artificial ground we need to reflect it via restituion
            if (y + radius >= Config::get().SCREEN_HEIGHT - ground->height) {

                y = Config::get().SCREEN_HEIGHT - radius - ground->height;  // reset y to ground level to ensure it never "goes below the ground"

                // threshold where ball is firmly on ground (no more bouncing impulses)
                if (velocityY <= accelerationY*dt*restitution) { 
                    double v0 = velocityX;  // initial linear velocity 
                    double w0 = w;          // initial anglular velocity
                    double R = radius;      
                    double s = v0 - w0 * R; // slip velocity at contact
                    double m = mass;
                    int sign;

                    double tr = (2*(velocityX - w*radius)) / (7*ground->mu_k*g); // time to transition
                    double velocity_rolling_threshold = velocityX - ground->mu_k*g*tr;  // linear velocity at the moment rolling should start
                    double angular_rolling_threshold = velocity_rolling_threshold/radius;  // angular velocity at the moment rolling should start

                    if (abs(s) <= slipTolerance) {
                        double a_rolling = ground->mu_r*g;  // linear acceleration due to rolling resistance
                        sign = fetchSign(v0); // check if v0 is pos or neg
                        double dv = sign*a_rolling*dt; // linear velocity change as a function of time
                        if (abs(dv) >= abs(v0)) {
                            velocityX = 0.0; 
                            w = 0.0;
                        } else {
                            velocityX = v0 - dv;
                            w = velocityX / R;
                        }
                        // velocityX -= mu_r*g*(dt-tr); // linear velocity while rolling
                    } else {
                        sign = fetchSign(s);
                        double a_linear = ground->mu_k*g; // linear acceleration defined
                        velocityX = v0 - sign * a_linear *dt; // linear velocity as function of time while sliding
                        alpha = (ground->mu_k*mass*g*radius)/I; // angular acceleration due to friction torque
                        w = w0 + sign * alpha *dt; // angular veolocity as function of time
                    }

                }

                // still bouncing
                else {
                    // need to reduce veloX from friction
                    // velocityX -= (velocityX>0 ? 1 : -1)*accelerationY*mu_b*dt; // very simply calcuation assuming no forces acting on ball besides mu per second thus dt (using accelerationY for simplicity as it's gravity)

                    double theta = ground->angle;


                    // Decompose velocity into normal and tangential components
                    double Vn = velocityX*sin(theta) + velocityY*cos(theta); // normal velocity
                    double Vt = velocityX*cos(theta) - velocityY*sin(theta); // tangential velocity

                    double Jn = -(1 + restitution)*mass*Vn; // normal impulse with restitition

                    double s = Vt - w*radius; // relative slip velocity

                    // finding tangential impulse 
                    double Jt;
                    double Jt_stick = -(s / ((1/mass) + ((radius * radius)/I)));
                    if (abs(Jt_stick) <= mu_b*fabs(Jn)) { // then stick
                        Jt = Jt_stick;
                    } else { // then slide
                        Jt = -mu_b*fabs(Jn)*fetchSign(s);
                    }

                    // apply impulses
                    double VnFinal = Vn + Jn/mass;
                    double VtFinal = Vt + Jt / mass;
                    w -= Jt*radius / I;

                    // recompose
                    velocityX = VnFinal*sin(theta) + VtFinal*cos(theta);
                    velocityY = VnFinal*cos(theta) - VtFinal*sin(theta);






                    // double effective_restitution = sqrt(restitution * ground->restitution);
                    // velocityY = -velocityY * effective_restitution; // partially reflect Y's velocity after contact with ground via restitution
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


};

