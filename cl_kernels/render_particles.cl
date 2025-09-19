typedef struct {
    float x, y;
    float r, g, b, a;
} GLParticle;

typedef float4 Color;

__constant Color stops[15] = {
    (float4)(0.0f, 0.0f, 0.2f, 1.0f),
    (float4)(0.0f, 0.0f, 1.0f, 1.0f),
    (float4)(0.4f, 0.6f, 1.0f, 1.0f),
    (float4)(0.4f, 1.0f, 0.6f, 1.0f),
    (float4)(0.0f, 1.0f, 0.0f, 1.0f),
    (float4)(0.6f, 1.0f, 0.4f, 1.0f),
    (float4)(1.0f, 1.0f, 0.4f, 1.0f),
    (float4)(1.0f, 1.0f, 0.0f, 1.0f),
    (float4)(0.8f, 0.7f, 0.0f, 1.0f),
    (float4)(1.0f, 0.6f, 0.2f, 1.0f),
    (float4)(1.0f, 0.5f, 0.0f, 1.0f),
    (float4)(0.8f, 0.3f, 0.0f, 1.0f),
    (float4)(1.0f, 0.3f, 0.3f, 1.0f),
    (float4)(1.0f, 0.0f, 0.0f, 1.0f),
    (float4)(0.6f, 0.0f, 0.0f, 1.0f)
};

Color interpolateColor(float Vn, float Vt) {
    float speed = sqrt(Vn*Vn + Vt*Vt);
    float maxSpeed = 2000.0f;
    float t = fmin(fmax(speed / maxSpeed, 0.0f), 1.0f);

    float scaled = t * (15 - 1);
    int idx = (int)scaled;
    float frac = scaled - (float)idx;

    if (idx >= 14) return stops[14];

    Color c1 = stops[idx];
    Color c2 = stops[idx + 1];

    return (Color)(
        c1.x + frac * (c2.x - c1.x),
        c1.y + frac * (c2.y - c1.y),
        c1.z + frac * (c2.z - c1.z),
        1.0f
    );
}




__kernel void render_particles(
    __global const float* xR,
    __global const float* yR,
    __global const float* Vn,
    __global const float* Vt,
    __global GLParticle* particles,
    const int N,
    const float screen_width,
    const float screen_height
) {

    int i = get_global_id(0);

    Color c = interpolateColor(Vn[i], Vt[i]);

    particles[i].x = xR[i] / screen_width;
    particles[i].y = 1.0f - (yR[i] / screen_height);
    particles[i].r = c[0];
    particles[i].g = c[1];
    particles[i].b = c[2];
    particles[i].a = c[3];

}