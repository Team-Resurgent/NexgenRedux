#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#include "OpenGLRenderingHelper.h"
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
    const std::string vertexShader =

		"precision mediump float;\n"

		"uniform sampler2D uTextureDiffuse;\n"
		"uniform mat4 uModelMatrix;\n"
		"uniform mat4 uViewMatrix;\n"
		"uniform mat4 uProjectionMatrix;\n"
		"uniform vec4 uCameraPosition;\n"
		"uniform vec4 uAmbientColor;\n"
		"uniform vec4 uLightPosition0;\n"
		"uniform vec4 uLightDiffuseColor0;\n"
		"uniform vec4 uLightPosition1;\n"
		"uniform vec4 uLightDiffuseColor1;\n"
		"uniform vec4 uLightPosition2;\n"
		"uniform vec4 uLightDiffuseColor2;\n"
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

		"struct pixel_shader_input {\n"
		"    vec4 position;\n"
		"    vec3 w_position;\n"
		"    vec3 normal;\n"
		"    vec3 w_normal;\n"
		"    vec2 uv;\n"
		"    float fog;\n"
		"};\n"

		"struct vertex_shader_input {\n"
		"    vec3 position;\n"
		"    vec3 normal;\n"
		"    vec2 uv;\n"
		"};\n"

		"pixel_shader_input process(in vertex_shader_input IN)\n"
		"{\n"
		"    pixel_shader_input OUT;\n"
		"    OUT.position = (uModelMatrix * vec4(IN.position, 1.0));\n"
		"    OUT.w_position = OUT.position.xyz;\n"
		"    OUT.fog = length(uCameraPosition.xyz - OUT.w_position);\n"
		"    OUT.position = (uViewMatrix * OUT.position);\n"
		"    OUT.position = (uProjectionMatrix * OUT.position);\n"
		"    OUT.normal = IN.normal;\n"
		"    OUT.w_normal = normalize((mat3(uModelMatrix) * IN.normal));\n"
		"    OUT.uv = IN.uv;\n"
		"    return OUT;\n"
		"}\n"

		"varying vec3 vPositionW;\n"
		"varying vec3 vNormalW;\n"
		"varying vec3 vNormal;\n"
		"varying vec2 vTexCoord;\n"
		"varying float vFog;\n"

		"void main() {\n"

		"    vertex_shader_input IN;\n"
		"    IN.position = aPosition;\n"
		"    IN.normal = aNormal;\n"
		"    IN.uv = aTexCoord;\n"

		"    pixel_shader_input result = process(IN);\n"
		"    gl_Position = result.position;\n"

		"    vPositionW = result.w_position;\n"
		"    vNormalW = result.w_normal;\n"
		"    vNormal = result.normal;\n"
		"    vTexCoord = result.uv;\n"
		"    vFog = result.fog;\n"

		"}";

	const std::string fragmentShader =

		"precision mediump float;\n"
	
		"uniform sampler2D uTextureDiffuse;\n"
		"uniform mat4 uModelMatrix;\n"
		"uniform mat4 uViewMatrix;\n"
		"uniform mat4 uProjectionMatrix;\n"
		"uniform vec4 uCameraPosition;\n"
		"uniform vec4 uAmbientColor;\n"
		"uniform vec4 uLightPosition0;\n"
		"uniform vec4 uLightDiffuseColor0;\n"
		"uniform vec4 uLightPosition1;\n"
		"uniform vec4 uLightDiffuseColor1;\n"
		"uniform vec4 uLightPosition2;\n"
		"uniform vec4 uLightDiffuseColor2;\n"
		"uniform vec4 uLightPosition3;\n"
		"uniform vec4 uLightDiffuseColor3;\n"
		"uniform int uLightsEnable;\n"
		"uniform int uFogMode;\n"
		"uniform vec4 uFogColor;\n"
		"uniform float uFogStart;\n"
		"uniform float uFogEnd;\n"
		"uniform float uFogDensity;\n"
		"uniform vec4 uTintColor;\n"

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

		"struct pixel_shader_input {\n"
		"    vec3 w_position;\n"
		"    vec3 w_normal;\n"
		"    vec3 normal;\n"
		"    vec2 uv;\n"
		"    float fog;\n"
		"};\n"

		"vec4 process(const pixel_shader_input IN ) {\n"
		
		"    vec4 albedo = texture2D(uTextureDiffuse, vTexCoord.st);\n"
		"    const vec4 diffuse_color = vec4(1.0, 1.0, 1.0, 1.0);\n"

		"    const float attenuation0 = 1.0;\n"
		"    const float attenuation1 = 0.0;\n"
		"    const float attenuation2 = 0.0;\n"

		"    float attenuation;\n"
		"    vec3 diffuse_total = vec3(0.0);\n"
		"    vec3 specular_total = vec3(0.0);\n"

		"    if (uLightDiffuseColor0.w >= 0.0) {\n"
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
		"        vec3 h = normalize((normalize(uCameraPosition.xyz - IN.w_position.xyz) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot(h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    if (uLightDiffuseColor1.w >= 0.0) {\n"
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
		"        vec3 h = normalize((normalize((uCameraPosition.xyz - IN.w_position.xyz)) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot( h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    if (uLightDiffuseColor2.w >= 0.0) {\n"
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
		"        vec3 h = normalize((normalize((uCameraPosition.xyz - IN.w_position.xyz)) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot( h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    if (uLightDiffuseColor3.w >= 0.0) {\n"
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
		"        vec3 h = normalize((normalize((uCameraPosition.xyz - IN.w_position.xyz)) - light_dir));\n"
		"        float spec_lighting = pow(saturate_f(dot(h, IN.normal)), 8.0);\n"
		"        specular_total += ((uLightDiffuseColor1.xyz * spec_lighting) * attenuation);\n"
		"    }\n"

		"    vec3 ambient = uAmbientColor.xyz;\n"
		"    if (uLightsEnable == 0) {\n"
		"        ambient = vec3( 0.0, 0.0, 0.0);\n"
		"        diffuse_total = vec3( 1.0, 1.0, 1.0);\n"
		"    }\n"

		"    vec4 final_color = vec4( (albedo.xyz * ((ambient + diffuse_total) + specular_total)), albedo.w);\n"
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

    GLuint CompileShader(GLenum type, const std::string& source)
    {
        GLuint shader = glCreateShader(type);
        const char* sourceArray[1] = { source.c_str() };
        glShaderSource(shader, 1, sourceArray, NULL);
        glCompileShader(shader);
        GLint compileResult;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
        if (compileResult == 0)
        {
            GLint infoLogLength;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
            std::vector<GLchar> infoLog(infoLogLength);
            glGetShaderInfoLog(shader, (GLsizei)infoLog.size(), NULL, infoLog.data());
            std::wstring errorMessage = std::wstring(L"Shader compilation failed: ");
            errorMessage += std::wstring(infoLog.begin(), infoLog.end());
            return 0;
        }
        return shader;
    }

    GLuint CompileProgram(const std::string& vsSource, const std::string& fsSource)
    {
        GLuint program = glCreateProgram();
        if (program == 0)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Creare program failed: %i", glGetError());
            return 0;
        }

        GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
        if (vs == 0)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Compile vertex shader failed.");
            glDeleteShader(vs);
            glDeleteProgram(program);
            return 0;
        }

        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
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

        GLint linkStatus;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus == 0)
        {
            GLint infoLogLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
            std::vector<GLchar> infoLog(infoLogLength);
            glGetProgramInfoLog(program, (GLsizei)infoLog.size(), NULL, infoLog.data());
            std::string errorMessage = "Program link failed: ";
            errorMessage += std::string(infoLog.begin(), infoLog.end());
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Program link failed.");
            return 0;
        }

        return program;
    }

    bool m_initialized = false;
    std::map<std::string, std::map<std::string, uint32_t>> m_shaderValueMap;

	uint32_t m_dynamicBuffer;
	uint32_t m_dynamicBufferSize = 0;

    uint32_t m_maxTextureContainerID = 0;
	std::map<uint32_t, OpenGLRenderingHelper::TextureContainer> m_textureContainerMap;
}

void OpenGLRenderingHelper::Close(void)
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

	uint32_t program = CompileProgram(vertexShader, fragmentShader);

    CreateShaderLookup("Default");
    AddShaderLookupKeyValue("Default", "Program", program);
    AddShaderLookupKeyValue("Default", "aPosition", glGetAttribLocation(program, "aPosition"));
    AddShaderLookupKeyValue("Default", "aNormal", glGetAttribLocation(program, "aNormal"));
    AddShaderLookupKeyValue("Default", "aTexCoord", glGetAttribLocation(program, "aTexCoord"));
    AddShaderLookupKeyValue("Default", "uModelMatrix", glGetUniformLocation(program, "uModelMatrix"));
    AddShaderLookupKeyValue("Default", "uViewMatrix", glGetUniformLocation(program, "uViewMatrix"));
    AddShaderLookupKeyValue("Default", "uProjectionMatrix", glGetUniformLocation(program, "uProjectionMatrix"));
    AddShaderLookupKeyValue("Default", "uTextureDiffuse", glGetUniformLocation(program, "uTextureDiffuse"));
	AddShaderLookupKeyValue("Default", "uCameraPosition", glGetUniformLocation(program, "uCameraPosition"));
    AddShaderLookupKeyValue("Default", "uAmbientColor", glGetUniformLocation(program, "uAmbientColor"));
    AddShaderLookupKeyValue("Default", "uLightPosition0", glGetUniformLocation(program, "uLightPosition0"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor0", glGetUniformLocation(program, "uLightDiffuseColor0"));
    AddShaderLookupKeyValue("Default", "uLightPosition1", glGetUniformLocation(program, "uLightPosition1"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor1", glGetUniformLocation(program, "uLightDiffuseColor1"));
    AddShaderLookupKeyValue("Default", "uLightPosition2", glGetUniformLocation(program, "uLightPosition2"));
    AddShaderLookupKeyValue("Default", "uLightDiffuseColor2", glGetUniformLocation(program, "uLightDiffuseColor2"));
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

bool OpenGLRenderingHelper::SetShader(std::string shaderName)
{
	uint32_t program;
	if (GetShaderLookupValue(shaderName, "Program", program) == false)
	{
		return false;
	}
	glUseProgram(program);

	MathUtility::Vec3F eye = MathUtility::Vec3F(0, 0, 2);
	MathUtility::Vec3F target = MathUtility::Vec3F(0, 0, 0);
	MathUtility::Vec3F up = MathUtility::Vec3F(0, 1, 0);
	MathUtility::Matrix4x4 viewMatrix = MathUtility::Matrix4x4::LookAtRH(eye, target, up);
	MathUtility::Matrix4x4 modelMatrix = MathUtility::Matrix4x4();
	MathUtility::Matrix4x4 perspectiveMatrix = MathUtility::Matrix4x4::OrthoOffCenterRH(0, 640, 0, 480, 1, 100);

	uint32_t value;
	GetShaderLookupValue("Default", "uModelMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, modelMatrix.values);
	GetShaderLookupValue("Default", "uViewMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, viewMatrix.values);
	GetShaderLookupValue("Default", "uProjectionMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, perspectiveMatrix.values);
	GetShaderLookupValue("Default", "uCameraPosition", value);
	glUniform4f(value, -viewMatrix.values[12], -viewMatrix.values[13], -viewMatrix.values[14], 0);
	GetShaderLookupValue("Default", "uAmbientColor", value);
	glUniform4f(value, 0.1f, 0.1f, 0.1f, 0);
	GetShaderLookupValue("Default", "uLightPosition0", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor0", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightPosition1", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor1", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightPosition2", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor2", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightPosition3", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor3", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightsEnable", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uFogMode", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uFogColor", value);
	glUniform4f(value, 0, 0, 0, 1);
	GetShaderLookupValue("Default", "uFogStart", value);
	glUniform1f(value, 0);
	GetShaderLookupValue("Default", "uFogEnd", value);
	glUniform1f(value, 0);
	GetShaderLookupValue("Default", "uFogDensity", value);
	glUniform1f(value, 0);
	GetShaderLookupValue("Default", "uTintColor", value);
	glUniform4f(value, 1, 1, 1, 1);

	return true;
}

bool OpenGLRenderingHelper::LoadTexture(std::wstring path, uint32_t& textureID)
{	
	std::wstring mappedPath;
	if (ConfigLoader::MapPath(path, mappedPath) == 0)
	{
		return false;
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
	m_textureContainerMap.insert(std::pair<int, TextureContainer>(textureContainerID, textureContainer));
	textureID = textureContainerID;
	return true;
}

bool OpenGLRenderingHelper::RenderDynamicBuffer(uint32_t meshID)
{
	MeshUtility::Mesh* mesh = MeshUtility::GetMesh(meshID);
	if (mesh == NULL)
	{
		return false;
	}

	TextureContainer* textureContainer = GetTextureContainer(mesh->textureID);
	if (textureContainer == NULL)
	{
		return false;
	}

	uint32_t aPosition;
	GetShaderLookupValue("Default", "aPosition", aPosition);
	uint32_t aNormal;
	GetShaderLookupValue("Default", "aNormal", aNormal);
	uint32_t aTexCoord;
	GetShaderLookupValue("Default", "aTexCoord", aTexCoord);

	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, textureContainer->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->vertices.size() * sizeof(MeshUtility::Vertex), mesh->vertices.data());

	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glEnableVertexAttribArray(aPosition);
	glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, position));
	glEnableVertexAttribArray(aNormal);
	glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, normal));
	glEnableVertexAttribArray(aTexCoord);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, texcoord));

	uint32_t vertexCount = mesh->vertices.size();
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
	return true;
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

void OpenGLRenderingHelper::CreateShaderLookup(std::string shaderName)
{
    std::map<std::string, uint32_t> lookupMap;
    m_shaderValueMap.insert(std::pair<std::string, std::map<std::string, uint32_t>>(shaderName, lookupMap));
}

bool OpenGLRenderingHelper::AddShaderLookupKeyValue(std::string shaderName, std::string key, uint32_t value)
{
    std::map<std::string, std::map<std::string, uint32_t>>::iterator lookupIt = m_shaderValueMap.find(shaderName);
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
    std::map<std::string, std::map<std::string, uint32_t>>::iterator lookupIt = m_shaderValueMap.find(shaderName);
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