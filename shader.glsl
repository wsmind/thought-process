//! FRAGMENT

uniform float _u[UNIFORM_COUNT];
vec2 resolution = vec2(_u[4], _u[5]);

float saturation;
float holeAmount;
float crazy;
float laser;

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

float repeat(float p, float period)
{
	return mod(p + period * 0.5, period) - period * 0.5;
}

vec2 repeat2(vec2 p, vec2 period)
{
	return mod(p + period * 0.5, period) - period * 0.5;
}

vec3 repeat3(vec3 p, vec3 period)
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

float box(vec3 p, vec3 s)
{
	vec3 diff = abs(p) - s;
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
	p = repeat3(p, vec3(4.0));
	return box(p, vec3(1.0));
}

float cubes2(vec3 p)
{
	p.xy += vec2(2.0);
	p.z += floor(_u[0] / 2.0) * 4.0;
	p = repeat3(p, vec3(4.0, 4.0, 16.0));
	return box(p, vec3(1.0));
}

/*float spheres(vec3 p)
{
	p.xy = rotate(p.xy, _u[0] * 0.2);
	p.xy += 1.0 + sin(p.z * 0.1);
	p = repeat3(p, vec3(2.0, 2.0, 10.0));
	return length(p) - 0.2;
}*/

float spheres(vec3 p)
{
	float ringSize = 1.4 * (sin(_u[0] * 0.1) * 0.4 + 0.6) + 50.0 * _u[2];
	p.xy = rotate(p.xy, _u[0] * 0.2 + floor(p.z / 10.0 + 0.2));
	p.xy += ringSize;
	p = repeat3(p, vec3(ringSize * 2.0, ringSize * 2.0, 10.0));
	return length(p) - 0.2;
}

float holes(vec3 p)
{
	p.yz = repeat2(p.yz, vec2(0.4, 1.0));
	return box(p, vec3(4.0, holeAmount * 0.1 + 0.4 * crazy, 1.0));
}

float holes2(vec3 p)
{
	p.z = repeat(p.z, 4.0);
	return box(p, vec3(1.0, holeAmount * 2.0 + 2.0, 1.0));
}

float corridor(vec3 p)
{
	vec2 e = vec2(2.0) - abs(p.xy);
	float wall = min(e.x, e.y);
	
	float c = cubes(p);
	float h = min(holes(p), holes2(p));
	
	return max(min(wall, c), -h);
	//return max(wall, -c);
}

float tube(vec3 p)
{
	return length(p.xy) - 0.1 * laser;
}

float tubes(vec3 p)
{
	p.x += 1000.0 * (ceil(laser) - 1.0);
	p.xy = rotate(p.xy, 0.2 - laser * 0.4);
	return min(tube(p + vec3(1.0, 0.0, 0.0)), tube(p - vec3(1.0, 0.0, 0.0)));
}

float map(vec3 p)
{
	//p.xy = rotate(p.xy, u[0] * 0.1);
	//p.xz = rotate(p.xz, u[0] * 0.07);
	return min(min(corridor(p), spheres(p)), tubes(p));
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
	saturation = step(36.0, _u[0]) * step(0.0, 260.0 - _u[0]) + step(292.0, _u[0]); //sin(_u[0] * 0.5) * 0.5 + 0.5;
	holeAmount = smoothstep(132.0, 164.0, _u[0]) * step(0.0, 260.0 - _u[0]); //sin(_u[0] * 0.1) * 0.5 + 0.5;
	crazy = smoothstep(194.0, 196.0, _u[0]); //sin(_u[0] * 0.2) * 0.5 + 0.5;
	laser = fract(_u[3] * 0.5); /*sin(_u[0] * 0.8) * 0.5 + 0.5;*/
	
	float shake = (-exp(-mod(_u[0] - 4.0, 32.0) * 4.0) + laser * 0.02) * rand(_u[0]);
	
	vec2 uv = vec2(gl_FragCoord.xy - resolution.xy * 0.5) / resolution.y;
	uv = rotate(uv, sin(_u[0] * 0.2) * 0.1 + (crazy * fract(_u[0] * 0.01) + shake) * 6.28);
	
	vec3 dir = normalize(vec3(uv, 0.5 - length(uv) * 0.4));
	vec3 pos = vec3(shake, -shake, _u[0]);
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
	vec3 sphereLight = mix(vec3(1.0), vec3(1.0, 0.1, 0.0), saturation) * light(pos, n, spheres(pos), 0.2, 2.0);
	vec3 cubeLight = mix(vec3(1.0), vec3(0.0, 0.7, 1.0), saturation) * light(pos, n, cubes2(pos), 0.1, 4.0) * exp(-fract(_u[0] * 0.5) * 4.0);
	vec3 holeLight = vec3(1.0, 0.02, 0.0) * light(pos, n, pos.y + mix(2.0, sin(_u[0] * 0.2) * 20.0, crazy) + holeAmount * 2.0, 0.3, 10.0) * holeAmount;
	vec3 tubeLight = vec3(10.0, 0.0, 0.02) * light(pos, n, tubes(pos), 0.2 + laser * 0.2, laser);
	vec3 radiance = sphereLight + cubeLight + holeLight + tubeLight + occ * 0.01;
	vec3 color = tonemap(radiance);
	
	// white flashes
	float flash = exp(-mod(_u[0] - 4.0, 32.0) * 0.5);
	color = mix(color, vec3(1.0), flash);
	
	// fade to black
	color = mix(color, vec3(0.0), _u[1]);
	
	gl_FragColor = vec4(color, 1.0);
}
