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
    if (i >= num_particles) return;
    bool collision = false;

    // Determine which cell the particle is in
    int cellX = (int)floor(x[i] / cell_size);
    int cellY = (int)floor(y[i] / cell_size);

    // Clamp to grid
    if (cellX < 0) cellX = 0;
    if (cellY < 0) cellY = 0;
    if (cellX >= grid_width) cellX = grid_width - 1;
    if (cellY >= grid_height) cellY = grid_height - 1;

    // Check neighbors in current + adjacent cells
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int nx = cellX + dx;
            int ny = cellY + dy;
            if (nx < 0 || ny < 0 || nx >= grid_width || ny >= grid_height) continue;

            int neighbor_cell = ny * grid_width + nx;
            int node = head[neighbor_cell];

            while (node != -1) {
                if (node != i) {
                    float ddx = fabs(x[node] - x[i]);
                    float ddy = y[node] - y[i];
                    // float dist2 = ddx*ddx + ddy*ddy;
                    float rsum = R[i] + R[node];

                    // if (dist2 < rsum*rsum && dist2 > 0.0f) {
                    //     float dist = sqrt(dist2);
                    //     float overlap = rsum - dist;
                    //     collision = true;
                    // }

                    if (rsum > ddx && rsum > ddy && ddy > 0) {
                        y[i] = y[node] - rsum;
                        if (fabs(Vn[i]) > 1e-3) {
                            Vn[i] = -Vn[i] * 0.9f; // restitution
                        } else {
                            Vn[i] = 0;
                        }
                        collision = true;
                    }
                }
                node = next[node];
            }
        }
    }


    if (!collision) {
        // Handle collisions with screen boundaries

        // Apply gravity


        // if (collision) {
        //     Vn[i] = -Vn[i] * 0.9f; // restitution
        // }

        // Floor
        float y_max = screen_height - R[i];
        if (y[i] > y_max) {
            y[i] = y_max;
            // Vn[i] = 0;
            if (fabs(Vn[i]) > 1e-1) {
                Vn[i] = -Vn[i] * 0.9f; // restitution
            } else {
                Vn[i] = 0;
            }
        }

        else {
            // Ceiling
            float y_min = R[i];
            if (y[i] < y_min) {
                y[i] = y_min;
                // Vn[i] = -Vn[i] * 0.5f;
            }

            // Walls
            float x_min = R[i];
            float x_max = screen_width - R[i];
            if (x[i] < x_min) {
                x[i] = x_min;
                // Vt[i] = -Vt[i] * 0.5f;
            }
            if (x[i] > x_max) {
                x[i] = x_max;
                // Vt[i] = -Vt[i] * 0.5f;
            }

            Vn[i] += g * dt;
            y[i] += Vn[i] * dt;
            x[i] += Vt[i] * dt;
        }

    }
}
