__kernel void build_grid(
    __global float* x,
    __global float* y,
    const float cell_size,
    __global int* cell_particles,
    __global int* cell_counts,
    const int grid_width,
    const int grid_height,
    const int MAX_PER_CELL
) {
    int i = get_global_id(0);

    int col = (int)floor(x[i] / cell_size);
    int row = (int)floor(y[i] / cell_size);

    // Clamp to grid
    col = max(0, min(col, grid_width - 1));
    row = max(0, min(row, grid_height - 1));
    
    // Clamp to grid
    // if (col < 0) col = 0;
    // if (row < 0) row = 0;
    // if (col >= grid_width) col = grid_width - 1;
    // if (row >= grid_height) row = grid_height - 1;

    int cellIndex = row * grid_width + col;


    int slot = atomic_inc(&cell_counts[cellIndex]);
    if (slot < MAX_PER_CELL) {
        cell_particles[cellIndex * MAX_PER_CELL + slot] = i;  // i = particle index
    } else {
        atomic_dec(&cell_counts[cellIndex]);
    }
    
    // if (i < 10) printf("x=%f y=%f col=%d row=%d cell_part=%d\n", x[i], y[i], col, row, cell_particles[cellIndex * MAX_PER_CELL + slot]);

    // if (i < 10) printf("x=%f y=%f col=%d row=%d\n", x[i], y[i], col, row);


}
