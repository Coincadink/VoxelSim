#include "Renderer.h"

#include <execution>
#include <algorithm>
#include <ctime>

namespace Utils 
{
	static inline float min(float x, float y) { return x < y ? x : y; }
	static inline float max(float x, float y) { return x > y ? x : y; }

	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}
}

struct ray 
{
	float origin[3];
	float dir[3];
	float dir_inv[3];
};

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;

		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;
	
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
			[this, y](uint32_t x)
				{
					uint32_t color = Utils::ConvertToRGBA(PerPixel(glm::vec2(x, y)));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = color;
				});
		});

	m_FinalImage->SetData(m_ImageData);

	m_FrameIndex += 1;
}


uint32_t Renderer::Intersection(const struct ray* ray, const struct Cube* cube)
{
	float tmin = 0.0, tmax = INFINITY;

	for (int d = 0; d < 3; ++d) {
		bool sign = signbit(ray->dir_inv[d]);
		float bmin = cube->corners[sign][d];
		float bmax = cube->corners[!sign][d];

		float dmin = (bmin - ray->origin[d]) * ray->dir_inv[d];
		float dmax = (bmax - ray->origin[d]) * ray->dir_inv[d];

		tmin = Utils::max(dmin, tmin);
		tmax = Utils::min(dmax, tmax);
	}

	if (tmin < tmax)
		return Utils::ConvertToRGBA(glm::vec4(glm::vec3(Utils::max(Utils::min(1.0f / tmin, 1.0f), 0.1f)), 1.0f));

	return 0xff000000;
}

//------------------------------------------------------------------------------------

#define HASHSCALE3 glm::vec3(.1031f, .1030f, .0973f)

#define detail 5
#define steps 300
#define maxdistance 30.0f

//#define drawgrid
#define fog
//#define borders
#define blackborders
//#define raymarchhybrid 100
#define objects
#define emptycells 0.5
#define subdivisions 0.95 //should be higher than emptycells

#define rot(spin) mat2(cos(spin),sin(spin),-sin(spin),cos(spin))

#define sqr(a) (a*a)

float fract(float x)
{
    return x - floor(x);
}

//random function from https://www.shadertoy.com/view/MlsXDf
float rnd(glm::vec4 v) { return fract(4e4f * sin(dot(v, glm::vec4(13.46f, 41.74f, -73.36f, 14.24f)) + 17.34f)); }

//hash function by Dave_Hoskins https://www.shadertoy.com/view/4djSRW
glm::vec3 hash33(glm::vec3 p3)
{
    p3 = fract(p3 * HASHSCALE3);
    p3 += dot(p3, glm::vec3(p3.y + 19.19f, p3.x + 19.19f, p3.z + 19.19f));
    return fract((glm::vec3(p3.x, p3.x, p3.y) + glm::vec3(p3.y, p3.x, p3.x)) * glm::vec3(p3.z, p3.y, p3.x));
}

//0 is empty, 1 is subdivide and 2 is full
int getvoxel(glm::vec3 p, float size) {
#ifdef objects
    if (p.x == 0.0f && p.y == 0.0f) {
        return 0;
    }
#endif

    float val = rnd(glm::vec4(p, size));

    if (val < emptycells) {
        return 0;
    }
    else if (val < subdivisions) {
        return 1;
    }
    else {
        return 2;
    }

    return int(val * val * 3.0f);
}

//ray-cube intersection, on the inside of the cube
glm::vec3 voxel(glm::vec3 ro, glm::vec3 rd, glm::vec3 ird, float size)
{
    size *= 0.5f;

    glm::vec3 hit = -(sign(rd) * (ro - size) - size) * ird;

    return hit;
}

float map(glm::vec3 p, glm::vec3 fp) {
    p -= 0.5f;

    glm::vec3 flipping = floor(hash33(fp) + 0.5f) * 2.0f - 1.0f;

    p *= flipping;

    glm::vec2 q = glm::vec2(abs(length(glm::vec2(p.x, p.y) - 0.5f) - 0.5f), p.z);
    float len = length(q);
    q = glm::vec2(abs(length(glm::vec2(p.y, p.z) - glm::vec2(-0.5f, 0.5f)) - 0.5f), p.x);
    len = Utils::min(len, length(q));
    q = glm::vec2(abs(length(glm::vec2(p.x, p.z) + 0.5f) - 0.5f), p.y);
    len = Utils::min(len, length(q));


    return len - 0.1666f;
}

glm::vec3 findnormal(glm::vec3 p, float epsilon, glm::vec3 fp)
{
    glm::vec2 eps = glm::vec2(0, epsilon);

    glm::vec3 normal = glm::vec3(
        map(p + glm::vec3(eps.y, eps.x, eps.x), fp) - map(p - glm::vec3(eps.y, eps.x, eps.x), fp),
        map(p + glm::vec3(eps.x, eps.y, eps.x), fp) - map(p - glm::vec3(eps.x, eps.y, eps.x), fp),
        map(p + glm::vec3(eps.x, eps.x, eps.y), fp) - map(p - glm::vec3(eps.x, eps.x, eps.y), fp));
    return normalize(normal);
}

glm::vec4 Renderer::PerPixel(glm::vec2 fragCoord)
{
    float time = (float)m_FrameIndex;

    glm::vec4 fragColor = glm::vec4(0.0f);
    glm::vec2 uv = (fragCoord * 2.0f - glm::vec2((float) m_FinalImage->GetWidth(), (float) m_FinalImage->GetHeight())) / (float) m_FinalImage->GetHeight();
    float size = 1.0f;

    glm::vec3 ro = glm::vec3(0.5f + sin(time) * 0.4f, 0.5f + cos(time) * 0.4f, time);
    glm::vec3 rd = normalize(glm::vec3(uv, 1.0f));

    //if the mouse is in the bottom left corner, don't rotate the camera
    /*
    if (length(iMouse.xy) > 40.0) {
        rd.yz *= rot(iMouse.y / iResolution.y * 3.14 - 3.14 * 0.5);
        rd.xz *= rot(iMouse.x / iResolution.x * 3.14 * 2.0 - 3.14);
    }
    */

    glm::vec3 lro = mod(ro, size);
    glm::vec3 fro = ro - lro;
    glm::vec3 ird = 1.0f / max(abs(rd), 0.001f);
    glm::vec3 mask;
    bool exitoct = false;
    int recursions = 0;
    float dist = 0.0f;
    float fdist = 0.0f;
    int i;
    float edge = 1.0f;
    glm::vec3 lastmask;
    glm::vec3 normal = glm::vec3(0.0f);

    //the octree traverser loop
    //each iteration i either:
    // - check if i need to go up a level
    // - check if i need to go down a level
    // - check if i hit a cube
    // - go one step forward if octree cell is empty
    // - repeat if i did not hit a cube
    for (i = 0; i < steps; i++)
    {
        if (dist > maxdistance) break;

        //i go up a level
        if (exitoct)
        {

            glm::vec3 newfro = floor(fro / (size * 2.0f)) * (size * 2.0f);

            lro += fro - newfro;
            fro = newfro;

            recursions--;
            size *= 2.0;

            exitoct = (recursions > 0) && (abs(dot(mod(fro / size + 0.5f, 2.0f) - 1.0f + mask * sign(rd) * 0.5f, mask)) < 0.1f);
        }
        else
        {
            //checking what type of cell it is: empty, full or subdivide
            int voxelstate = getvoxel(fro, size);
            if (voxelstate == 1 && recursions > detail)
            {
                voxelstate = 0;
            }

            if (voxelstate == 1 && recursions <= detail)
            {
                //if(recursions>detail) break;

                recursions++;
                size *= 0.5;

                //find which of the 8 voxels i will enter
                glm::vec3 mask2 = step(glm::vec3(size), lro);
                fro += mask2 * size;
                lro -= mask2 * size;
            }
            //move forward
            else if (voxelstate == 0 || voxelstate == 2)
            {
                //raycast and find distance to nearest voxel surface in ray direction
                //i don't need to use voxel() every time, but i do anyway
                glm::vec3 hit = voxel(lro, rd, ird, size);

                /*if (hit.x < min(hit.y,hit.z)) {
                    mask = glm::vec3(1,0,0);
                } else if (hit.y < hit.z) {
                    mask = glm::vec3(0,1,0);
                } else {
                    mask = glm::vec3(0,0,1);
                }*/
                mask = glm::vec3(lessThan(hit, min(glm::vec3(hit.y, hit.z, hit.x), glm::vec3(hit.z, hit.x, hit.y))));
                float len = dot(hit, mask);
#ifdef objects
                if (voxelstate == 2) {
#ifdef raymarchhybrid
                    //if (length(fro-ro) > 20.0*size) break;
                    glm::vec3 p = lro / size;
                    if (map(p, fro) < 0.0f) {
                        normal = -lastmask * sign(rd);
                        break;
                    }
                    float d = 0.0f;
                    bool hit = false;
                    float e = 0.001f / size;
                    for (int j = 0; j < raymarchhybrid; j++) {
                        float l = map(p, fro);
                        p += l * rd;
                        d += l;
                        if (l < e || d > len / size) {
                            if (l < e) hit = true;
                            d = min(len, d);
                            break;
                        }
                    }
                    if (hit) {
                        dist += d * size;
                        ro += rd * d * size;
                        normal = findnormal(p, e, fro);//(lro-0.5)*2.0;
                        break;
                    }
#else
                    break;
#endif
                }
#endif

                //moving forward in ray direction, and checking if i need to go up a level
                dist += len;
                fdist += len;
                lro += rd * len - mask * sign(rd) * size;
                glm::vec3 newfro = fro + mask * sign(rd) * size;
                exitoct = (floor(newfro / size * 0.5f + 0.25f) != floor(fro / size * 0.5f + 0.25f)) && (recursions > 0);
                fro = newfro;
                lastmask = mask;
            }
        }
#ifdef drawgrid
        glm::vec3 q = abs(lro / size - 0.5f) * (1.0f - lastmask);
        edge = min(edge, -(max(max(q.x, q.y), q.z) - 0.5f) * 80.0f * size);
#endif
    }
    ro += rd * dist;
    if (i < steps && dist < maxdistance)
    {
        float val = fract(dot(fro, glm::vec3(15.23f, 754.345f, 3.454f)));
#ifndef raymarchhybrid
        glm::vec3 normal = -lastmask * sign(rd);
#endif
        glm::vec3 color = sin(val * glm::vec3(39.896f, 57.3225f, 48.25f)) * 0.5f + 0.5f;
        fragColor = glm::vec4(color * (normal * 0.25f + 0.75f), 1.0f);

#ifdef borders
        glm::vec3 q = abs(lro / size - 0.5f) * (1.0f - lastmask);
        edge = clamp(-(max(max(q.x, q.y), q.z) - 0.5f) * 20.0f * size, 0.0f, edge);
#endif
#ifdef blackborders
        fragColor *= edge;
#else
        fragColor = 1.0f - (1.0f - fragColor) * edge;
#endif
    }
    else {
#ifdef blackborders
        fragColor = glm::vec4(edge);
#else
        fragColor = glm::vec4(1.0f - edge);
#endif
    }
#ifdef fog
    fragColor *= 1.0f - dist / maxdistance;
#endif
    fragColor = sqrt(fragColor);

    return fragColor;
}