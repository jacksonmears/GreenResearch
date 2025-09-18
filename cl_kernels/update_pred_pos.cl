__kernel void update_pred_pos(
    __global float* x,
    __global float* y,
    __global const float* xR,
    __global const float* yR,
    __global float* Vn,
    __global float* Vt,
    const float dt,
    const float g

) {
    int i = get_global_id(0);

    
    Vn[i] += g * dt;

    y[i] = yR[i] + Vn[i] * dt;
    x[i] = xR[i];

    // printf("y=%f",y[i]);

    // y[i] = yR[i];
    // x[i] = xR[i];

}