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

    // --- No collision damping off walls for now
    if (y[i] < y_min + R) {
        Vn[i] *= -1;
        y[i] = y_min + R;
    }
    if (y[i] > y_max - R) {
        Vn[i] *= -0.25;
        y[i] = y_max - R;
    }
    if (x[i] < x_min + R) {
        Vt[i] *= -1;
        x[i] = x_min + R;
    }
    if (x[i] > x_max - R) {
        Vt[i] *= -1;
        x[i] = x_max - R;
    }




}
