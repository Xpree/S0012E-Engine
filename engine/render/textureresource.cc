//------------------------------------------------------------------------------
//  textureresource.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "config.h"
#include "textureresource.h"
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"

namespace Render
{

TextureResource* TextureResource::instance = nullptr;

void TextureResource::Create()
{
    assert(TextureResource::instance == nullptr);
    TextureResource::instance = new TextureResource();
    
    // setup default textures
    ImageCreateInfo info;
    info.extents = { 1,1 };
    info.type = Render::ImageType::TEXTURE_2D;
    ImageId imageIds[4] =
    {
        AllocateImage(info),
        AllocateImage(info),
        AllocateImage(info),
        AllocateImage(info)
    };

    byte colors[4][4] = {
        {255,255,255,255}, // white texture
        {0,0,0,0}, // black texture
        {0, 255, 0, 0}, // metallic roughness texture (g = roughness, b = metalness)
        {128,128,255,0} // flat normal texture
    };

    for (int i = 0; i < 4; i++)
    {
        Image& image = instance->images[imageIds[i]];

        glBindTexture(GL_TEXTURE_2D, image.handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        void* buffer = (void*)colors[i];
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.extent.w, image.extent.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    instance->whiteTexture = imageIds[0];
    instance->blackTexture = imageIds[1];
    instance->defaultMetallicRoughnessTexture = imageIds[2];
    instance->defaultNormalTexture = imageIds[3];
}

void TextureResource::Destroy()
{
    assert(TextureResource::instance != nullptr);
    // TODO: Resource cleanup
    delete TextureResource::instance;
}

TextureResourceId TextureResource::LoadTexture(const char * path, MagFilter mag, MinFilter min, WrappingMode wrapModeS, WrappingMode wrapModeT, bool sRGB = false)
{
    int w, h, n; //Width, Height, components per pixel (ex. RGB = 3, RGBA = 4)
    unsigned char *image = stbi_load(path, &w, &h, &n, STBI_default);

    if (image == nullptr)
    {
        printf("Could not find texture file!\n");
        assert(false);
    }

    GLuint handle;
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLenum)min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLenum)mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLenum)wrapModeS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLenum)wrapModeT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // If there's no alpha channel, use RGB colors. else: use RGBA.
    if (n == 3)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, sRGB ? GL_SRGB : GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    }
    else if (n == 4)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, sRGB ? GL_SRGB_ALPHA : GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(image);

    Image img;
    img.handle = handle;
    img.extent = { (unsigned)w, (unsigned)h };
    img.type = ImageType::TEXTURE_2D;

    ImageId iid = (ImageId)Instance()->images.size();
    Instance()->images.push_back(img);
    Instance()->imageRegistry.emplace(path, iid);
    return iid;
}

TextureResourceId TextureResource::LoadTextureFromMemory(std::string name, void* buffer, uint64_t bytes, ImageId imageId, MagFilter mag, MinFilter min, WrappingMode wrapModeS, WrappingMode wrapModeT, bool sRGB = false)
{
	assert(Instance()->imageRegistry.count(name) == 0);
    int channels;
    Image& image = Instance()->images[imageId];
    void* decompressed = stbi_load_from_memory((uchar*)buffer, (int)bytes, (int*)&image.extent.w, (int*)&image.extent.h, (int*)&channels, 0);
    glBindTexture(GL_TEXTURE_2D, image.handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLenum)min);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLenum)mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLenum)wrapModeS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLenum)wrapModeT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // If there's no alpha channel, use RGB colors. else: use RGBA.
    if (channels == 3)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, sRGB ? GL_SRGB : GL_RGB, image.extent.w, image.extent.h, 0, GL_RGB, GL_UNSIGNED_BYTE, decompressed);
    }
    else if (channels == 4)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, sRGB ? GL_SRGB_ALPHA : GL_RGBA, image.extent.w, image.extent.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, decompressed);
    }

    stbi_image_free(decompressed);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    Instance()->imageRegistry.emplace(name, imageId);
    return imageId;
}

TextureResourceId TextureResource::LoadCubemap(std::string const& name, std::vector<const char*> const& paths, bool sRGB = false)
{
    GLuint handle;
    glGenTextures(1, &handle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, handle);

    int w, h, nrChannels;
    for (unsigned int i = 0; i < paths.size(); i++)
    {
        unsigned char* data = stbi_load(paths[i], &w, &h, &nrChannels, 0);
        assert(data);
        
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0, sRGB ? GL_SRGB : GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data
        );
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    Image img;
    img.handle = handle;
    img.extent = { (unsigned)w, (unsigned)h };
    img.type = ImageType::TEXTURE_2D;

    ImageId iid = (ImageId)Instance()->images.size();
    Instance()->images.push_back(img);
    Instance()->imageRegistry.emplace(name, iid);
    return iid;
}

ImageId TextureResource::AllocateImage(ImageCreateInfo info)
{
    GLuint handle;
    glGenTextures(1, &handle);
    
    Image img;
    img.handle = handle;
    img.extent = info.extents;
    img.type = info.type;

    ImageId iid = (ImageId)Instance()->images.size();
    Instance()->images.push_back(img);
    return iid;
}

ImageExtents TextureResource::GetImageExtents(ImageId id)
{
    return Instance()->images[id].extent;
}

GLuint TextureResource::GetTextureHandle(TextureResourceId tid)
{
    return Instance()->images[tid].handle;
}

GLuint TextureResource::GetImageHandle(ImageId tid)
{
    return Instance()->images[tid].handle;
}

ImageId TextureResource::GetImageId(std::string name)
{
    auto index = Instance()->imageRegistry.find(name);
    if (index != Instance()->imageRegistry.end())
    {
        return (*index).second;
    }
    return InvalidImageId;
}

TextureResourceId TextureResource::GetWhiteTexture()
{
    return Instance()->whiteTexture;
}

TextureResourceId TextureResource::GetBlackTexture()
{
    return Instance()->blackTexture;
}

TextureResourceId TextureResource::GetDefaultMetallicRoughnessTexture()
{
    return Instance()->defaultMetallicRoughnessTexture;
}

TextureResourceId TextureResource::GetDefaultNormalTexture()
{
    return Instance()->defaultNormalTexture;
}

} // namespace Render

