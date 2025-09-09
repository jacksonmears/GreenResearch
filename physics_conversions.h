#include <iostream>
#include <cmath>

const double PI = 3.141592653589793;

struct Ball {
    // Essential properties
    double mass;           // kg
    double radius;         // m
    double I;              // moment of inertia, kg*m^2
    double omega;          // spin, rad/s
    double vx;             // horizontal velocity, m/s
    double vy;             // vertical velocity, m/s

    // Optional properties
    double contactDuration;  // s, optional: duration of contact
    double stiffness;        // N/m, optional: for soft contacts
    double deformation;      // m, optional: max compression
};

struct Surface {
    double friction;       // coefficient of friction (μ)
    double restitution;    // coefficient of restitution (e)
    double angle;          // angle of surface in radians (0 = horizontal)
    double roughness;      // optional: affects μ, can be combined
};

struct ImpactResult {
    double omegaFinal; // rad/s
    double vxFinal;    // m/s
    double vyFinal;    // m/s
};

// Calculate impact result for a ball hitting a surface
ImpactResult calculateImpact(const Ball& ball, const Surface& surface) {
    ImpactResult result;

    // 1. Resolve velocity into normal and tangential components
    double vx = ball.vx;
    double vy = ball.vy;
    double theta = surface.angle;
    
    double Vn = vx * sin(theta) + vy * cos(theta); // velocity normal to surface
    double Vt = vx * cos(theta) - vy * sin(theta); // velocity tangential to surface

    // 2. Estimate normal impulse (simplified using restitution)
    double Jn = ball.mass * (1 + surface.restitution) * Vn;

    // 3. Relative tangential speed at contact (spin + tangential velocity)
    double vRel = ball.omega * ball.radius + Vt;

    // 4. Required friction impulse to stop relative motion
    double Jt_req = vRel / (1.0/ball.mass + (ball.radius * ball.radius) / ball.I);

    // 5. Friction-limited impulse
    double Jt_max = surface.friction * Jn;
    double Jt;
    if (std::abs(Jt_req) <= Jt_max) {
        Jt = Jt_req; // sticking
    } else {
        Jt = (Jt_req > 0 ? 1 : -1) * Jt_max; // sliding
    }

    // 6. Update angular velocity
    result.omegaFinal = ball.omega - Jt * ball.radius / ball.I;

    // 7. Update tangential velocity
    double VtFinal = Vt - Jt / ball.mass;

    // 8. Update normal velocity after impact
    double VnFinal = -surface.restitution * Vn;

    // 9. Convert back to global vx, vy
    result.vxFinal = VtFinal * cos(theta) + VnFinal * sin(theta);
    result.vyFinal = -VtFinal * sin(theta) + VnFinal * cos(theta);

    return result;
}

int main() {
    // Example usage
    Ball ball {0.43, 0.11, 2.0/5.0*0.43*0.11*0.11, 125.66, 3.0, -5.0, 0.01, 1000.0, 0.005};
    Surface floor {0.5, 0.8, 0.0, 0.0};

    ImpactResult res = calculateImpact(ball, floor);

    std::cout << "Final spin (rad/s): " << res.omegaFinal << std::endl;
    std::cout << "Final vx (m/s): " << res.vxFinal << std::endl;
    std::cout << "Final vy (m/s): " << res.vyFinal << std::endl;

    return 0;
}
