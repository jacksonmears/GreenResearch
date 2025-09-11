__kernel void update_drops(
    __global float* x,
    __global float* y,
    __global float* Vn,
    __global float* Vt,
    __global const float* R,
    __global const float* restitution,
    const float dt,
    const float g,
    const float mu_k,
    const float screen_width,
    const float screen_height
) {
    int i = get_global_id(0);

    // Update vertical velocity and position
    Vn[i] += g * dt;
    y[i] += Vn[i] * dt;

    // Update horizontal position
    x[i] += Vt[i] * dt;

    // Bounce off walls using branchless operations
    float x_max = screen_width - R[i];
    float x_min = R[i];
    Vt[i] = (x[i] > x_max) ? -Vt[i] * restitution[i] : ((x[i] < x_min) ? -Vt[i] * restitution[i] : Vt[i]);
    x[i] = fmin(fmax(x[i], x_min), x_max);

    // Bounce off floor
    float y_max = screen_height - R[i];
    if (y[i] > y_max) {
        y[i] = y_max;
        if (Vn[i] <= 0.1f) {
            Vt[i] -= (Vt[i] >= 0 ? 1 : -1) * g * mu_k * dt;
        }
        Vn[i] = -Vn[i] * restitution[i];
    }
}
