__kernel void update_particles(
    __global float* x,
    __global float* y,
    __global float* Vn,
    __global float* Vt,
    const float R,
    const float dt,
    const float g,
    const float screen_width,
    const float screen_height,
    const int num_particles,
    const float collision_damping,
    __global float* pressureForceX, 
    __global float* pressureForceY,
    __global float* pressureAccelerationX, 
    __global float* pressureAccelerationY
) {
    int i = get_global_id(0);


    // Vn[i] += g * dt;
    Vn[i] += pressureAccelerationY[i] * dt;
    Vt[i] += pressureAccelerationX[i] * dt;

    y[i] += Vn[i] * dt;
    x[i] += Vt[i] * dt;



    // --- Wall bounds ---
    float y_max = screen_height - R;
    float y_min = R;
    float x_min = R;
    float x_max = screen_width - R;

    // --- Minimal wall-normal repulsion ---
    if (y[i] < y_min + R) {
        Vn[i] *= -collision_damping;
        y[i] = y_min + R;
    }
    if (y[i] > y_max - R) {
        Vn[i] *= -collision_damping;
        y[i] = y_max - R;
    }
    if (x[i] < x_min + R) {
        Vt[i] *= -collision_damping;
        x[i] = x_min + R;
    }
    if (x[i] > x_max - R) {
        Vt[i] *= -collision_damping;
        x[i] = x_max - R;
    }

    // --- Clamp positions to screen bounds ---
    // if (y[i] > y_max) y[i] = y_max;
    // if (y[i] < y_min) y[i] = y_min;
    // if (x[i] < x_min) x[i] = x_min;
    // if (x[i] > x_max) x[i] = x_max;

}
