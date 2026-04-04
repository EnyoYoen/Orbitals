#version 330 core

uniform vec2 uResolution;
uniform float uYaw;
uniform float uPitch;
uniform float uRadius;
uniform float uFovY;
out vec4 FragColor;

const int MAX_STEPS = 1024;
const float ATTENUATION = 20.0;
const vec3 bmin = vec3(-10.0);
const vec3 bmax = vec3(10.0);

// ===== GENERATED PSI FUNCTION =====

float psi(vec3 p)
{
    float r = length(p);
    if (r < 1e-5) {
        return 0.0;
    }
    float cosTheta = p.z / r;
    float theta = acos(p.z / r);
    float phi = atan(p.y, p.x);

        const int n = 2;
    const int l = 1;
    const int m = 0;

    float et = exp(-r / n);
    float fa = pow(r, l);
    float c = 0.199471;
    float laguerre = 0;
    float x = 1.0f;
    laguerre += x * (1);
    x *= r;
    float legendre = 0;
    x = cosTheta;
    legendre += x * (1);
    x *= r;
    legendre *= pow(1.0f - cosTheta * cosTheta, 0);
    return c * et * fa * laguerre * legendre;
} 
// ===================================

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

        float w = psi(p);
        float density = w * w;

        float localAlpha = 1.0 - exp(-density * ATTENUATION * dt);
        //float localAlpha = density * 0.08;
        //localAlpha = clamp(localAlpha, 0.0, 0.2);

        vec3 localColor = (w > 0.0)
            ? vec3(0.2, 0.4, 1.0)
            : vec3(1.0, 0.5, 0.2);

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
