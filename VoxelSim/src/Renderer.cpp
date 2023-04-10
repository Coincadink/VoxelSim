#include "Renderer.h"

#include <execution>
#include <algorithm>

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

	m_FrameIndex = 1;
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

#define pi acos(-1.0f)
#define eps 1.0f / m_FinalImage->GetHeight()
#define MAX_LEVEL 8.0f
#define FAR 100.0f

float fract(float x)
{
	return x - floor(x);
}

float step(float edge, float x) 
{
	if (x < edge) {
		return 0.0f;
	}
	else {
		return 1.0f;
	}
}

float hash13(glm::vec3 p3)
{
	p3 = fract(p3 * 0.1031f);
	p3 += dot(p3, glm::vec3(p3.z, p3.y, p3.x) + 31.32f);
	return fract((p3.x + p3.y) * p3.z);
}

float mapQ(glm::vec3 p) 
{
	float s = 0.5f;
	for (float i = 1.0f; i < MAX_LEVEL; i++) 
	{
		s *= 2.0f;
		i += step(hash13(floor(p * s)), 0.5f) * MAX_LEVEL;
	}
	return s;
}

float calcT(glm::vec3 p, glm::vec3 rd, glm::vec3 delta) 
{
	float s = mapQ(p);
	glm::vec3 t = glm::vec3(
		rd.x < 0. ? ((p.x * s - floor(p.x * s)) / s) * delta.x : ((ceil(p.x * s) - p.x * s) / s) * delta.x,
		rd.y < 0. ? ((p.y * s - floor(p.y * s)) / s) * delta.y : ((ceil(p.y * s) - p.y * s) / s) * delta.y,
		rd.z < 0. ? ((p.z * s - floor(p.z * s)) / s) * delta.z : ((ceil(p.z * s) - p.z * s) / s) * delta.z
	);
	return Utils::min(t.x, Utils::min(t.y, t.z)) + 0.01;
}

glm::vec4 Renderer::PerPixel(glm::vec2 fragCoord)
{
	glm::vec2 uv = (fragCoord * 2.0f - glm::vec2((float) m_FinalImage->GetWidth(), (float) m_FinalImage->GetHeight())) / (float) m_FinalImage->GetHeight();
	glm::vec3 fwd = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 right = normalize(cross(up, fwd));
	up = cross(fwd, right);
	glm::vec3 ro = glm::vec3(0.0f);
	glm::vec3 rd = right * uv.x + up * uv.y + fwd;
	rd = normalize(rd);
	glm::vec3 delta = 1.0f / abs(rd);
	float t = 0.0f;
	for (float i = 0.; i < FAR; i++) 
	{
		glm::vec3 pos = ro + rd * t;
		float ss = mapQ(pos);
		glm::vec3 shi = abs(floor(pos - ro)) - 1.0f;
		float hole = Utils::max(shi.x, shi.z);
		if (hole > 0.001 && hash13(floor((pos)*ss)) > 0.4f) break;
		t += calcT(pos, rd, delta);
	}
	return glm::vec4(pow(glm::vec3(std::clamp(1.0f - t / 7.0f, 0.0f, 1.0f)) * 1.3f, glm::vec3(4.0f)), 1.0f);
}