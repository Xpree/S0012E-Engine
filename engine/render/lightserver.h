#pragma once
//------------------------------------------------------------------------------
/**
	@file lightserver.h

	@copyright
	(C) 2021 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "renderdevice.h"
#include "shaderresource.h"
#include "lightsources.h"

namespace Render
{

namespace LightServer
{
	extern glm::vec3 globalLightDirection;
	extern glm::vec3 globalLightColor;

	void Initialize();
	void Update(Render::ShaderProgramId pid);

    void DrawPointLights(Render::ShaderProgramId pid);

    bool IsValid(PointLightId id);
	PointLightId CreatePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius);
	void DestroyPointLight(PointLightId id);

    void SetPosition(PointLightId id, glm::vec3 position);
    glm::vec3 GetPosition(PointLightId id);
    void SetColorAndIntensity(PointLightId id, glm::vec3 color, float intensity);
    glm::vec3 GetColorAndIntensity(PointLightId id);
    void SetRadius(PointLightId id, float radius);
    float GetRadius(PointLightId id);
};
} // namespace Render
