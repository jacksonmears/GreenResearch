float dir(float x) {
    return (x >= 0) - (x < 0);
}


__kernel void update_particles(
    __global float* xR,
    __global float* yR,
    __global float* Vn,
    __global float* Vt,
    const float R,
    const float dt,
    const float g,
    const float screen_width,
    const float screen_height,
    const int N,
    const float collision_damping,
    __global float* pressureForceX, 
    __global float* pressureForceY,
    __global float* pressureAccelerationX, 
    __global float* pressureAccelerationY,
    const float air_damping,
    const int leftMouseDown,
    const int mouseX,
    const int mouseY
) {
    int i = get_global_id(0);


    if (leftMouseDown) {
        // std::cout << mouseX << " " << mouseY << std::endl;
        float radius = 10.0f;   // example radius in screen units
        float strength = 1.0f; // how hard to push

        for (int i = 0; i < N; i++) {
            float dx = xR[i] - mouseX;
            float dy = yR[i] - mouseY;
            float dist2 = dx*dx + dy*dy;

            if (dist2 < radius*radius && dist2 > 1e-6f) {
                float dist = sqrt(dist2);
                float nx = dx / dist; // normalize
                float ny = dy / dist;

                float force = strength * (1.0f - dist / radius); 
                Vn[i] += ny * force; 
                Vt[i] += nx * force;
            }
        }
    }

    Vn[i] += pressureAccelerationY[i] * dt;
    Vt[i] += pressureAccelerationX[i] * dt;

    yR[i] += Vn[i] * dt * air_damping;
    xR[i] += Vt[i] * dt * air_damping;

    // --- Wall bounds ---
    float y_max = screen_height - R;
    float y_min = R;
    float x_min = R;
    float x_max = screen_width - R;

    // --- No collision damping off walls for now
    if (yR[i] < y_min + R) {
        Vn[i] *= -1;
        yR[i] = y_min + R;
    }
    if (yR[i] > y_max - R) {
        Vn[i] *= -0.5;
        yR[i] = y_max - R;
    }
    if (xR[i] < x_min + R) {
        Vt[i] *= -0.5;
        xR[i] = x_min + R;
    }
    if (xR[i] > x_max - R) {
        Vt[i] *= -0.5;
        xR[i] = x_max - R;
    }




}
