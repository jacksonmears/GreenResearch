__kernel void build_grid(
    __global const float* x,
    __global const float* y,
    const float cell_size,
    const int grid_width,
    const int grid_height,
    const int num_particles,
    __global int* head,
    __global int* next
) {
    int i = get_global_id(0);
    if (i >= num_particles) return;

    // Compute cell index
    int cellX = (int)floor(x[i] / cell_size);
    int cellY = (int)floor(y[i] / cell_size);

    // Clamp to grid
    if (cellX < 0) cellX = 0;
    if (cellY < 0) cellY = 0;
    if (cellX >= grid_width) cellX = grid_width - 1;
    if (cellY >= grid_height) cellY = grid_height - 1;

    int cellIndex = cellY * grid_width + cellX;

    // Insert into linked list atomically
    int old_head = atomic_xchg(&head[cellIndex], i);
    next[i] = old_head;
}
