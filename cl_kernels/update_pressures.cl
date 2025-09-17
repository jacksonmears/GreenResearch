__kernel void update_pressures(
    __global const float* pressureForceX,
    __global const float* pressureForceY,
    __global const float* densities,
    __global float* pressureAccelerationX,
    __global float* pressureAccelerationY
) {
    int i = get_global_id(0);


    pressureAccelerationX[i] = pressureForceX[i] / densities[i];
    pressureAccelerationY[i] = pressureForceY[i] / densities[i];

    // if (i%20 == 0) printf("i=%d dens=%f presForce=%f accel=%f", i, densities[i], pressureForceX[i], pressureAccelerationX[i]);
}