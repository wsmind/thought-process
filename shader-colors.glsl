//! FRAGMENT

uniform float u[UNIFORM_COUNT];
vec2 resolution = vec2(WIDTH, HEIGHT);

vec2 rotate(vec2 uv, float a)
{
	return mat2(cos(a), sin(a), -sin(a), cos(a)) * uv;
}

float rand(float f)
{
	return fract(sin(f * 12.9898) * 43758.5453);
}

float rand2(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

vec3 randColor(vec3 p)
{
	float id = dot(p, vec3(17.0, 7.0, 13.0));
	return vec3(
		rand(id),
		rand(id + 1.0),
		rand(id + 2.0)
	);
}

float box(vec3 p)
{
	return length(max(abs(p) - vec3(2.0), 0.0)) - 0.8;
}

float tube(vec3 p)
{
	return length(p.xy) - 0.5;
}

float map(vec3 p, out vec3 color)
{
	vec3 index = floor((p + 10.0) / 20.0);
	color = randColor(index);
	p = mod(p + 10.0, 20.0) - 10.0;
	p.xy = rotate(p.xy, u[0] + index.x);
	p.xz = rotate(p.xz, u[0] * 0.7 + index.y);
	return box(p);
}

vec3 normal(vec3 p)
{
	vec2 e = vec2(0.01, 0.0);
	return normalize(vec3(
		map(p + e.xyy) - map(p - e.xyy),
		map(p + e.yxy) - map(p - e.yxy),
		map(p + e.yyx) - map(p - e.yyx)
	));
}

void main(void)
{
	vec2 uv = vec2(gl_FragCoord.xy - resolution.xy * 0.5) / resolution.y;
	vec3 dir = normalize(vec3(uv, 0.5 - length(uv) * 0.4));

	vec3 pos = vec3(0.0, 3.0, -9.0);
	vec3 color;
	float d;
	int i;
	for (i = 0; i < 64; i++)
	{
		d = map(pos, color);
		if (d < 0.001) break;
		pos += dir * d;
	}

	color *= vec3(float(i) / 64.0) * vec3(1.0, 0.7, 0.1);
	//vec3 color = /*vec3(float(i) / 64.0) **/ normal(pos);
	gl_FragColor = vec4(color, 1.0);
}
