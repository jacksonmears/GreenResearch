inline float calculateDistance(
    float x1, float x2,
    float y1, float y2
) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return sqrt(dx*dx + dy*dy);
}


inline float smoothingKernel(
    float dst, 
    float smoothing_radius,
    float PI

) {
    if (dst > smoothing_radius) return 0;

    float volume  = (PI * smoothing_radius * smoothing_radius* smoothing_radius* smoothing_radius) / 6;
    return fabs((smoothing_radius - dst) * (smoothing_radius - dst) / volume);
}


__kernel void calculate_densities(
    __global float* x,
    __global float* y,
    __global int* cell_particles,
    __global int* cell_counts,
    __global float* densities,
    const int MAX_PARTICLES,
    const int grid_width,
    const int grid_height,
    const float smoothing_radius,
    const float PI,
    const float mass
) {
    int cell_id = get_global_id(0);

    int row = cell_id / grid_width;
    int col = cell_id % grid_width;

    int start_idx = getCell(row, col, grid_width, MAX_PARTICLES);
    int num_particles_in_cell = cell_counts[cell_id];

    // Loop over all particles in this cell
    for (int u = 0; u < num_particles_in_cell; ++u) {
        int particle_u = cell_particles[start_idx + u];
        if (particle_u == -1) continue;

        float density = 0.0f;

        // Loop over neighbor cells (including this cell)
        for (int n_row = row - 1; n_row <= row + 1; ++n_row) {
            for (int n_col = col - 1; n_col <= col + 1; ++n_col) {
                if (!inBounds(n_row, n_col, grid_width, grid_height)) continue;

                int neighbor_cell = getCell(n_row, n_col, grid_width, MAX_PARTICLES);
                int num_particles_in_neighbor = cell_counts[n_row * grid_width + n_col];

                for (int v = 0; v < num_particles_in_neighbor; ++v) {
                    int particle_v = cell_particles[neighbor_cell + v];
                    if (particle_v == -1 || particle_v == particle_u) continue;

                    float dst = calculateDistance(x[particle_u], x[particle_v], y[particle_u], y[particle_v]);
                    float influence =  smoothingKernel(dst, smoothing_radius, PI);
                    density += mass * influence;
                }
            }
        }

        densities[particle_u] = density > 1e-4 ? density :  1e-4;
    }
}
