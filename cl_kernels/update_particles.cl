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
    const int num_particles,
    const float collision_damping,
    __global float* pressureForceX, 
    __global float* pressureForceY,
    __global float* pressureAccelerationX, 
    __global float* pressureAccelerationY,
    const float air_damping
) {
    int i = get_global_id(0);

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
        Vn[i] *= -1;
        yR[i] = y_max - R;
    }
    if (xR[i] < x_min + R) {
        Vt[i] *= -1;
        xR[i] = x_min + R;
    }
    if (xR[i] > x_max - R) {
        Vt[i] *= -1;
        xR[i] = x_max - R;
    }




}
