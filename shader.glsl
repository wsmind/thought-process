//! FRAGMENT

uniform float _u[UNIFORM_COUNT];
vec2 resolution = vec2(WIDTH, HEIGHT);

vec2 rotate(vec2 uv, float a)
{
	return mat2(cos(a), sin(a), -sin(a), cos(a)) * uv;
}

float vmin(vec3 p)
{
	return min(min(p.x, p.y), p.z);
}

float vmax(vec3 p)
{
	return max(max(p.x, p.y), p.z);
}

float rand(float f)
{
	return fract(sin(f * 12.9898) * 43758.5453);
}

float rand2(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}

vec3 repeat(vec3 p, vec3 period)
{
	return mod(p + period * 0.5, period) - period * 0.5;
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
	vec3 diff = abs(p) - vec3(1.0);
	return length(max(diff, 0.0)) + vmax(min(diff, 0.0));
}

float reverseBox(vec3 p)
{
	vec3 diff = vec3(2.0) - abs(p);
	return vmin(max(diff, 0.0)) - length(min(diff, 0.0));
}

float reverseTube(vec3 p)
{
	return 1.5 - length(p.xy);
}

float cubes(vec3 p)
{
	p.xy += vec2(2.0);
	p = repeat(p, vec3(4.0));
	return box(p);
}

float cubes2(vec3 p)
{
	p.xy += vec2(2.0);
	p.z += floor(_u[0] / 2.0) * 4.0;
	p = repeat(p, vec3(4.0, 4.0, 16.0));
	return box(p);
}

/*float spheres(vec3 p)
{
	p.xy = rotate(p.xy, _u[0] * 0.2);
	p.xy += 1.0 + sin(p.z * 0.1);
	p = repeat(p, vec3(2.0, 2.0, 10.0));
	return length(p) - 0.2;
}*/

float spheres(vec3 p)
{
	float ringSize = 1.4 * (sin(_u[0] * 0.1) * 0.4 + 0.6);
	p.xy = rotate(p.xy, _u[0] * 0.2 + p.z * 0.2);
	p.xy += ringSize;
	p = repeat(p, vec3(ringSize * 2.0, ringSize * 2.0, 10.0));
	return length(p) - 0.2;
}

float corridor(vec3 p)
{
	vec2 e = vec2(2.0) - abs(p.xy);
	float wall = min(e.x, e.y);
	
	float c = cubes(p);
	
	return min(wall, c);
	//return max(wall, -c);
}

float tube(vec3 p)
{
	return length(p.xy) - 0.5;
}

float map(vec3 p)
{
	//p.xy = rotate(p.xy, u[0] * 0.1);
	//p.xz = rotate(p.xz, u[0] * 0.07);
	return min(corridor(p), spheres(p));
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

/*float ao(vec3 p, float md)
{
	vec3 n = normal(p);
	float d = map(p + n * md);
	return smoothstep(-md, md, d);
	//return clamp(d, 0.0, md) / md;
}*/

float ao(vec3 p, vec3 n, float step)
{
	float md = 0.0;
	float ao = 1.0;
	for (int i = 0; i < 5; i++)
	{
		p += n * step;
		md += step;
		ao = min(ao, map(p) / md);
		//ao = min(ao, smoothstep(0.0, md * 2.0, map(p)));
	}
	
	return max(ao, 0.0);
}

vec3 tonemap(vec3 color)
{
	// rheinhard
	color = color / (1.0 + color);
	
	// gamma
	color = pow(color, vec3(1.0 / 2.2));
	
	return color;
}

float light(vec3 p, vec3 n, float d, float range, float energy)
{
	float irradiance = dot(n.xy, -normalize(p.xy)) * 0.4 + 0.6;
	
	float ld = d / range;
	return irradiance * energy / (ld * ld + 1.0);
}

void main(void)
{
	vec2 uv = vec2(gl_FragCoord.xy - resolution.xy * 0.5) / resolution.y;
	uv = rotate(uv, sin(_u[0] * 0.2) * 0.1);
	
	vec3 dir = normalize(vec3(uv, 0.5 - length(uv) * 0.4));

	vec3 pos = vec3(0.0, 0.0, _u[0]);
	float d;
	int i;
	for (i = 0; i < 64; i++)
	{
		d = map(pos);
		if (d < 0.001) break;
		pos += dir * d;
	}

	//vec3 color = vec3(float(i) / 64.0);
	//vec3 color = normal(pos);
	vec3 n = normal(pos);
	float occ = ao(pos, n, 0.04);
	//vec3 color = (ao(pos, n, 0.04) * 0.5 + 0.5);// * (normal(pos) * 0.5 + 0.5);
	vec3 sphereLight = vec3(1.0, 0.1, 0.0) * light(pos, n, spheres(pos), 0.2, 2.0);
	vec3 cubeLight = vec3(0.0, 0.7, 1.0) * light(pos, n, cubes2(pos), 0.1, 4.0) * exp(-fract(_u[0] * 0.5) * 4.0);
	vec3 radiance = sphereLight + cubeLight + occ * 0.01;
	vec3 color = tonemap(radiance);
	gl_FragColor = vec4(color, 1.0);
}
