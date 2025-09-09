#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include "Config.h"
#include "Ground.h"
#include <iostream>
#include <cmath>
#include <algorithm>

class Shape {
public:
    int width, height;
    double mass, I, x, y, restitution;      
    double accelerationY = Config::get().gravity, accelerationX = 0;
    double omega = 5;              // angular velocity (rad/sec)
    double velocityY = 0, velocityX = 0;
    double frictionGround = 50.0; // horizontal ground friction (pixels/sec^2)
    double frictionWall   = 50.0; // vertical wall friction
    double rollingFriction = 0.001;
    double fc_modifier = 1;

    // eventually include
    double contactDuration, stiffness, deformation;
    SDL_Color color;

    Shape(int width_, int height_, double mass_, double x_, double y_, double restitution_, SDL_Color color_)
        : width(width_), height(height_), mass(mass_), x(x_), y(y_), restitution(restitution_), color(color_) {}

    virtual void render(SDL_Renderer* renderer) = 0;

    void resetVelocity() { velocityY = 0; velocityX = 0; omega = 0; }

    void update(Ground* ground) {
            double dt = Config::get().deltaTime;
            
            // --- Gravity ---
            velocityY += accelerationY * dt;

            // --- Horizontal acceleration (optional external force) ---
            // velocityX += accelerationX * dt;

            // --- Update positions ---
            x += velocityX * dt;
            y += velocityY * dt;

            // --- Wall collisions ---
            if (x - width/2 <= 0) {
                x = width/2;
                velocityX = -velocityX * restitution;
                // velocityY += omega * width/2 * dt; // spin contributes to bounce
                omega *= (1-rollingFriction); // simple wall decay, small
                applyWallFriction(dt);
            } else if (x + width/2 >= Config::get().SCREEN_WIDTH) {
                x = Config::get().SCREEN_WIDTH - width/2;
                velocityX = -velocityX * restitution;
                // velocityY -= omega * width/2 * dt;
                omega *= (1-rollingFriction);
                applyWallFriction(dt);
            }

            // --- Ground collision ---
            if (y + height/2 >= ground->y) {
                std::cout << omega << std::endl;
                // Prevent sinking into ground
                y = ground->y - height/2;

                double theta = ground->angle;

                // Decompose velocity into normal and tangential components
                double normal_velocity = velocityX*sin(theta) + velocityY*cos(theta);
                double tangential_velocity = velocityX*cos(theta) - velocityY*sin(theta);

                // ----- COLLISION IMPULSE -----
                double normal_impulse = mass * (1 + restitution) * normal_velocity;
                double relative_tang_velocity = omega*width/2 + tangential_velocity;
                double friction_impulse_threshold = relative_tang_velocity / (1.0/mass + (width/2*width/2)/I);
                double friction_limited_impulse = ground->fc * normal_impulse * dt;

                double Jt;
                if (std::abs(friction_impulse_threshold) <= friction_limited_impulse) {
                    Jt = friction_impulse_threshold; // sticking
                } else {
                    Jt = (friction_impulse_threshold > 0 ? 1 : -1) * friction_limited_impulse; // sliding
                }

                // Update angular velocity from collision
                omega = omega - Jt * width/2 / I;

                // Update tangential velocity from collision
                double VtFinal = tangential_velocity - Jt / mass;

                // Update normal velocity (collision restitution)
                double effective_restitution = sqrt(restitution * ground->restitution);
                double VnFinal = -effective_restitution * normal_velocity;

                // ----- SPIN → HORIZONTAL TRANSFER (BLENDED) -----
                // double r = width / 2.0;
                // double v_slip = VtFinal - omega * r;

                // // Determine max fraction of spin that can convert per collision
                // double transfer_fraction = 0.05;  // tweak for golf vs bouncy
                // double friction_transfer = v_slip * transfer_fraction;

                // // Cap the transfer so it doesn’t fully lock the ball
                // friction_transfer = std::clamp(friction_transfer, -std::abs(v_slip), std::abs(v_slip));

                // VtFinal -= friction_transfer;    // reduce slip gradually
                // omega += friction_transfer / r;  // reduce spin gradually

                // ----- CONTINUOUS ROLLING FRICTION (SMOOTHED) -----
                // double rollingFactor = rollingFriction * mass * Config::get().gravity * dt;
                // v_slip = VtFinal - omega * r;

                // // scale effect: weak when big slip, strong when close to rolling
                // double slip_ratio = std::min(1.0, std::abs(v_slip) / (std::abs(VtFinal) + 1e-5));

                // if (std::abs(v_slip) < 50) {
                //     double dv = rollingFactor * (v_slip > 0 ? -1 : 1) * slip_ratio;
                //     VtFinal += dv;            // linear velocity change
                //     omega  -= dv / r;         // spin reduction
                // }



                // // --- SPEED-DEPENDENT ROLLING FRICTION (optional damping) ---
                // double speed = std::abs(VtFinal);
                // double lowSpeedBoost = 0.05;
                // double frictionFactor = rollingFriction * (1.0 + lowSpeedBoost / (speed + 0.01));
                // VtFinal *= std::max(0.0, 1.0 - frictionFactor * dt);

                // --- THRESHOLD STOP ---
                double minRollVelocity = 5; // pixels/sec threshold to stop
                if (std::abs(VtFinal) < minRollVelocity) VtFinal = 0.0;

                // Convert back to global velocities
                velocityX = VtFinal * cos(theta) + VnFinal * sin(theta);
                velocityY = -VtFinal * sin(theta) + VnFinal * cos(theta);
            }



    }


protected:
    // void applyGroundFriction(double dt, double frictionDecel) {
    //     if (velocityX > 0) {
    //         velocityX -= frictionDecel * dt;
    //         if (velocityX < 0) velocityX = 0;
    //     } else if (velocityX < 0) {
    //         velocityX += frictionDecel * dt;
    //         if (velocityX > 0) velocityX = 0;
    //     }
    // }

    void applyWallFriction(double dt) {
        if (velocityY > 0) {
            velocityY -= frictionWall * dt;
            if (velocityY < 0) velocityY = 0;
        } else if (velocityY < 0) {
            velocityY += frictionWall * dt;
            if (velocityY > 0) velocityY = 0;
        }
    }
};

class Ball : public Shape {
public:
    int radius;
    double radius_m = radius / Config::get().pixelsPerMeter;

    Ball(int radius_, double mass_, double x_, double y_, double cor_, SDL_Color color_)
        : Shape(radius_ * 2, radius_ * 2, mass_, x_, y_, cor_, color_), radius(radius_) 
    {

        I = (2.0 / 5.0) * mass * radius_m * radius_m;
    }

    void render(SDL_Renderer* renderer) override {
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
