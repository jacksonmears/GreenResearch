inline int getCell(
    const int row,
    const int col,
    const int grid_width,
    const int MAX_PARTICLES
) {
    return (row * grid_width + col) * MAX_PARTICLES;
}


inline bool inBounds(
    const int row,
    const int col,
    const int grid_width,
    const int grid_height
) {
    return (row >= 0 && row < grid_height && col >= 0 && col < grid_width);
}


__kernel void kernel_functions(

) {

}
