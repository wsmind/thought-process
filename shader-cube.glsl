//! FRAGMENT

uniform float u[UNIFORM_COUNT];
vec2 resolution = vec2(WIDTH, HEIGHT);

vec2 rotate(vec2 uv, float a)
{
  return mat2(cos(a), sin(a), -sin(a), cos(a)) * uv;
}

float box(vec3 p)
{
  return length(max(abs(p) - vec3(2.0), 0.0)) - 0.8;
}

float tube(vec3 p)
{
  return length(p.xy) - 0.5;
}

float map(vec3 p)
{
  p.xy = rotate(p.xy, u[0]);
  p.x = abs(p.x);
  p.xz = rotate(p.xz, u[0] * 0.07);
  p.yz = rotate(p.yz, u[0] * 0.09);
  return min(mix(box(p), tube(p), cos(u[0]) * 0.01), -length(p) + 100.0);
}

vec3 normal(vec3 p)
{
  vec2 e = vec2(0.01, 0.0);
  return normalize(vec3(map(p+e.xyy),map(p+e.yxy),map(p+e.yyx)) - vec3(map(p-e.xyy),map(p-e.yxy),map(p-e.yyx)));
}

float c(vec2 uv, float r, float w)
{
  float l = length(uv);
  return smoothstep(r, r + 0.01, l) * smoothstep(r + w, r + w - 0.01, l);
}

void main(void)
{
  vec2 uv = vec2(gl_FragCoord.xy - resolution.xy * 0.5) / resolution.y;
  uv.x += cos(uv.y * 800.0) * 0.05 * exp(-fract(u[0] * 0.25) * 5.0);
  vec3 dir = normalize(vec3(uv, 0.5 - length(uv) * 0.4));

  vec3 pos = vec3(0.0, 0.0, -9.0);
  float d;
  int i;
  for (i = 0; i < 64; i++)
  {
    d = map(pos);
    if (d < 0.001) break;
    pos += dir * d;
  }

  vec3 color = vec3(float(i) / 64.0);
  float fog = exp(-pos.z * 0.01);
  color = mix(vec3(0.8, 0.9, 1.0), color, fog);
  float cc = c(uv, 0.4, 0.05) + c(uv, 0.45, 0.02) + c(uv, 0.35, 0.02) + smoothstep(0.1, 0.0, fract(uv.x * 20.0));
  color = pow(color, vec3(cc * 2.0 + 1.0));
  gl_FragColor = vec4(color, 1.0);
}

/*void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    gl_FragColor = vec4(uv, 0.0, 1.0);
}*/
