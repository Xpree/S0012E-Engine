//------------------------------------------------------------------------------
//  @file lightserver.cc
//  @copyright (C) 2021 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "config.h"
#include "GL/glew.h"
#include "lightserver.h"
#include "model.h"
#include "cameramanager.h"
#include "core/cvar.h"
#include "core/idpool.h"

namespace Render
{
namespace LightServer
{

struct PointLights
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> colors;
	std::vector<float> radii;
	std::vector<bool> active;
};

glm::vec3 globalLightDirection;
glm::vec3 globalLightColor;

static ModelId icoSphereModel;
static Util::IdPool<PointLightId> pointLightPool;
static Core::CVar* r_draw_light_spheres = nullptr;
static Core::CVar* r_draw_light_sphere_id = nullptr;
static PointLights pointLights;



//------------------------------------------------------------------------------
/**
*/
void
Initialize()
{
	globalLightDirection = glm::normalize(glm::vec3(-0.1f,-0.77735f,-0.27735f));
	globalLightColor = glm::vec3(1.0f,0.8f,0.8f) * 0.5f;

	icoSphereModel = LoadModel("assets/system/icosphere.glb");

	r_draw_light_spheres = Core::CVarCreate(Core::CVarType::CVar_Int, "r_draw_light_spheres", "0");
	r_draw_light_sphere_id = Core::CVarCreate(Core::CVarType::CVar_Int, "r_draw_light_sphere_id", "-1");

}

//------------------------------------------------------------------------------
/**
*/
void
Update(Render::ShaderProgramId pid)
{
	GLuint programHandle = ShaderResource::GetProgramHandle(pid);
	glUniform3fv(glGetUniformLocation(programHandle, "GlobalLightDirection"), 1, &globalLightDirection[0]);
	glUniform3fv(glGetUniformLocation(programHandle, "GlobalLightColor"), 1, &globalLightColor[0]);
}

//------------------------------------------------------------------------------
/**
*/
void
DrawPointLights(Render::ShaderProgramId pid)
{
	glDepthFunc(GL_GEQUAL);
	glCullFace(GL_FRONT);
	GLuint programHandle = ShaderResource::GetProgramHandle(pid);
	GLuint lightPosLocation = glGetUniformLocation(programHandle, "LightPos");
	GLuint lightColorLocation = glGetUniformLocation(programHandle, "LightColor");
	GLuint lightRadiusLocation = glGetUniformLocation(programHandle, "LightRadius");

	Model const& model = GetModel(icoSphereModel);
	Model::Mesh::Primitive const& primitive = model.meshes[0].primitives[0];

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBindVertexArray(primitive.vao);
	for (int i = 0; i < pointLights.active.size(); i++)
	{
		// TODO: we could instead pack the point light array and issue a single instanced drawcall. This does require some indirection from pointlightID to index, but should be a favorable tradeoff
		if (pointLights.active[i])
		{
			glUniform3fv(lightPosLocation, 1, &pointLights.positions[i][0]);
			glUniform3fv(lightColorLocation, 1, &pointLights.colors[i][0]);
			glUniform1f(lightRadiusLocation, pointLights.radii[i]);
			glDrawElements(GL_TRIANGLES, primitive.numIndices, primitive.indexType, (void*)(intptr_t)primitive.offset);
		}
	}
	
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);
	glCullFace(GL_BACK);
	glDepthMask(GL_TRUE);

#if _DEBUG
	if (Core::CVarReadInt(r_draw_light_spheres) > 0)
	{
		static Render::ShaderResourceId const vs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::VERTEXSHADER, "shd/debug.vs");
		static Render::ShaderResourceId const fs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::FRAGMENTSHADER, "shd/debug.fs");
		static ShaderProgramId debugProgram = Render::ShaderResource::CompileShaderProgram({ vs, fs });
		GLuint debugProgramHandle = ShaderResource::GetProgramHandle(debugProgram);
		glUseProgram(debugProgramHandle);

		glm::vec4 color(1, 0, 0, 1);
		glUniform4fv(glGetUniformLocation(debugProgramHandle, "color"), 1, &color.x);

		static GLuint model = glGetUniformLocation(debugProgramHandle, "model");
		static GLuint viewProjection = glGetUniformLocation(debugProgramHandle, "viewProjection");
		Render::Camera* const mainCamera = Render::CameraManager::GetCamera(CAMERA_MAIN);
		glUniformMatrix4fv(viewProjection, 1, GL_FALSE, &mainCamera->viewProjection[0][0]);

		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		int drawId = Core::CVarReadInt(r_draw_light_sphere_id);
		for (int i = 0; i < pointLights.active.size(); i++)
		{
			if (drawId < 0 || i == drawId)
			{
				if (pointLights.active[i])
				{
					glm::mat4 transform = glm::translate(pointLights.positions[i]) * glm::scale(glm::vec3(pointLights.radii[i]));
					glUniformMatrix4fv(model, 1, GL_FALSE, &transform[0][0]);
					glDrawElements(GL_TRIANGLES, primitive.numIndices, primitive.indexType, (void*)(intptr_t)primitive.offset);
				}
			}
		}
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);

		glUseProgram(programHandle);
	}
#endif

	glBindVertexArray(0);
}

//------------------------------------------------------------------------------
/**
*/
bool
IsValid(PointLightId id)
{
	return pointLightPool.IsValid(id);
}

//------------------------------------------------------------------------------
/**
*/
PointLightId
CreatePointLight(glm::vec3 position, glm::vec3 color, float intensity, float radius)
{
	PointLightId id;
	if (pointLightPool.Allocate(id))
	{
		pointLights.positions.push_back(position);
		pointLights.colors.push_back(color * intensity);
		pointLights.radii.push_back(radius);
		pointLights.active.push_back(true);
	}
	else
	{
		pointLights.positions[id.index] = position;
		pointLights.colors[id.index] = color * intensity;
		pointLights.radii[id.index] = radius;
		pointLights.active[id.index] = true;
	}
	return id;
}

//------------------------------------------------------------------------------
/**
*/
void
DestroyPointLight(PointLightId id)
{
	pointLights.active[id.index] = false;
	pointLightPool.Deallocate(id);
}

void 
SetPosition(PointLightId id, glm::vec3 position)
{
	assert(IsValid(id));
	pointLights.positions[id.index] = position;
}

//------------------------------------------------------------------------------
/**
*/
glm::vec3 
GetPosition(PointLightId id)
{
	assert(IsValid(id));
	return pointLights.positions[id.index];
}

//------------------------------------------------------------------------------
/**
*/
void 
SetColorAndIntensity(PointLightId id, glm::vec3 color, float intensity)
{
	assert(IsValid(id));
	pointLights.colors[id.index] = color * intensity;
}

//------------------------------------------------------------------------------
/**
*/
glm::vec3 
GetColorAndIntensity(PointLightId id)
{
	assert(IsValid(id));
	return pointLights.colors[id.index];
}

//------------------------------------------------------------------------------
/**
*/
void 
SetRadius(PointLightId id, float radius)
{
	assert(IsValid(id));
	pointLights.radii[id.index] = radius;
}

//------------------------------------------------------------------------------
/**
*/
float 
GetRadius(PointLightId id)
{
	assert(IsValid(id));
	return pointLights.radii[id.index];
}

//------------------------------------------------------------------------------
/**
*/
		
}
} // namespace Render
