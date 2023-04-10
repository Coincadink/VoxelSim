#include "Renderer.h"

#include <execution>

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
					uint32_t color = PerPixel(x, y);
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

uint32_t Renderer::PerPixel(uint32_t x, uint32_t y)
{
	uint32_t color = 0xff000000;

	for (size_t i = 0; i < m_ActiveScene->Cubes.size(); i++)
	{
		const Cube& cube = m_ActiveScene->Cubes[i];

		glm::vec3 origin = m_ActiveCamera->GetPosition() - cube.pos;
		glm::vec3 dir = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

		ray _ray{
			{origin.x, origin.y, origin.z},
			{dir.x, dir.y, dir.z},
			{1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z}
		};
		
		color += Intersection(&_ray, &cube);
	}
	
	return color;
}