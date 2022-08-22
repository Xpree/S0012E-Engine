#pragma once
//------------------------------------------------------------------------------
/**
    RenderDevice

    (C) 2019 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "GL/glew.h"
#include <string>
#include <vector>
#include "render/window.h"

namespace Render
{

class Grid;

typedef unsigned int ResourceId;
static const ResourceId InvalidResourceId = UINT_MAX;

typedef ResourceId MeshResourceId;
typedef ResourceId TextureResourceId;
typedef ResourceId VertexShaderResourceId;
typedef ResourceId FragmentShaderResourceId;
typedef ResourceId ShaderResourceId;
typedef ResourceId ShaderProgramId;

typedef unsigned ImageId;
static const ImageId InvalidImageId = UINT_MAX;
typedef unsigned ModelId;

class RenderDevice
{
private:
    RenderDevice();
    static RenderDevice* Instance()
    {
        static RenderDevice instance;
        return &instance;
    }
public:
    RenderDevice(const RenderDevice&) = delete;
    void operator=(const RenderDevice&) = delete;

    static void Init();
    static void Draw(ModelId model, glm::mat4 localToWorld);
    static void Render(Display::Window* wnd);
    static void SetSkybox(TextureResourceId tex);

private:
    struct DrawCommand
    {
        ModelId modelId;
        glm::mat4 transform;
    };
    std::vector<DrawCommand> drawCommands;

    void StaticShadowPass();
    void StaticGeometryPass();
    void LightPass();
    void SkyboxPass();

    GLuint geometryBuffer;
    union RenderTargets
    {
        GLuint RT[4];
        struct
        {
            GLuint albedo;     // GL_RGBA8
            GLuint normal;     // GL_R11F_G11F_B10F (signed values, 0 to 1)
            GLuint properties; // GL_RG16F (metallic, roughness
            GLuint emissive;   // GL_RGB9_E5 (this could be problematic on some systems)
        };
    } renderTargets;
    GLuint depthStencilBuffer; // GL_DEPTH24_STENCIL8
    unsigned int frameSizeW;
    unsigned int frameSizeH;
    Render::Grid* grid;
    TextureResourceId skybox = InvalidResourceId;
};

inline void RenderDevice::SetSkybox(TextureResourceId tex)
{
    Instance()->skybox = tex;
}

} // namespace Render
