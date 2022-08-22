//------------------------------------------------------------------------------
//  renderdevice.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "config.h"
#include "renderdevice.h"
#include "model.h"
#include "textureresource.h"
#include "shaderresource.h"
#include "lightserver.h"
#include "cameramanager.h"
#include "debugrender.h"
#include "render/grid.h"

namespace Render
{

#define CAMERA_SHADOW uint('GSHW')
Render::ShaderProgramId directionalLightProgram;
Render::ShaderProgramId pointlightProgram;
Render::ShaderProgramId staticGeometryProgram;
Render::ShaderProgramId staticShadowProgram;
Render::ShaderProgramId skyboxProgram;

GLuint fullscreenQuadVB;
GLuint fullscreenQuadVAO;

GLuint globalShadowMap;
GLuint globalShadowFrameBuffer;
const unsigned int shadowMapSize = 4096;

//------------------------------------------------------------------------------
/**
*/
RenderDevice::RenderDevice() :
    frameSizeW(1024),
    frameSizeH(1024)
{
    // empty
}

void SetupFullscreenQuad()
{
    const float verts[] = {
        -1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, 1.0f,
         1.0f, -1.0f, 1.0f,
         1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, 1.0f
    };
    
    glGenBuffers(1, &fullscreenQuadVB);
    glGenVertexArrays(1, &fullscreenQuadVAO);
    glBindVertexArray(fullscreenQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadVB);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), (void*)&verts, GL_STATIC_DRAW);
    
    glEnableVertexArrayAttrib(fullscreenQuadVAO, 0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RenderDevice::Init()
{
    RenderDevice::Instance();
    LightServer::Initialize();
    TextureResource::Create();
    CameraManager::Create();

    SetupFullscreenQuad();
    
    // Shaders
    {
        auto vs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::VERTEXSHADER, "shd/vs_static.glsl");
        auto fs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::FRAGMENTSHADER, "shd/fs_static.glsl");
        staticGeometryProgram = Render::ShaderResource::CompileShaderProgram({ vs, fs });
    }
    {
        auto vs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::VERTEXSHADER, "shd/vs_static_shadow.glsl");
        auto fs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::FRAGMENTSHADER, "shd/fs_static_shadow.glsl");
        staticShadowProgram = Render::ShaderResource::CompileShaderProgram({ vs, fs });
    }
    {
        auto vs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::VERTEXSHADER, "shd/vs_skybox.glsl");
        auto fs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::FRAGMENTSHADER, "shd/fs_skybox.glsl");
        skyboxProgram = Render::ShaderResource::CompileShaderProgram({ vs, fs });
    }
    {
        auto vs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::VERTEXSHADER, "shd/vs_fullscreen.glsl");
        auto fs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::FRAGMENTSHADER, "shd/fs_directional_light.glsl");
        directionalLightProgram = Render::ShaderResource::CompileShaderProgram({ vs, fs });
    }
    {
        auto vs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::VERTEXSHADER, "shd/vs_pointlight.glsl");
        auto fs = Render::ShaderResource::LoadShader(Render::ShaderResource::ShaderType::FRAGMENTSHADER, "shd/fs_pointlight.glsl");
        pointlightProgram = Render::ShaderResource::CompileShaderProgram({ vs, fs });
    }

    GLint dims[4] = { 0 };
    glGetIntegerv(GL_VIEWPORT, dims);
    // default viewport extents
    GLint fbWidth = dims[2];
    GLint fbHeight = dims[3];
    
    // Setup geometry buffer
    glGenFramebuffers(1, &Instance()->geometryBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, Instance()->geometryBuffer);

    glGenTextures(4, Instance()->renderTargets.RT);

    glBindTexture(GL_TEXTURE_2D, Instance()->renderTargets.albedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbWidth, fbHeight, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, Instance()->renderTargets.normal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, fbWidth, fbHeight, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, Instance()->renderTargets.properties);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, fbWidth, fbHeight, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, Instance()->renderTargets.emissive);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, fbWidth, fbHeight, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
    glGenerateMipmap(GL_TEXTURE_2D);

    glGenTextures(1, &Instance()->depthStencilBuffer);
    glBindTexture(GL_TEXTURE_2D, Instance()->depthStencilBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, fbWidth, fbHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);

    //glGenRenderbuffers(1, &Instance()->depthStencilBuffer);
    //glBindRenderbuffer(GL_RENDERBUFFER, Instance()->depthStencilBuffer);
    //glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, Instance()->frameSizeW, Instance()->frameSizeH);
    
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, Instance()->renderTargets.albedo, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, Instance()->renderTargets.normal, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, Instance()->renderTargets.properties, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, Instance()->renderTargets.emissive, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, Instance()->depthStencilBuffer, 0);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, Instance()->depthStencilBuffer);
    
    const GLenum drawbuffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(4, drawbuffers);

    { GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(err == GL_FRAMEBUFFER_COMPLETE); }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // setup shadow pass
    glGenTextures(1, &globalShadowMap);
    glBindTexture(GL_TEXTURE_2D, globalShadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenFramebuffers(1, &globalShadowFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, globalShadowFrameBuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, globalShadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // setup a shadow camera
    Render::CameraCreateInfo shadowCameraInfo;
    shadowCameraInfo.hash = CAMERA_SHADOW;
    shadowCameraInfo.projection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 500.0f);
    shadowCameraInfo.view = glm::lookAt(glm::vec3(-20.0f, 60.0f, -10.0f),
                                        glm::vec3(0.0f, 0.0f, 0.0f),
                                        glm::vec3(0.0f, 1.0f, 0.0f));
    LightServer::globalLightDirection = shadowCameraInfo.view[2];

    CameraManager::CreateCamera(shadowCameraInfo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    Debug::InitDebugRendering();
    Instance()->grid = new Grid();
}

void RenderDevice::Draw(ModelId model, glm::mat4 localToWorld)
{
    Instance()->drawCommands.push_back({ model, localToWorld });
}

void RenderDevice::StaticGeometryPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, Instance()->geometryBuffer);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Camera* const mainCamera = CameraManager::GetCamera(CAMERA_MAIN);

    this->grid->Draw(&mainCamera->viewProjection[0][0]);

    auto programHandle = Render::ShaderResource::GetProgramHandle(staticGeometryProgram);
    glUseProgram(programHandle);
    glUniformMatrix4fv(glGetUniformLocation(programHandle, "ViewProjection"), 1, false, &mainCamera->viewProjection[0][0]);
    
    GLuint baseColorFactorLocation = glGetUniformLocation(programHandle, "BaseColorFactor");
    GLuint emissiveFactorLocation = glGetUniformLocation(programHandle, "EmissiveFactor");
    GLuint metallicFactorLocation = glGetUniformLocation(programHandle, "MetallicFactor");
    GLuint roughnessFactorLocation = glGetUniformLocation(programHandle, "RoughnessFactor");
    GLuint modelLocation = glGetUniformLocation(programHandle, "Model");
    GLuint alphaCutoffLocation = glGetUniformLocation(programHandle, "AlphaCutoff");

    // Draw opaque first
    for (auto const& cmd : this->drawCommands)
    {
        Model const& model = GetModel(cmd.modelId);
        glUniformMatrix4fv(modelLocation, 1, false, &cmd.transform[0][0]);

        for (auto const& mesh : model.meshes)
        {
            for (auto& primitiveId : mesh.opaquePrimitives)
            {
                auto& primitive = mesh.primitives[primitiveId];

                for (int i = 0; i < Model::Material::NUM_TEXTURES; i++)
                {
                    if (primitive.material.textures[i] != InvalidResourceId)
                    {
                        glActiveTexture(GL_TEXTURE0 + i);
                        glBindTexture(GL_TEXTURE_2D, Render::TextureResource::GetTextureHandle(primitive.material.textures[i]));
                        glUniform1i(i, i);
                    }
                }

                glUniform4fv(baseColorFactorLocation, 1, &primitive.material.baseColorFactor[0]);
                glUniform4fv(emissiveFactorLocation, 1, &primitive.material.emissiveFactor[0]);
                glUniform1f(metallicFactorLocation, primitive.material.metallicFactor);
                glUniform1f(roughnessFactorLocation, primitive.material.roughnessFactor);

                if (primitive.material.alphaMode == Model::Material::AlphaMode::Mask)
                    glUniform1f(alphaCutoffLocation, primitive.material.alphaCutoff);
                else
                    glUniform1f(alphaCutoffLocation, 0);

                glBindVertexArray(primitive.vao);
                glDrawElements(GL_TRIANGLES, primitive.numIndices, primitive.indexType, (void*)(intptr_t)primitive.offset);
            }
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Render::RenderDevice::LightPass()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Camera* const mainCamera = CameraManager::GetCamera(CAMERA_MAIN);
    
    { // Begin directional light drawing
        GLuint programHandle = Render::ShaderResource::GetProgramHandle(directionalLightProgram);
        glUseProgram(programHandle);

        glUniform4fv(glGetUniformLocation(programHandle, "CameraPosition"), 1, &mainCamera->view[3][0]);
        glUniformMatrix4fv(glGetUniformLocation(programHandle, "InvView"), 1, false, &mainCamera->invView[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(programHandle, "InvProjection"), 1, false, &mainCamera->invProjection[0][0]);

        glActiveTexture(GL_TEXTURE16);
        glBindTexture(GL_TEXTURE_2D, globalShadowMap);
        glUniform1i(glGetUniformLocation(programHandle, "GlobalShadowMap"), 16);
        Camera* globalShadowCamera = CameraManager::GetCamera(CAMERA_SHADOW);
        glUniformMatrix4fv(glGetUniformLocation(programHandle, "GlobalShadowMatrix"), 1, false, &globalShadowCamera->viewProjection[0][0]);

        LightServer::Update(directionalLightProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.albedo);
        glUniform1i(0, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.normal);
        glUniform1i(1, 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.properties);
        glUniform1i(2, 2);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.emissive);
        glUniform1i(3, 3);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, this->depthStencilBuffer);
        glUniform1i(4, 4);

        glBindVertexArray(fullscreenQuadVAO);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    } // end directional light drawing

    { // begin drawing point lights
        GLuint programHandle = Render::ShaderResource::GetProgramHandle(pointlightProgram);
        glUseProgram(programHandle);
        
        glUniform4fv(glGetUniformLocation(programHandle, "CameraPosition"), 1, &mainCamera->view[3][0]);
        glUniformMatrix4fv(glGetUniformLocation(programHandle, "InvView"), 1, false, &mainCamera->invView[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(programHandle, "InvProjection"), 1, false, &mainCamera->invProjection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(programHandle, "ViewProjection"), 1, false, &mainCamera->viewProjection[0][0]);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.albedo);
        glUniform1i(0, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.normal);
        glUniform1i(1, 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, this->renderTargets.properties);
        glUniform1i(2, 2);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, this->depthStencilBuffer);
        glUniform1i(4, 4);
        
        LightServer::DrawPointLights(pointlightProgram);
    } // end drawing point lights
}

void RenderDevice::StaticShadowPass()
{
    glViewport(0, 0, shadowMapSize, shadowMapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, globalShadowFrameBuffer);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Camera const* const mainCamera = CameraManager::GetCamera(CAMERA_MAIN);
    Camera* const shadowCamera = CameraManager::GetCamera(CAMERA_SHADOW);

    glm::vec3 camForward = glm::vec3(mainCamera->invView[2]);
    glm::vec3 camUp = glm::vec3(mainCamera->invView[1]);
    glm::vec3 shadowCamOffset = glm::normalize(glm::vec3(-2.0f, 6.0f, -1.0f)) * 250.0f;
    glm::vec3 shadowCamTarget = glm::vec3(mainCamera->invView[3]) - camForward * 45.0f;
    shadowCamTarget.y = 0.0f;
    shadowCamera->view = glm::lookAt(shadowCamTarget + shadowCamOffset,
                                     shadowCamTarget,
                                     glm::vec3(0.0f, 1.0f, 0.0f));

    CameraManager::UpdateCamera(shadowCamera);

    auto programHandle = Render::ShaderResource::GetProgramHandle(staticShadowProgram);
    glUseProgram(programHandle);
    //glUniformMatrix4fv(glGetUniformLocation(programHandle, "ViewProjection"), 1, false, &shadowCamera->viewProjection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(programHandle, "View"), 1, false, &shadowCamera->view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(programHandle, "Projection"), 1, false, &shadowCamera->projection[0][0]);
    
    GLuint baseColorFactorLocation = glGetUniformLocation(programHandle, "BaseColorFactor");
    GLuint modelLocation = glGetUniformLocation(programHandle, "Model");
    GLuint alphaCutoffLocation = glGetUniformLocation(programHandle, "AlphaCutoff");

    // Draw opaque first
    for (auto const& cmd : this->drawCommands)
    {
        Model const& model = GetModel(cmd.modelId);
        glUniformMatrix4fv(modelLocation, 1, false, &cmd.transform[0][0]);

        for (auto const& mesh : model.meshes)
        {
            for (auto& primitiveId : mesh.opaquePrimitives)
            {
                auto& primitive = mesh.primitives[primitiveId];
                
                glActiveTexture(GL_TEXTURE0 + Model::Material::TEXTURE_BASECOLOR);
                glBindTexture(GL_TEXTURE_2D, Render::TextureResource::GetTextureHandle(primitive.material.textures[Model::Material::TEXTURE_BASECOLOR]));
                glUniform1i(Model::Material::TEXTURE_BASECOLOR, Model::Material::TEXTURE_BASECOLOR);
                    
                glUniform4fv(baseColorFactorLocation, 1, &primitive.material.baseColorFactor[0]);
                
                if (primitive.material.alphaMode == Model::Material::AlphaMode::Mask)
                    glUniform1f(alphaCutoffLocation, primitive.material.alphaCutoff);
                else
                    glUniform1f(alphaCutoffLocation, 0);

                glBindVertexArray(primitive.vao);
                glDrawElements(GL_TRIANGLES, primitive.numIndices, primitive.indexType, (void*)(intptr_t)primitive.offset);
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderDevice::SkyboxPass()
{
    Camera* const camera = CameraManager::GetCamera(CAMERA_MAIN);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    GLuint handle = Render::ShaderResource::GetProgramHandle(skyboxProgram);
    glUseProgram(handle);
    glBindVertexArray(fullscreenQuadVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, TextureResource::GetTextureHandle(skybox));
    glUniform1i(0, 0);
    glUniformMatrix4fv(1, 1, false, &camera->invProjection[0][0]);
    glUniformMatrix4fv(2, 1, false, &camera->invView[0][0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthFunc(GL_LESS);
}

void RenderDevice::Render(Display::Window* wnd)
{
    CameraManager::OnBeforeRender();

    wnd->MakeCurrent();

    Instance()->StaticShadowPass();

    int w, h;
    wnd->GetSize(w, h);
    glViewport(0, 0, w, h);

    Instance()->StaticGeometryPass();

    Instance()->LightPass();

    if (Instance()->skybox != InvalidResourceId)
    {
        Instance()->SkyboxPass();
    }
    
    Instance()->drawCommands.clear();

    Debug::DispatchDebugDrawing();


}

} // namespace Render
