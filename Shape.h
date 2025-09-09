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


            if (y + height/2 >= ground->y) {
                // Prevent sinking into ground
                y = ground->y - height/2;

                double theta = ground->angle;

                // Decompose velocity into normal and tangential components
                double normal_velocity     = velocityX*sin(theta) + velocityY*cos(theta);
                double tangential_velocity = velocityX*cos(theta) - velocityY*sin(theta);

                // ------------------------------
                // Case 1: Impulse collision (bouncing phase)
                // ------------------------------
                const double eps = 1e-1; // still used for collision threshold
                if (normal_velocity > eps) {
                    // Normal impulse
                    double normal_impulse = mass * (1 + restitution) * normal_velocity;

                    // Tangential friction impulse
                    double relative_tang_velocity = omega * (width/2) + tangential_velocity;

                    // Effective mass at contact
                    double m_eff = (1.0/mass) + ((width/2.0)*(width/2.0))/I;
                    double friction_impulse_threshold = relative_tang_velocity / m_eff;
                    double friction_limited_impulse   = ground->fc * normal_impulse;

                    double Jt;
                    if (std::abs(friction_impulse_threshold) <= friction_limited_impulse) {
                        Jt = friction_impulse_threshold; // sticking
                    } else {
                        Jt = (friction_impulse_threshold > 0 ? 1 : -1) * friction_limited_impulse; // sliding
                    }

                    // Update angular velocity from collision
                    omega -= Jt * (width/2.0) / I;

                    // Update tangential velocity from collision
                    double VtFinal = tangential_velocity - Jt / mass;

                    // Update normal velocity with restitution
                    double effective_restitution = sqrt(restitution * ground->restitution);
                    double VnFinal = -effective_restitution * normal_velocity;

                    // Recompose into world space
                    velocityX = VtFinal * cos(theta) + VnFinal * sin(theta);
                    velocityY = -VtFinal * sin(theta) + VnFinal * cos(theta);
                }

                // ------------------------------
                // Case 2: Sustained contact (sliding/rolling)
                // ------------------------------
                else {
                    double N = mass * Config::get().gravity;   // normal force
                    double v_t = tangential_velocity;

                    // Compute contact point velocity: v_contact = v_t - r * omega
                    double r = width / 2.0;
                    double v_contact = v_t - omega * r;

                    // std::cout << N << " " << v_t << " " <<  v_contact << " " << omega << std::endl;

                    // Sliding: contact point is moving relative to surface
                    if (std::abs(v_contact) > 0.5) {
                        // --- Sliding with kinetic friction ---
                        // Determine sliding direction
                        double sliding_dir = (v_t != 0.0) ? (v_t / std::abs(v_t)) : 0.0;

                        // Friction always opposes center-of-mass motion
                        double Ff = -ground->mu_k * N * sliding_dir;
                        double a_t = Ff / mass;

                        // Update tangential velocity
                        v_t += a_t * dt * Config::get().pixelsPerMeter;

                        // Angular velocity update (torque)
                        omega += (Ff * r / I) * dt;

                    } 
                    // Rolling: contact point nearly at rest
                    else {
                        // Rolling resistance
                        double torque_rr = -ground->Crr * N * (omega / (std::abs(omega) + 1e-8));
                        omega += (torque_rr / I) * dt;

                        // Linear velocity follows angular velocity
                        v_t = omega * r;

                        // Clamp tiny velocities to stop
                        if (std::abs(v_t) < 0.5) {
                            v_t = 0.0;
                        }
                        if (std::abs(omega) < 0.01) omega = 0.0;
                    
                    }

                    // Recompose into world space
                    velocityX = v_t * cos(theta);
                    velocityY = -v_t * sin(theta);
                }
            }

    }


protected:
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
