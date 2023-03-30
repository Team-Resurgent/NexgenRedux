#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#include "OpenGLRenderingHelper.h"
#include "RenderStateManager.h"
#include "ConfigLoader.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <GLAD/glad.h>
#include <map>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include <STB/stb_image.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    const std::string defaultVertexShader =

		"precision mediump float;\n"
		
		"struct pixel_shader_input {\n"
		"    vec4 position;\n"
		"    vec3 w_position;\n"
		"    vec3 normal;\n"
		"    vec3 w_normal;\n"
		"    vec2 uv;\n"
		"    float fog;\n"
		"};\n"

		"struct vertex_shader_input {\n"
		"    vec3 cameraPosition;\n"
		"    vec3 position;\n"
		"    vec3 normal;\n"
		"    vec2 uv;\n"
		"};\n"

		"uniform sampler2D uTextureDiffuse;\n"
		"uniform mat4 uModelMatrix;\n"
		"uniform mat4 uViewMatrix;\n"
		"uniform mat4 uProjectionMatrix;\n"
		"uniform vec4 uAmbientColor;\n"
		"uniform int uLightEnable0;\n"
		"uniform vec4 uLightPosition0;\n"
		"uniform vec4 uLightDiffuseColor0;\n"
		"uniform int uLightEnable1;\n"
		"uniform vec4 uLightPosition1;\n"
		"uniform vec4 uLightDiffuseColor1;\n"
		"uniform int uLightEnable2;\n"
		"uniform vec4 uLightPosition2;\n"
		"uniform vec4 uLightDiffuseColor2;\n"
		"uniform int uLightEnable3;\n"
		"uniform vec4 uLightPosition3;\n"
		"uniform vec4 uLightDiffuseColor3;\n"
		"uniform int uLightsEnable;\n"
		"uniform int uFogMode;\n"
		"uniform vec4 uFogColor;\n"
		"uniform float uFogStart;\n"
		"uniform float uFogEnd;\n"
		"uniform float uFogDensity;\n"
		"uniform vec4 uTintColor;\n"

		"attribute vec3 aPosition;\n"
		"attribute vec3 aNormal;\n"
		"attribute vec2 aTexCoord;\n"

		"pixel_shader_input process(in vertex_shader_input IN)\n"
		"{\n"
		"    pixel_shader_input OUT;\n"
		"    OUT.position = (uModelMatrix * vec4(IN.position, 1.0));\n"
		"    OUT.w_position = OUT.position.xyz;\n"
		"    OUT.fog = length(IN.cameraPosition.xyz - OUT.w_position);\n"
		"    OUT.position = (uViewMatrix * OUT.position);\n"
		"    OUT.position = (uProjectionMatrix * OUT.position);\n"
		"    OUT.normal = IN.normal;\n"
		"    OUT.w_normal = normalize((mat3(uModelMatrix) * IN.normal));\n"
		"    OUT.uv = IN.uv;\n"
		"    return OUT;\n"
		"}\n"

		"varying vec3 vCameraPosition;\n"
		"varying vec3 vPositionW;\n"
		"varying vec3 vNormalW;\n"
		"varying vec3 vNormal;\n"
		"varying vec2 vTexCoord;\n"
		"varying float vFog;\n"

		"void main() {\n"

		"    vertex_shader_input IN;\n"
		"    IN.cameraPosition = vec3(-uViewMatrix[3][0], -uViewMatrix[3][1], -uViewMatrix[3][2]);\n"
		"    IN.position = aPosition;\n"
		"    IN.normal = aNormal;\n"
		"    IN.uv = aTexCoord;\n"

		"    pixel_shader_input result = process(IN);\n"
		"    gl_Position = result.position;\n"

		"    vCameraPosition = IN.cameraPosition;\n"
		"    vPositionW = result.w_position;\n"
		"    vNormalW = result.w_normal;\n"
		"    vNormal = result.normal;\n"
		"    vTexCoord = result.uv;\n"
		"    vFog = result.fog;\n"

		"}";

	const std::string defaultFragmentShader =

		"precision mediump float;\n"

		"struct pixel_shader_input {\n"
		"    vec3 w_position;\n"
		"    vec3 w_normal;\n"
		"    vec3 normal;\n"
		"    vec2 uv;\n"
		"    float fog;\n"
		"};\n"
	
		"uniform sampler2D uTextureDiffuse;\n"
		"uniform mat4 uModelMatrix;\n"
		"uniform mat4 uViewMatrix;\n"
		"uniform mat4 uProjectionMatrix;\n"
		"uniform vec4 uAmbientColor;\n"
		"uniform int uLightEnable0;\n"
		"uniform vec4 uLightPosition0;\n"
		"uniform vec4 uLightDiffuseColor0;\n"
		"uniform int uLightEnable1;\n"
		"uniform vec4 uLightPosition1;\n"
		"uniform vec4 uLightDiffuseColor1;\n"
		"uniform int uLightEnable2;\n"
		"uniform vec4 uLightPosition2;\n"
		"uniform vec4 uLightDiffuseColor2;\n"
		"uniform int uLightEnable3;\n"
		"uniform vec4 uLightPosition3;\n"
		"uniform vec4 uLightDiffuseColor3;\n"
		"uniform int uLightsEnable;\n"
		"uniform int uFogMode;\n"
		"uniform vec4 uFogColor;\n"
		"uniform float uFogStart;\n"
		"uniform float uFogEnd;\n"
		"uniform float uFogDensity;\n"
		"uniform vec4 uTintColor;\n"

		"varying vec3 vCameraPosition;\n"
		"varying vec3 vPositionW;\n"
		"varying vec3 vNormalW;\n"
		"varying vec3 vNormal;\n"
		"varying vec2 vTexCoord;\n"
		"varying float vFog;\n"

		"float saturate_f(float x) {\n"
		"  return clamp(x, 0.0, 1.0);\n"
		"}\n"

		"vec2 saturate_vf2(vec2 x) {\n"
		"  return clamp(x, 0.0, 1.0);\n"
		"}\n"

		"vec3 saturate_vf3(vec3 x) {\n"
		"  return clamp(x, 0.0, 1.0);\n"
		"}\n"

		"vec4 saturate_vf4(vec4 x) {\n"
		"  return clamp(x, 0.0, 1.0);\n"
		"}\n"

		"mat2 saturate_mf2x2(mat2 m) {\n"
		"  return mat2(clamp(m[0], 0.0, 1.0), clamp(m[1], 0.0, 1.0));\n"
		"}\n"

		"mat3 saturate_mf3x3(mat3 m) {\n"
		"  return mat3(clamp(m[0], 0.0, 1.0), clamp(m[1], 0.0, 1.0), clamp(m[2], 0.0, 1.0));\n"
		"}\n"

		"mat4 saturate_mf4x4(mat4 m) {\n"
		"  return mat4(clamp(m[0], 0.0, 1.0), clamp(m[1], 0.0, 1.0), clamp(m[2], 0.0, 1.0), clamp(m[3], 0.0, 1.0));\n"
		"}\n"

		"vec4 process(const pixel_shader_input IN ) {\n"
		
		"    vec4 albedo = texture2D(uTextureDiffuse, vTexCoord.st);\n"
		"    const vec4 diffuse_color = vec4(1.0, 1.0, 1.0, 1.0);\n"

		"    const float attenuation0 = 1.0;\n"
		"    const float attenuation1 = 0.0;\n"
		"    const float attenuation2 = 0.0;\n"

		"    float attenuation;\n"
		"    vec3 diffuse_total = vec3(0.0);\n"
		"    vec3 specular_total = vec3(0.0);\n"

		"    if (uLightEnable0 == 1) {\n"
		"        float d = length(IN.w_position.xyz - uLightPosition0.xyz);\n"
		"        if (d <= uLightPosition0.w) {\n"
		"            float d2 = d * d;\n"
		"            attenuation = (attenuation0 + attenuation1 * d + attenuation2 * d2);\n"
		"        } else {\n"
		"            attenuation = 0.0;\n"
		"        }\n"
		"        vec3 light_dir = normalize(IN.w_position.xyz - uLightPosition0.xyz);\n"
		"        float diffuse_lighting = saturate_f(dot(IN.w_normal, -light_dir));\n"
		"        diffuse_lighting *= (uLightPosition0.w / (length(uLightPosition0.xyz - IN.w_position.xyz) * length(uLightPosition0.xyz - IN.w_position.xyz)));\n"
		"        diffuse_total += (((diffuse_color.xyz * uLightDiffuseColor0.xyz) * diffuse_lighting) * attenuation);\n"
		"        vec3 h = normalize((normalize(vCameraPosition.xyz - IN.w_position.xyz) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot(h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    if (uLightEnable1 == 1) {\n"
		"        float d = length((IN.w_position.xyz - uLightPosition1.xyz));\n"
		"        if (d <= uLightPosition1.w) {\n"
		"            float d2 = d * d;\n"
		"            attenuation = (attenuation0 + attenuation1 * d + attenuation2 * d2);\n"
		"        } else {\n"
		"            attenuation = 0.0;\n"
		"        }\n"
		"        vec3 light_dir = normalize(IN.w_position.xyz - uLightPosition1.xyz);\n"
		"        float diffuse_lighting = saturate_f(dot(IN.w_normal, -light_dir));\n"
		"        diffuse_lighting *= (uLightPosition1.w / (length((uLightPosition1.xyz - IN.w_position.xyz)) * length((uLightPosition1.xyz - IN.w_position.xyz))));\n"
		"        diffuse_total += (((diffuse_color.xyz * uLightDiffuseColor1.xyz) * diffuse_lighting) * attenuation);\n"
		"        vec3 h = normalize((normalize((vCameraPosition.xyz - IN.w_position.xyz)) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot( h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    if (uLightEnable2 == 1) {\n"
		"        float d = length((IN.w_position.xyz - uLightPosition2.xyz));\n"
		"        if (d <= uLightPosition2.w) {\n"
		"            float d2 = d * d;\n"
		"            attenuation = (attenuation0 + attenuation1 * d + attenuation2 * d2);\n"
		"        } else {\n"
		"            attenuation = 0.0;\n"
		"        }\n"
		"        vec3 light_dir = normalize(IN.w_position.xyz - uLightPosition2.xyz);\n"
		"        float diffuse_lighting = saturate_f(dot(IN.w_normal, -light_dir));\n"
		"        diffuse_lighting *= (uLightPosition2.w / (length((uLightPosition2.xyz - IN.w_position.xyz)) * length((uLightPosition2.xyz - IN.w_position.xyz))));\n"
		"        diffuse_total += (((diffuse_color.xyz * uLightDiffuseColor2.xyz) * diffuse_lighting) * attenuation);\n"
		"        vec3 h = normalize((normalize((vCameraPosition.xyz - IN.w_position.xyz)) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot( h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    if (uLightEnable3 == 1) {\n"
		"        float d = length((IN.w_position.xyz - uLightPosition3.xyz));\n"
		"        if (d <= uLightPosition3.w) {\n"
		"            float d2 = d * d;\n"
		"            attenuation = (attenuation0 + attenuation1 * d + attenuation2 * d2);\n"
		"        } else {\n"
		"            attenuation = 0.0;\n"
		"        }\n"
		"        vec3 light_dir = normalize((IN.w_position.xyz - uLightPosition3.xyz));\n"
		"        float diffuse_lighting = saturate_f(dot( IN.w_normal, -light_dir));\n"
		"        diffuse_lighting *= (uLightPosition3.w / (length((uLightPosition3.xyz - IN.w_position.xyz)) * length((uLightPosition3.xyz - IN.w_position.xyz))));\n"
		"        diffuse_total += (((diffuse_color.xyz * uLightDiffuseColor3.xyz) * diffuse_lighting) * attenuation);\n"
		"        vec3 h = normalize((normalize((vCameraPosition.xyz - IN.w_position.xyz)) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot(h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    vec3 ambient = uAmbientColor.xyz;\n"
		"    if (uLightsEnable == 0) {\n"
		"        ambient = vec3( 0.0, 0.0, 0.0);\n"
		"        diffuse_total = vec3( 1.0, 1.0, 1.0);\n"
		"    }\n"

		"    vec4 final_color = vec4((albedo.xyz * ((ambient + diffuse_total) + specular_total)), albedo.w);\n"
		"    final_color = (final_color * uTintColor);\n"

		"    if (uFogMode > 0) {\n"
		"        float fog_coefficient = 1.0;\n"
		"        if (uFogMode == 1) {\n"
		"            fog_coefficient = ((uFogEnd - IN.fog) / (uFogEnd - uFogStart));\n"
		"        } else if (uFogMode == 2) {\n"
		"            fog_coefficient = (1.0 / pow( 2.71828, (IN.fog * uFogDensity)));\n"
		"        } else if (uFogMode == 3) {\n"
		"            fog_coefficient = (1.0 / pow( 2.71828, (((IN.fog * IN.fog) * uFogDensity) * uFogDensity)));\n"
		"        }\n"
		"        float fog_factor = clamp(fog_coefficient, 0.0, 1.0);\n"
		"        final_color.xyz = ((fog_factor * final_color.xyz) + ((1.0 - fog_factor) * uFogColor.xyz));\n"
		"    }\n"

		"    return final_color;\n"

		"}\n"

		"void main() {\n"

		"    pixel_shader_input IN;\n"
		"    IN.w_position = vPositionW;\n"
		"    IN.w_normal = vNormalW;\n"
		"    IN.normal = vNormal;\n"
		"    IN.uv = vTexCoord;\n"
		"    IN.fog = vFog;\n"

		"    gl_FragColor = process(IN);\n"

		"}";

}

OpenGLRenderingHelper::OpenGLRenderingHelper(RenderStateManager *renderStateManager)
{
	m_renderStateManager = renderStateManager;

	m_initialized = false;
 	m_dynamicBuffer = 0;
	m_dynamicBufferSize = 0;
	m_maxTextureContainerID = 0;
}

OpenGLRenderingHelper::~OpenGLRenderingHelper()
{
	DeleteShaders();
	DeleteTextures();
	DeleteDynamicBuffer();
}

bool OpenGLRenderingHelper::Init()
{
    if (m_initialized == true)
    {
        return true;
    }
    m_initialized = true;

	uint32_t program = CompileProgram(defaultVertexShader, defaultFragmentShader);

    CreateShaderLookup("Default");
    AddShaderLookupKeyValue("Default", "Program", program);
    AddShaderLookupKeyValue("Default", "aPosition", glGetAttribLocation(program, "aPosition"));
    AddShaderLookupKeyValue("Default", "aNormal", glGetAttribLocation(program, "aNormal"));
    AddShaderLookupKeyValue("Default", "aTexCoord", glGetAttribLocation(program, "aTexCoord"));
    AddShaderLookupKeyValue("Default", "uModelMatrix", glGetUniformLocation(program, "uModelMatrix"));
    AddShaderLookupKeyValue("Default", "uViewMatrix", glGetUniformLocation(program, "uViewMatrix"));
    AddShaderLookupKeyValue("Default", "uProjectionMatrix", glGetUniformLocation(program, "uProjectionMatrix"));
    AddShaderLookupKeyValue("Default", "uTextureDiffuse", glGetUniformLocation(program, "uTextureDiffuse"));
    AddShaderLookupKeyValue("Default", "uAmbientColor", glGetUniformLocation(program, "uAmbientColor"));
	AddShaderLookupKeyValue("Default", "uLightEnable0", glGetUniformLocation(program, "uLightEnable0"));
    AddShaderLookupKeyValue("Default", "uLightPosition0", glGetUniformLocation(program, "uLightPosition0"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor0", glGetUniformLocation(program, "uLightDiffuseColor0"));
	AddShaderLookupKeyValue("Default", "uLightEnable1", glGetUniformLocation(program, "uLightEnable1"));
    AddShaderLookupKeyValue("Default", "uLightPosition1", glGetUniformLocation(program, "uLightPosition1"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor1", glGetUniformLocation(program, "uLightDiffuseColor1"));
	AddShaderLookupKeyValue("Default", "uLightEnable2", glGetUniformLocation(program, "uLightEnable2"));
    AddShaderLookupKeyValue("Default", "uLightPosition2", glGetUniformLocation(program, "uLightPosition2"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor2", glGetUniformLocation(program, "uLightDiffuseColor2"));
	AddShaderLookupKeyValue("Default", "uLightEnable3", glGetUniformLocation(program, "uLightEnable3"));
    AddShaderLookupKeyValue("Default", "uLightPosition3", glGetUniformLocation(program, "uLightPosition3"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor3", glGetUniformLocation(program, "uLightDiffuseColor3"));
    AddShaderLookupKeyValue("Default", "uLightsEnable", glGetUniformLocation(program, "uLightsEnable"));
    AddShaderLookupKeyValue("Default", "uFogMode", glGetUniformLocation(program, "uFogMode"));
    AddShaderLookupKeyValue("Default", "uFogColor", glGetUniformLocation(program, "uFogColor"));
    AddShaderLookupKeyValue("Default", "uFogStart", glGetUniformLocation(program, "uFogStart"));
    AddShaderLookupKeyValue("Default", "uFogEnd", glGetUniformLocation(program, "uFogEnd"));
    AddShaderLookupKeyValue("Default", "uFogDensity", glGetUniformLocation(program, "uFogDensity"));
    AddShaderLookupKeyValue("Default", "uTintColor", glGetUniformLocation(program, "uTintColor"));

	CreateDynamicBuffer();

	return true;
}

void OpenGLRenderingHelper::SetShader(std::string shaderName)
{
	uint32_t program;
	if (GetShaderLookupValue(shaderName, "Program", program) == false)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("SetShader: Shader '%s' not recognized.", shaderName.c_str()));
		return;
	}
	glUseProgram(program);
}

void OpenGLRenderingHelper::SetModelMatrix(const MathUtility::Matrix4x4& matrix) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uModelMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, matrix.values);
}

void OpenGLRenderingHelper::SetViewMatrix(const MathUtility::Matrix4x4& matrix) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uViewMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, matrix.values);
}

void OpenGLRenderingHelper::SetProjectionMatrix(const MathUtility::Matrix4x4& matrix) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uProjectionMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, matrix.values);
}

void OpenGLRenderingHelper::SetAmbientLight(const MathUtility::Color3F& color) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uAmbientColor", value);
	glUniform4f(value, color.r, color.g, color.b, 0);
}

void OpenGLRenderingHelper::SetTexture(const uint32_t& textureID, const TextureFilter& filter) 
{
	TextureContainer* textureContainer = GetTextureContainer(textureID);
	if (textureContainer == NULL)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("SetTexture: Texture with ID '%i' not found.", textureID));
		return;
	}

	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, textureContainer->texture);
	if (filter == TextureFilterLinear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else if (filter == TextureFilterNearest)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}

void OpenGLRenderingHelper::SetTint(const MathUtility::Color4F& color) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uTintColor", value);
	glUniform4f(value, color.r, color.g, color.b, color.a);
}

void OpenGLRenderingHelper::SetBlend(const BlendOperation& operation) 
{
	if (operation == BlendOperationDisabled)
	{
		glDisable(GL_BLEND);
	}
	else if (operation == BlendOperationAdd)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
	}
	else if (operation == BlendOperationSubtract)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_SUBTRACT);
	}
	else if (operation == BlendOperationReverseSubtract)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	}
}

void OpenGLRenderingHelper::SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor) 
{
	uint32_t srcFactorValue = BlendFactorZero;
	if (srcFactor == BlendFactorZero) 
	{
		srcFactorValue = GL_ZERO;
	}
	else if (srcFactor == BlendFactorOne) 
	{
		srcFactorValue = GL_ONE;
	}
	else if (srcFactor == BlendFactorSrcColor) 
	{
		srcFactorValue = GL_SRC_COLOR;
	}
	else if (srcFactor == BlendFactorOneMinusSrcColor) 
	{
		srcFactorValue = GL_ONE_MINUS_SRC_COLOR;
	}
	else if (srcFactor == BlendFactorDstColor) 
	{
		srcFactorValue = GL_DST_COLOR;
	}
	else if (srcFactor == BlendFactorOneMinusDstColor) 
	{
		srcFactorValue = GL_ONE_MINUS_DST_COLOR;
	}
	else if (srcFactor == BlendFactorSrcAlpha) 
	{
		srcFactorValue = GL_SRC_ALPHA;
	}
	else if (srcFactor == BlendFactorOneMinusSrcAlpha) 
	{
		srcFactorValue = GL_ONE_MINUS_SRC_ALPHA;
	}
	else if (srcFactor == BlendFactorDstAlpha) 
	{
		srcFactorValue = GL_DST_ALPHA;
	}
	else if (srcFactor == BlendFactorOneMinusDstAlpha) 
	{
		srcFactorValue = GL_ONE_MINUS_DST_ALPHA;
	}
	else if (srcFactor == BlendFactorConstantColor) 
	{
		srcFactorValue = GL_CONSTANT_COLOR;
	}
	else if (srcFactor == BlendFactorOneMinusConstantColor) 
	{
		srcFactorValue = GL_ONE_MINUS_CONSTANT_COLOR;
	}
	else if (srcFactor == BlendFactorConstantAlpha) 
	{
		srcFactorValue = GL_CONSTANT_ALPHA;
	}
	else if (srcFactor == BlendFactorOneMinusConstantAlpha) 
	{
		srcFactorValue = GL_ONE_MINUS_CONSTANT_ALPHA;
	}
	else if (srcFactor == BlendFactorSrcAlphaSaturate) 
	{
		srcFactorValue = GL_SRC_ALPHA_SATURATE;
	}

	uint32_t dstFactorValue = BlendFactorZero;
	if (dstFactor == BlendFactorZero) 
	{
		dstFactorValue = GL_ZERO;
	}
	else if (dstFactor == BlendFactorOne) 
	{
		dstFactorValue = GL_ONE;
	}
	else if (dstFactor == BlendFactorSrcColor) 
	{
		dstFactorValue = GL_SRC_COLOR;
	}
	else if (dstFactor == BlendFactorOneMinusSrcColor) 
	{
		dstFactorValue = GL_ONE_MINUS_SRC_COLOR;
	}
	else if (dstFactor == BlendFactorDstColor) 
	{
		dstFactorValue = GL_DST_COLOR;
	}
	else if (dstFactor == BlendFactorOneMinusDstColor) 
	{
		dstFactorValue = GL_ONE_MINUS_DST_COLOR;
	}
	else if (dstFactor == BlendFactorSrcAlpha) 
	{
		dstFactorValue = GL_SRC_ALPHA;
	}
	else if (dstFactor == BlendFactorOneMinusSrcAlpha) 
	{
		dstFactorValue = GL_ONE_MINUS_SRC_ALPHA;
	}
	else if (dstFactor == BlendFactorDstAlpha) 
	{
		dstFactorValue = GL_DST_ALPHA;
	}
	else if (dstFactor == BlendFactorOneMinusDstAlpha) 
	{
		dstFactorValue = GL_ONE_MINUS_DST_ALPHA;
	}
	else if (dstFactor == BlendFactorConstantColor) 
	{
		dstFactorValue = GL_CONSTANT_COLOR;
	}
	else if (dstFactor == BlendFactorOneMinusConstantColor) 
	{
		dstFactorValue = GL_ONE_MINUS_CONSTANT_COLOR;
	}
	else if (dstFactor == BlendFactorConstantAlpha) 
	{
		dstFactorValue = GL_CONSTANT_ALPHA;
	}
	else if (dstFactor == BlendFactorOneMinusConstantAlpha) 
	{
		dstFactorValue = GL_ONE_MINUS_CONSTANT_ALPHA;
	}
	else if (dstFactor == BlendFactorSrcAlphaSaturate) 
	{
		dstFactorValue = GL_SRC_ALPHA_SATURATE;
	}

	glBlendFunc(srcFactorValue, dstFactorValue);
}

void OpenGLRenderingHelper::SetCulling(const CullingOperation& operation) 
{
	if (operation == CullingOperationDisabled) 
	{
		glDisable(GL_CULL_FACE);
	}
	else if (operation == CullingOperationFront) 
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
	else if (operation == CullingOperationBack) 
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
}

void OpenGLRenderingHelper::SetDepth(const DepthOperation& operation) 
{
	if (operation == DepthOperationDisabled) 
	{
		glDisable(GL_DEPTH_TEST);
	}
	else if (operation == DepthOperationNever) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_NEVER);
	}
	else if (operation == DepthOperationLess) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}
	else if (operation == DepthOperationEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_EQUAL);
	}
	else if (operation == DepthOperationGreater) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
	}
	else if (operation == DepthOperationLessOrEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_GREATER);
	}
	else if (operation == DepthOperationNotEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_NOTEQUAL);
	}
	else if (operation == DepthOperationGreaterOrEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_GEQUAL);
	}
	else if (operation == DepthOperationAlways) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
	}
}

void OpenGLRenderingHelper::SetLights(const LightsOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightsEnable", value);
	if (operation == LightsOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == LightsOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLight1(const LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightEnable0", value);
	if (operation == LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightPosition0", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightDiffuseColor0", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);

}

void OpenGLRenderingHelper::SetLight2(const LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightEnable1", value);
	if (operation == LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightPosition1", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightDiffuseColor1", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);
}

void OpenGLRenderingHelper::SetLight3(const LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightEnable2", value);
	if (operation == LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightPosition2", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightDiffuseColor2", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);
}

void OpenGLRenderingHelper::SetLight4(const LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightEnable3", value);
	if (operation == LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightPosition3", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uLightDiffuseColor3", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);
}

void OpenGLRenderingHelper::SetFog(const FogOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uFogMode", value);
	if (operation == FogOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == FogOperationExponent) 
	{
		glUniform1i(value, 1);
	}
	else if (operation == FogOperationExponent2) 
	{
		glUniform1i(value, 2);
	}
	else if (operation == FogOperationLinear) 
	{
		glUniform1i(value, 3);
	}
}

void OpenGLRenderingHelper::SetFogInstance(const MathUtility::Color3F& color, const float& start, const float& end, const float& density) 
{
	uint32_t value;
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uFogColor", value);
	glUniform4f(value, color.r, color.g, color.b, 0);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uFogStart", value);
	glUniform1f(value, start);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uFogEnd", value);
	glUniform1f(value, end);
	GetShaderLookupValue(m_renderStateManager->GetRenderState()->shaderState.shader, "uFogDensity", value);
	glUniform1f(value, density);
}

void OpenGLRenderingHelper::SetViewport(const MathUtility::RectI rect) 
{
	glViewport(rect.x, rect.y, rect.width, rect.height);
}

void OpenGLRenderingHelper::SetScissor(const ScissorOperation& operation, const MathUtility::RectI& rect) 
{
	if (operation == ScissorOperationDisabled) 
	{
		glDisable(GL_SCISSOR_TEST);
	}
	else if (operation == ScissorOperationEnabled) 
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(rect.x, rect.y, rect.width, rect.height);
	}
}

bool OpenGLRenderingHelper::LoadTexture(std::wstring path, uint32_t& textureID)
{	
	std::wstring mappedPath;
	if (ConfigLoader::MapPath(path, mappedPath) == 0)
	{
		return false;
	}

	for (std::map<uint32_t, TextureContainer>::iterator it = m_textureContainerMap.begin(); it != m_textureContainerMap.end(); ++it)
	{
		if (it->second.path != path)
		{
			continue;
		}
		it->second.refCount++;
		textureID = it->first;
		return true;
	}
	
	int width;
	int height;
	int channels;
	unsigned char* data = stbi_load(StringUtility::ToString(mappedPath).c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!data)
	{
		return false;
	}

	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);		
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	stbi_image_free(data);

	uint32_t textureContainerID = ++m_maxTextureContainerID;
	TextureContainer textureContainer;
	textureContainer.texture = textureId;
	textureContainer.width = width;
	textureContainer.height = height;
	textureContainer.path = path;
	textureContainer.refCount = 1;
	m_textureContainerMap.insert(std::pair<int, TextureContainer>(textureContainerID, textureContainer));
	textureID = textureContainerID;
	return true;
}

void OpenGLRenderingHelper::DeleteTexture(const uint32_t& textureID)
{
    std::map<uint32_t, TextureContainer>::iterator it = m_textureContainerMap.find(textureID);
	if (it == m_textureContainerMap.end()) 
	{
		return;
	}

	it->second.refCount--;
	if (it->second.refCount > 0)
	{
		return;
	}
	
	TextureContainer* textureContainer = (TextureContainer*)&it->second;
	uint32_t texture = it->second.texture;
	glDeleteTextures(1, &texture);
	m_textureContainerMap.erase(it);
}

bool OpenGLRenderingHelper::RenderMesh(const std::vector<MeshUtility::Vertex>& mesh)
{
	uint32_t requestedSize =  (uint32_t)(mesh.size() * sizeof(MeshUtility::Vertex));
	ResizeDynamicBufferIfNeeded(requestedSize);

	uint32_t aPosition;
	GetShaderLookupValue("Default", "aPosition", aPosition);
	uint32_t aNormal;
	GetShaderLookupValue("Default", "aNormal", aNormal);
	uint32_t aTexCoord;
	GetShaderLookupValue("Default", "aTexCoord", aTexCoord);

	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, mesh.size() * sizeof(MeshUtility::Vertex), mesh.data());

	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glEnableVertexAttribArray(aPosition);
	glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, position));
	glEnableVertexAttribArray(aNormal);
	glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, normal));
	glEnableVertexAttribArray(aTexCoord);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, texcoord));

	uint32_t vertexCount = (uint32_t)mesh.size();
	if (m_renderStateManager->GetRenderState()->drawModeState.operation == DrawModeOperationTriangles) 
	{
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
	}
	else if (m_renderStateManager->GetRenderState()->drawModeState.operation == DrawModeOperationLines) 
	{
		glDrawArrays(GL_LINES, 0, (GLsizei)vertexCount);
	}
	return true;
}

void OpenGLRenderingHelper::Clear(const MathUtility::Color4F& color) 
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClearDepthf(1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Privates 

void OpenGLRenderingHelper::ResizeDynamicBufferIfNeeded(uint32_t requestedSize)
{
	if (requestedSize <= m_dynamicBufferSize)
	{
		return;
	}
	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glBufferData(GL_ARRAY_BUFFER, requestedSize, nullptr, GL_DYNAMIC_DRAW);
	m_dynamicBufferSize = requestedSize;
}

void OpenGLRenderingHelper::CreateDynamicBuffer()
{
	glGenBuffers(1, &m_dynamicBuffer);
	ResizeDynamicBufferIfNeeded(65535);
}

void OpenGLRenderingHelper::DeleteDynamicBuffer()
{
	if (m_dynamicBufferSize == 0)
	{
		return;
	}
	glDeleteBuffers(1, &m_dynamicBuffer);
}

OpenGLRenderingHelper::TextureContainer* OpenGLRenderingHelper::GetTextureContainer(uint32_t textureID)
{
    std::map<uint32_t, TextureContainer>::iterator it = m_textureContainerMap.find(textureID);
	if (it == m_textureContainerMap.end()) 
	{
		return NULL;
	}
	return (TextureContainer*)&it->second;
}

void OpenGLRenderingHelper::DeleteTextures()
{
	for (std::map<uint32_t, TextureContainer>::iterator it = m_textureContainerMap.begin(); it != m_textureContainerMap.end(); ++it)
	{
		uint32_t texture = it->second.texture;
		glDeleteTextures(1, &texture);
	}
}

uint32_t OpenGLRenderingHelper::CompileShader(uint32_t type, const std::string& source)
{
	uint32_t shader = glCreateShader(type);
	const char* sourceArray[1] = { source.c_str() };
	glShaderSource(shader, 1, sourceArray, NULL);
	glCompileShader(shader);
	int32_t compileResult;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
	if (compileResult == 0)
	{
		int32_t infoLogLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
		std::vector<GLchar> infoLog(infoLogLength);
		glGetShaderInfoLog(shader, (GLsizei)infoLog.size(), NULL, infoLog.data());
		std::string errorMessage = std::string("Shader compilation failed: ");
		errorMessage += std::string(infoLog.begin(), infoLog.end());
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Shader compiled failed: %s", errorMessage.c_str()));
		return 0;
	}
	return shader;
}

uint32_t OpenGLRenderingHelper::CompileProgram(const std::string& vsSource, const std::string& fsSource)
{
	uint32_t program = glCreateProgram();
	if (program == 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Create program failed: %i", glGetError()));
		return 0;
	}

	uint32_t vs = CompileShader(GL_VERTEX_SHADER, vsSource);
	if (vs == 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Compile vertex shader failed.");
		glDeleteShader(vs);
		glDeleteProgram(program);
		return 0;
	}

	uint32_t fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
	if (fs == 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Compile pixel shader failed.");
		glDeleteShader(fs);
		glDeleteProgram(program);
		return 0;
	}

	glAttachShader(program, vs);
	glDeleteShader(vs);
	glAttachShader(program, fs);
	glDeleteShader(fs);
	glLinkProgram(program);

	int32_t linkStatus;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus == 0)
	{
		int32_t infoLogLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
		std::vector<GLchar> infoLog(infoLogLength);
		glGetProgramInfoLog(program, (GLsizei)infoLog.size(), NULL, infoLog.data());
		std::string errorMessage = "Program link failed: ";
		errorMessage += std::string(infoLog.begin(), infoLog.end());
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Program link failed '%s'.", errorMessage.c_str()));
		return 0;
	}

	return program;
}

void OpenGLRenderingHelper::CreateShaderLookup(std::string shaderName)
{
    std::map<std::string, uint32_t> lookupMap;
    m_shaderValueMap.insert(std::pair<std::string, std::map<std::string, uint32_t> >(shaderName, lookupMap));
}

bool OpenGLRenderingHelper::AddShaderLookupKeyValue(std::string shaderName, std::string key, uint32_t value)
{
    std::map<std::string, std::map<std::string, uint32_t> >::iterator lookupIt = m_shaderValueMap.find(shaderName);
	if (lookupIt == m_shaderValueMap.end()) 
	{
		return false;
	}

    std::map<std::string, uint32_t>* lookupMap = &lookupIt->second;
	lookupMap->insert(std::pair<std::string, uint32_t>(key, value));
    return true;
}

bool OpenGLRenderingHelper::GetShaderLookupValue(std::string shaderName, std::string key, uint32_t& value)
{
    std::map<std::string, std::map<std::string, uint32_t> >::iterator lookupIt = m_shaderValueMap.find(shaderName);
	if (lookupIt == m_shaderValueMap.end()) 
	{
		return false;
	}

    std::map<std::string, uint32_t>* lookupMap = &lookupIt->second;
	std::map<std::string, uint32_t>::iterator keyValueIt = lookupMap->find(key);
	if (keyValueIt == lookupMap->end()) 
	{
		return false;
	}

	value = keyValueIt->second;
    return true;
}

void OpenGLRenderingHelper::DeleteShaders()
{
	uint32_t program;
	if (GetShaderLookupValue("Default", "Program", program) == true)
	{
		glDeleteProgram(program);
	}
}

#endif