#version 430
layout(location=0) in vec3 in_WorldSpacePos;
layout(location=1) in vec3 in_Normal;
layout(location=2) in vec4 in_Tangent;
layout(location=3) in vec2 in_TexCoords;

layout(location=0) out vec4 out_Albedo;
layout(location=1) out vec3 out_Normal;
layout(location=2) out vec2 out_Properties;
layout(location=3) out vec3 out_Emissive;

layout(location=0) uniform sampler2D BaseColorTexture;
layout(location=1) uniform sampler2D NormalTexture;
// GLTF 2.0 spec: metallicRoughness = Its green channel contains roughness values and its blue channel contains metalness values.
layout(location=2) uniform sampler2D MetallicRoughnessTexture;
layout(location=3) uniform sampler2D EmissiveTexture;
layout(location=4) uniform sampler2D OcclusionTexture;

uniform vec4 BaseColorFactor;
uniform vec4 EmissiveFactor;
uniform float MetallicFactor;
uniform float RoughnessFactor;
uniform float AlphaCutoff;

vec3 CalcNormal(in vec4 tangent, in vec3 binormal, in vec3 normal, in vec3 bumpData)
{
    mat3 tangentViewMatrix = mat3(tangent.xyz, binormal.xyz, normal.xyz);
    return tangentViewMatrix * ((bumpData.xyz * 2.0f) - 1.0f);
}

void main()
{
	vec4 baseColor = texture(BaseColorTexture,in_TexCoords).rgba * BaseColorFactor;
	
	if (baseColor.a <= AlphaCutoff)
		discard;

    vec4 metallicRoughness = texture(MetallicRoughnessTexture, in_TexCoords) * vec4(1.0f, RoughnessFactor, MetallicFactor, 1.0f);
    vec4 emissive = texture(EmissiveTexture, in_TexCoords) * EmissiveFactor;
    vec4 occlusion = texture(OcclusionTexture, in_TexCoords);
    vec4 normal = texture(NormalTexture, in_TexCoords);

    vec3 binormal = cross(in_Normal, in_Tangent.xyz) * in_Tangent.w;
    vec3 N = (CalcNormal(in_Tangent, binormal, in_Normal, normal.xyz));
    
    // TODO: Occlusion should be multiplied with the diffuse term
    //       Maybe bake occlusion into alpha of albedo?
    out_Albedo = vec4(baseColor.rgb, 1.0f);
    out_Normal = N;
    out_Properties = vec2(metallicRoughness.b, metallicRoughness.g);
    out_Emissive = emissive.rgb;
}