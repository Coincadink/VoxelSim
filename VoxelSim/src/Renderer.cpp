#include "Renderer.h"

#include "Walnut/Random.h"

#include <execution>

/*
namespace Utils {

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
*/

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

	/*
	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];
	*/

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

// void Renderer::Render(const Scene& scene, const Camera& camera)
void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	/*
	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));
	*/
	
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
			[this, y](uint32_t x)
				{
					/*
					glm::vec4 color = PerPixel(x, y);
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
					*/

					uint32_t color = PerPixel(x, y);
					m_ImageData[x + y * m_FinalImage->GetWidth()] = color;
				});
		});

	m_FinalImage->SetData(m_ImageData);

	/*
	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
	*/

	m_FrameIndex = 1;
}

// ---------------------------------------------------

static inline float min(float x, float y) {
	return x < y ? x : y;
}

static inline float max(float x, float y) {
	return x > y ? x : y;
}

struct ray {
	float origin[3];
	float dir[3];
	float dir_inv[3];
};

struct box {
	float corners[2][3];
};

bool intersection(const struct ray* ray, const struct Cube* box) {
	float tmin = 0.0, tmax = INFINITY;

	for (int d = 0; d < 3; ++d) {
		bool sign = signbit(ray->dir_inv[d]);
		float bmin = box->corners[sign][d];
		float bmax = box->corners[!sign][d];

		float dmin = (bmin - ray->origin[d]) * ray->dir_inv[d];
		float dmax = (bmax - ray->origin[d]) * ray->dir_inv[d];

		tmin = max(dmin, tmin);
		tmax = min(dmax, tmax);
	}

	return tmin < tmax;
}

/*
void intersections(const struct ray* ray, size_t nboxes, const struct box boxes[nboxes], float ts[nboxes])
{
	bool signs[3];
	for (int d = 0; d < 3; ++d) {
		signs[d] = signbit(ray->dir_inv[d]);
	}

	for (size_t i = 0; i < nboxes; ++i) {
		const struct box* box = &boxes[i];
		float tmin = 0.0, tmax = ts[i];

		for (int d = 0; d < 3; ++d) {
			float bmin = box->corners[signs[d]][d];
			float bmax = box->corners[!signs[d]][d];

			float dmin = (bmin - ray->origin[d]) * ray->dir_inv[d];
			float dmax = (bmax - ray->origin[d]) * ray->dir_inv[d];

			tmin = max(dmin, tmin);
			tmax = min(dmax, tmax);
		}

		ts[i] = tmin <= tmax ? tmin : ts[i];
	}
}
*/

// ---------------------------------------------------

// glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
uint32_t Renderer::PerPixel(uint32_t x, uint32_t y)
{
	glm::vec3 origin = m_ActiveCamera->GetPosition();
	glm::vec3 dir = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	ray _ray {
		{origin.x, origin.y, origin.z},
		{dir.x, dir.y, dir.z},
		{1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z}
	};

	/*
	box _box{
		{ {-1.0f, -1.0f, 5.0f }, {1.0f, 1.0f, 7.0f} }
	};
	*/

	uint32_t color = 0xff000000;

	/*
	for (size_t i = 0; i < m_ActiveScene->Cubes.size(); i++)
	{
		const Cube& _box = m_ActiveScene->Cubes[i];
		if (intersection(&_ray, &_box))
			color += 0xffff00ff;
	}
	*/

	const Cube& _box = m_ActiveScene->Cubes[0];
	if (intersection(&_ray, &_box))
		color += 0xffff00ff;

	return color;

	/*
	uint32_t nboxes = 10;
	for (int i = 0; i < nboxes; i++)
	{

	}

	float ts[nboxes] = { INFINITY, INFINITY, ... };
	intersections(ray, nobjects, bounding_boxes, ts);

	for (size_t i = 0; i < nobjects; ++i) {
		if (ts[i] < t) {
			if (objects[i]->intersection(ray, &t)) {
				object = objects[i];
			}
		}
	}
	*/

	/*
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 color(0.0f);
	float multiplier = 1.0f;

	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)
		{
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			color += skyColor * multiplier;
			break;
		}

		glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));
		float lightIntensity = glm::max(glm::dot(payload.WorldNormal, -lightDir), 0.0f); // == cos(angle)

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		glm::vec3 sphereColor = material.Albedo;
		sphereColor *= lightIntensity;
		color += sphereColor * multiplier;

		multiplier *= 0.5f;

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		ray.Direction = glm::reflect(ray.Direction,
			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
	}

	return glm::vec4(color, 1.0f);
	*/
}

/*
Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	// (bx^2 + by^2)t^2 + (2(axbx + ayby))t + (ax^2 + ay^2 - r^2) = 0
	// where
	// a = ray origin
	// b = ray direction
	// r = radius
	// t = hit distance

	int closestSphere = -1;
	float hitDistance = std::numeric_limits<float>::max();
	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)
	{
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		glm::vec3 origin = ray.Origin - sphere.Position;

		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(origin, ray.Direction);
		float c = glm::dot(origin, origin) - sphere.Radius * sphere.Radius;

		// Quadratic forumula discriminant:
		// b^2 - 4ac

		float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f)
			continue;

		// Quadratic formula:
		// (-b +- sqrt(discriminant)) / 2a

		// float t0 = (-b + glm::sqrt(discriminant)) / (2.0f * a); // Second hit distance (currently unused)
		float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);
		if (closestT > 0.0f && closestT < hitDistance)
		{
			hitDistance = closestT;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	glm::vec3 origin = ray.Origin - closestSphere.Position;
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += closestSphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}
*/