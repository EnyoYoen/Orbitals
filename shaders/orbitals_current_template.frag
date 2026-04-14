#version 330 core

uniform vec2 uResolution;
uniform float uYaw;
uniform float uPitch;
uniform float uRadius;
uniform float uFovY;
uniform float uTime;
out vec4 FragColor;

const int MAX_STEPS = 1024;
const float ATTENUATION = 20000.0;
const vec3 bmin = vec3(-150.0);
const vec3 bmax = vec3(150.0);

vec2 complex_mul(vec2 a, vec2 b) {
    return vec2(
        a.x*b.x - a.y*b.y,
        a.x*b.y + a.y*b.x
    );
}

float energy(int n) {
    return -1.0 / float(n*n);
}

vec2 phase(float E, float t) {
    return vec2(cos(E*t), -sin(E*t));
}

// ===== GENERATED FUNCTIONS =====
vec2 psi(vec3 p) { return vec2(0.0); }
// ===================================

vec3 current(vec3 p) {
    float eps = 0.01;

    vec2 psi_x1 = psi(p + vec3(eps,0,0));
    vec2 psi_x2 = psi(p - vec3(eps,0,0));

    vec2 psi_y1 = psi(p + vec3(0,eps,0));
    vec2 psi_y2 = psi(p - vec3(0,eps,0));

    vec2 psi_z1 = psi(p + vec3(0,0,eps));
    vec2 psi_z2 = psi(p - vec3(0,0,eps));

    vec2 dpsidx = (psi_x1 - psi_x2) / (2.0 * eps);
    vec2 dpsidy = (psi_y1 - psi_y2) / (2.0 * eps);
    vec2 dpsidz = (psi_z1 - psi_z2) / (2.0 * eps);

    vec2 w = psi(p);
    vec2 conj_w = vec2(w.x, -w.y);

    vec2 jx = complex_mul(conj_w, dpsidx);
    vec2 jy = complex_mul(conj_w, dpsidy);
    vec2 jz = complex_mul(conj_w, dpsidz);

    vec3 j;
    j.x = jx.y;
    j.y = jy.y;
    j.z = jz.y;

    return j;
}

bool intersectBox(vec3 ro, vec3 rd, out float tmin, out float tmax)
{
    vec3 inv = 1.0 / rd;
    vec3 t0 = (bmin - ro) * inv;
    vec3 t1 = (bmax - ro) * inv;

    vec3 tsmaller = min(t0, t1);
    vec3 tbigger  = max(t0, t1);

    tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    tmax = min(min(tbigger.x, tbigger.y), tbigger.z);

    return tmax > max(tmin, 0.0);
}

vec3 integrateStreamline(vec3 start)
{
    vec3 p = start;
    vec3 accum = vec3(0.0);

    const int STEPS = 24;
    const float STEP_SIZE = 0.5;

    for(int i = 0; i < STEPS; i++)
    {
        vec3 j = current(p);
        float speed = length(j);

        if(speed < 1e-6) break;

        vec3 dir = j / speed;

        vec3 col = 0.5 + 0.5 * dir;
        accum += col * 0.04;

        p += dir * STEP_SIZE;

        // stop if outside bounds
        if(any(greaterThan(abs(p), bmax)))
            break;
    }

    return accum;
}

vec4 raymarch(vec3 ro, vec3 rd) {
    float t0, t1;
    if(!intersectBox(ro, rd, t0, t1))
    {
        return vec4(0.0);
    }

    float t = max(t0, 0.0);
    float dt = (t1 - t0) / float(MAX_STEPS);

    vec3 color = vec3(0.0);
    float alpha = 0.0;

    for(int i = 0; i < MAX_STEPS && t < t1; ++i)
    {
        vec3 p = ro + rd * t;

        vec3 flowColor = integrateStreamline(p);
        float field = length(flowColor);

        float localAlpha = 1.0 - exp(-field * ATTENUATION * dt);
        //float localAlpha = density * 0.08;
        //localAlpha = clamp(localAlpha, 0.0, 0.2);

        vec3 localColor = flowColor;

        color += (1.0 - alpha) * localAlpha * localColor;
        alpha += (1.0 - alpha) * localAlpha;

        if(alpha > 0.99)
            break;

        t += dt;
    }

    return vec4(color, alpha);
}

void main()
{
    vec2 uv = (gl_FragCoord.xy / uResolution) * 2.0 - 1.0;
    uv.x *= uResolution.x / uResolution.y;

    vec3 ro = vec3(
        uRadius * cos(uPitch) * sin(uYaw),
        uRadius * sin(uPitch),
        uRadius * cos(uPitch) * cos(uYaw)
    );
    vec3 forward = normalize(-ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, forward));

    float tanHalfFov = tan(radians(uFovY) * 0.5);
    vec3 rd = normalize(
        forward
        + uv.x * tanHalfFov * right
        + uv.y * tanHalfFov * up
    );
    
    FragColor = raymarch(ro, rd);
}
