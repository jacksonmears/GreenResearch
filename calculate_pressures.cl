inline float offsetX(
    float x1, float x2
) {
    float dx = x1 - x2;
    return dx;
}


inline float offsetY(
    float y1, float y2
) {
    float dy = y1 - y2;
    return dy;
}


float smoothingKernelDerivative(
    const float dst,
    const float smoothing_radius,
    const float PI
) {
    if (dst >= smoothing_radius) return 0;

    float scale = -12 / (smoothing_radius * smoothing_radius * smoothing_radius * smoothing_radius * PI);
    return (dst - smoothing_radius) * scale; 
}


float convertDensityToPressure(
    const float density, 
    const float target_density, 
    const float pressureMultiplier
) {
    float densityError = density - target_density;
    float pressure = densityError * pressureMultiplier;
    return pressure;
}


float calculateSharedPressure(
    float densityA, 
    float densityB, 
    const float target_density, 
    const float pressureMultiplier
) {
    float pressureA = convertDensityToPressure(densityA, target_density, pressureMultiplier);
    float pressureB = convertDensityToPressure(densityB, target_density, pressureMultiplier);
    return (pressureA + pressureB) / 2;
}


inline void atomic_add_float(
    __global float* addr, 
    float val
) {
    union { unsigned int u32; float f; } oldVal, newVal;
    do {
        oldVal.f = *addr;
        newVal.f = oldVal.f + val;
    } while (atomic_cmpxchg((__global unsigned int*)addr, oldVal.u32, newVal.u32) != oldVal.u32);
}



inline float atomic_xchg_float(__global float* addr, float val) {
    union { unsigned int u32; float f; } oldVal, newVal;
    oldVal.f = *addr;
    newVal.f = val;
    // Loop until the compare-and-swap succeeds
    while (atomic_cmpxchg((__global unsigned int*)addr, oldVal.u32, newVal.u32) != oldVal.u32) {
        oldVal.f = *addr; // reload current value
    }
    return oldVal.f; // return the old value
}

inline float getRandomDir(uint seed) {
    // Linear congruential generator parameters
    seed = (1103515245 * seed + 12345);
    float rnd = (float)(seed & 0x00FFFFFF) / 0x01000000; // gives [0,1)
    return -1.0f + 2.0f * rnd;  // maps to [-1,1)
}




__kernel void calculate_pressures(
    __global const int* cell_particles,
    __global const int* cell_counts,
    __global const float* x,
    __global const float* y,
    __global const float* densities,
    __global float* pressureForceX,
    __global float* pressureForceY,
    __global float* pressureAccelerationX,
    __global float* pressureAccelerationY,
    const float target_density,
    const float pressureMultiplier,
    const float smoothing_radius,
    const float PI,
    const float mass,
    const int grid_width,
    const int grid_height,
    const int MAX_PARTICLES,
    const float collision_damping
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

        // Loop over neighbor cells (including this cell)
        for (int n_row = row - 1; n_row <= row + 1; ++n_row) {
            for (int n_col = col - 1; n_col <= col + 1; ++n_col) {
                if (!inBounds(n_row, n_col, grid_width, grid_height)) continue;

                int neighbor_cell = getCell(n_row, n_col, grid_width, MAX_PARTICLES);
                int num_particles_in_neighbor = cell_counts[n_row * grid_width + n_col];

                for (int v = 0; v < num_particles_in_neighbor; ++v) {
                    int particle_v = cell_particles[neighbor_cell + v];
                    if (particle_v == -1 || particle_v == particle_u) continue;

                    float oY = -offsetY(y[particle_u], y[particle_v]), oX = -offsetX(x[particle_u], x[particle_v]);
                    float dst = sqrt(oY*oY + oX*oX);
                    if (dst < 1e-5) dst = 1e-5;


                    float2 dir = {oX / dst, oY / dst};
                    // if (dir.x == 0) dir.x = 0.15f;
                    // if (dir.y == 0) dir.y = 0.15f;

                    float slope = smoothingKernelDerivative(dst, smoothing_radius, PI);
                    float sharedPressure = calculateSharedPressure(densities[particle_u], densities[particle_v], target_density, pressureMultiplier);

                    float Fx = sharedPressure * dir[0] * slope * mass;
                    float Fy = sharedPressure * dir[1] * slope * mass;

                    if (particle_u < particle_v) {
                        float Fx = sharedPressure * dir.x * slope * mass * collision_damping;
                        float Fy = sharedPressure * dir.y * slope * mass * collision_damping;

                        atomic_add_float(&pressureForceX[particle_u], Fx / densities[particle_u]);
                        atomic_add_float(&pressureForceY[particle_u], Fy / densities[particle_u]);
                        atomic_add_float(&pressureForceX[particle_v], -Fx / densities[particle_v]);
                        atomic_add_float(&pressureForceY[particle_v], -Fy / densities[particle_v]);

                        // atomic_xchg_float(&pressureAccelerationX[particle_u], pressureForceX[particle_u] / densities[particle_u]);
                        // atomic_xchg_float(&pressureAccelerationY[particle_u], pressureForceY[particle_u] / densities[particle_u]);
                        // atomic_xchg_float(&pressureAccelerationX[particle_v], pressureForceX[particle_v] / densities[particle_v]);
                        // atomic_xchg_float(&pressureAccelerationY[particle_v], pressureForceY[particle_v] / densities[particle_v]);
                        // printf("i=%d pressureForceX=%f presureForceY=%f\n", cell_id, pressureForceX[particle_u], pressureForceY[particle_u]);
                    }

                }
            }
        }

    }





}
