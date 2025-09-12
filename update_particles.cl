__kernel void update_particles(
    __global float* x,
    __global float* y,
    __global float* Vn,
    __global float* Vt,
    __global const float* R,
    const float dt,
    const float g,
    const float screen_width,
    const float screen_height,
    const float cell_size,
    const int grid_width,
    const int grid_height,
    const int num_particles,
    __global int* head,
    __global int* next,
    __global int* killed
) {
    int i = get_global_id(0);

    // Apply gravity
    Vn[i] += g * dt;

    // Update positions
    x[i] += Vt[i] * dt;
    y[i] += Vn[i] * dt;

    // Collisions with floor/ceiling
    float y_max = screen_height - R[i];
    if (y[i] > y_max) {
        y[i] = y_max;
        Vn[i] *= -0.5f; // simple restitution
    }

    float y_min = R[i];
    if (y[i] < y_min) {
        y[i] = y_min;
        Vn[i] *= -0.5f;
    }

    // Collisions with walls
    float x_min = R[i];
    float x_max = screen_width - R[i];
    if (x[i] < x_min) { x[i] = x_min; Vt[i] *= -0.5f; }
    if (x[i] > x_max) { x[i] = x_max; Vt[i] *= -0.5f; }
}
