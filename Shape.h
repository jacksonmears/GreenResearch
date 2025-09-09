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
    double omega = -1000;              // angular velocity (rad/sec)
    double velocityY = 0, velocityX = 0;
    double frictionGround = 50.0; // horizontal ground friction (pixels/sec^2)
    double frictionWall   = 50.0; // vertical wall friction
    double rollingFriction = 0.001;
    double k_spin = 0.05;   // air drag for rotation (bleed omega)
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

            double angular_damping = -k_spin * omega;
            omega += (angular_damping / I) * dt;

            // Clamp very small values to zero
            if (std::abs(omega) < 1e-3) omega = 0.0;

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
                    std::cout << omega << std::endl;
                    // --- Normal impulse ---
                    double effective_restitution = sqrt(restitution * ground->restitution);
                    double VnFinal = -effective_restitution * normal_velocity;

                    // --- Tangential impulse due to friction and spin ---
                    double r = width; // radius for torque
                    double v_contact = tangential_velocity - omega * r; // relative velocity at contact
                    double N_impulse = mass * (1 + effective_restitution) * normal_velocity; // normal impulse magnitude

                    // Max friction impulse allowed
                    double Jt_max = ground->fc * N_impulse;

                    // Desired tangential impulse to bring contact velocity to zero
                    // Limit the maximum correction based on realistic physical scale
                    const double max_correction_speed = 50.0; // tune this
                    double v_contact_clamped = std::clamp(v_contact, -max_correction_speed, max_correction_speed);

                    double Jt_desired = v_contact_clamped / (1.0 / mass + r * r / I);

                    // Still clamp by Coulomb friction
                    double Jt = std::clamp(Jt_desired, -Jt_max, Jt_max);


                    // --- Apply impulses ---
                    // Linear tangential velocity
                    double force_t = Jt / dt;    // Convert impulse to force
                    double a_t = force_t / mass; // Linear acceleration
                    double VtFinal = tangential_velocity - a_t * dt * 100;

                    // Scale torque effect relative to tangential vs. normal contribution
                    double tangential_factor = std::abs(tangential_velocity) /
                                            (std::abs(tangential_velocity) + std::abs(normal_velocity) + 1e-8);

                    double alpha = ((force_t * r) / I) * tangential_factor; // Angular acceleration
                    omega += alpha * dt;


                    // --- Recompose world-space velocity ---
                    velocityX = VtFinal * cos(theta) + VnFinal * sin(theta);
                    velocityY = -VtFinal * sin(theta) + VnFinal * cos(theta);
                }

                // ------------------------------
                // Case 2: Sustained contact (sliding/rolling)
                // ------------------------------
                else {
                    double N = mass * Config::get().gravity;   // normal force
                    double r = width / 2.0;

                    // Compute contact point velocity along surface
                    double v_t = tangential_velocity;
                    double v_contact = v_t - omega * r;

                    const double threshold = 1e-3; // for numerical stability

                    if (std::abs(v_contact) > threshold) {
                        std::cout << "sliding" << std::endl;
                        // --- Sliding ---
                        double friction_dir = (v_contact != 0.0) ? (v_contact / std::abs(v_contact)) : 0.0;

                        // Tangential force opposes relative motion
                        double Ff = -ground->mu_k * N * (v_contact / (std::abs(v_contact) + 1e-8));


                        // Linear acceleration along tangent
                        double a_t = Ff / mass;

                        // Update tangential velocity along surface
                        v_t += a_t * dt * Config::get().pixelsPerMeter;

                        // Update angular velocity (torque)
                        omega += (Ff * r / I) * dt;

                    } else {
                        // --- Rolling ---
                        double torque_rr = -ground->Crr * N * (omega / (std::abs(omega) + 1e-8));
                        omega += (torque_rr / I) * dt;

                        // Linear velocity along tangent follows omega
                        v_t = omega * r;

                        // Clamp very small velocities
                        if (std::abs(v_t) < threshold) v_t = 0.0;
                        if (std::abs(omega) < 1e-3) omega = 0.0;
                    }

                    // Recompose into world space along slope
                    velocityX = v_t * cos(theta) + normal_velocity * sin(theta);
                    velocityY = -v_t * sin(theta) + normal_velocity * cos(theta);
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

    Ball(int radius_, double mass_, double x_, double y_, double cor_, SDL_Color color_)
        : Shape(radius_ * 2, radius_ * 2, mass_, x_, y_, cor_, color_), radius(radius_) 
    {
        double radius_m = radius / Config::get().pixelsPerMeter;

        I = (2.0 / 5.0) * mass * radius_m * radius_m * 
            Config::get().pixelsPerMeter * Config::get().pixelsPerMeter;
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
