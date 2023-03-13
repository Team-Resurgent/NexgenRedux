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
    const std::string vertexShader =

		"precision mediump float;\n"

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

	const std::string fragmentShader =

		"precision mediump float;\n"
	
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
            std::string errorMessage = std::string("Shader compilation failed: ");
            errorMessage += std::string(infoLog.begin(), infoLog.end());
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Shader compiled failed: %s", errorMessage.c_str());
            return 0;
        }
        return shader;
    }

    GLuint CompileProgram(const std::string& vsSource, const std::string& fsSource)
    {
        GLuint program = glCreateProgram();
        if (program == 0)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Create program failed: %i", glGetError());
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
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Program link failed '%s'.", errorMessage.c_str());
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
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Shader '%s' not recognized.", shaderName.c_str());
		return;
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
	GetShaderLookupValue("Default", "uAmbientColor", value);
	glUniform4f(value, 0.1f, 0.1f, 0.1f, 0);
	GetShaderLookupValue("Default", "uLightEnable0", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uLightPosition0", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor0", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightEnable1", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uLightPosition1", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor1", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightEnable2", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uLightPosition2", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor2", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightEnable3", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uLightPosition3", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor3", value);
	glUniform4f(value, 0, 0, 0, 0);
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
}

void OpenGLRenderingHelper::SetModelMatrix(const MathUtility::Matrix4x4& matrix) 
{
	uint32_t value;
	GetShaderLookupValue("Default", "uModelMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, matrix.values);
}

void OpenGLRenderingHelper::SetViewMatrix(const MathUtility::Matrix4x4& matrix) 
{
	uint32_t value;
	GetShaderLookupValue("Default", "uViewMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, matrix.values);
}

void OpenGLRenderingHelper::SetProjectionMatrix(const MathUtility::Matrix4x4& matrix) 
{
	uint32_t value;
	GetShaderLookupValue("Default", "uProjectionMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, matrix.values);
}

void OpenGLRenderingHelper::SetAmbientLight(const MathUtility::Color3I& color) 
{
	uint32_t value;
	GetShaderLookupValue("Default", "uAmbientColor", value);
	glUniform4f(value, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 0);
}

void OpenGLRenderingHelper::SetTexture(const uint32_t& textureID, const RenderStateManager::TextureFilter& filter) 
{
	TextureContainer* textureContainer = GetTextureContainer(textureID);
	if (textureContainer == NULL)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Texture with ID '%s' not found.", textureID);
		return;
	}

	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, textureContainer->texture);
	if (filter == RenderStateManager::TextureFilterLinear)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else if (filter == RenderStateManager::TextureFilterNearest)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}

void OpenGLRenderingHelper::SetTint(const MathUtility::Color4I& color) 
{
	uint32_t value;
	GetShaderLookupValue("Default", "uTintColor", value);
	glUniform4f(value, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
}

void OpenGLRenderingHelper::SetBlend(const RenderStateManager::BlendOperation& operation) 
{
	if (operation == RenderStateManager::BlendOperationDisabled)
	{
		glDisable(GL_BLEND);
	}
	else if (operation == RenderStateManager::BlendOperationAdd)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
	}
	else if (operation == RenderStateManager::BlendOperationSubtract)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_SUBTRACT);
	}
	else if (operation == RenderStateManager::BlendOperationReverseSubtract)
	{
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	}
}

void OpenGLRenderingHelper::SetBlendFactors(const RenderStateManager::BlendFactor& srcFactor, const RenderStateManager::BlendFactor& dstFactor) 
{
	uint32_t srcFactorValue = RenderStateManager::BlendFactorZero;
	if (srcFactor == RenderStateManager::BlendFactorZero) 
	{
		srcFactorValue = GL_ZERO;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOne) 
	{
		srcFactorValue = GL_ONE;
	}
	else if (srcFactor == RenderStateManager::BlendFactorSrcColor) 
	{
		srcFactorValue = GL_SRC_COLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusSrcColor) 
	{
		srcFactorValue = GL_ONE_MINUS_SRC_COLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorDstColor) 
	{
		srcFactorValue = GL_DST_COLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusDstColor) 
	{
		srcFactorValue = GL_ONE_MINUS_DST_COLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorSrcAlpha) 
	{
		srcFactorValue = GL_SRC_ALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusSrcAlpha) 
	{
		srcFactorValue = GL_ONE_MINUS_SRC_ALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorDstAlpha) 
	{
		srcFactorValue = GL_DST_ALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusDstAlpha) 
	{
		srcFactorValue = GL_ONE_MINUS_DST_ALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorConstantColor) 
	{
		srcFactorValue = GL_CONSTANT_COLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusConstantColor) 
	{
		srcFactorValue = GL_ONE_MINUS_CONSTANT_COLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorConstantAlpha) 
	{
		srcFactorValue = GL_CONSTANT_ALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusConstantAlpha) 
	{
		srcFactorValue = GL_ONE_MINUS_CONSTANT_ALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorSrcAlphaSaturate) 
	{
		srcFactorValue = GL_SRC_ALPHA_SATURATE;
	}

	uint32_t dstFactorValue = RenderStateManager::BlendFactorZero;
	if (dstFactor == RenderStateManager::BlendFactorZero) 
	{
		dstFactorValue = GL_ZERO;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOne) 
	{
		dstFactorValue = GL_ONE;
	}
	else if (dstFactor == RenderStateManager::BlendFactorSrcColor) 
	{
		dstFactorValue = GL_SRC_COLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusSrcColor) 
	{
		dstFactorValue = GL_ONE_MINUS_SRC_COLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorDstColor) 
	{
		dstFactorValue = GL_DST_COLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusDstColor) 
	{
		dstFactorValue = GL_ONE_MINUS_DST_COLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorSrcAlpha) 
	{
		dstFactorValue = GL_SRC_ALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusSrcAlpha) 
	{
		dstFactorValue = GL_ONE_MINUS_SRC_ALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorDstAlpha) 
	{
		dstFactorValue = GL_DST_ALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusDstAlpha) 
	{
		dstFactorValue = GL_ONE_MINUS_DST_ALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorConstantColor) 
	{
		dstFactorValue = GL_CONSTANT_COLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusConstantColor) 
	{
		dstFactorValue = GL_ONE_MINUS_CONSTANT_COLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorConstantAlpha) 
	{
		dstFactorValue = GL_CONSTANT_ALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusConstantAlpha) 
	{
		dstFactorValue = GL_ONE_MINUS_CONSTANT_ALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorSrcAlphaSaturate) 
	{
		dstFactorValue = GL_SRC_ALPHA_SATURATE;
	}

	glBlendFunc(srcFactorValue, dstFactorValue);
}

void OpenGLRenderingHelper::SetCulling(const RenderStateManager::CullingOperation& operation) 
{
	if (operation == RenderStateManager::CullingOperationDisabled) 
	{
		glDisable(GL_CULL_FACE);
	}
	else if (operation == RenderStateManager::CullingOperationFront) 
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
	else if (operation == RenderStateManager::CullingOperationBack) 
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
}

void OpenGLRenderingHelper::SetDepth(const RenderStateManager::DepthOperation& operation) 
{
	if (operation == RenderStateManager::DepthOperationDisabled) 
	{
		glDisable(GL_DEPTH_TEST);
	}
	else if (operation == RenderStateManager::DepthOperationNever) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_NEVER);
	}
	else if (operation == RenderStateManager::DepthOperationLess) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}
	else if (operation == RenderStateManager::DepthOperationEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_EQUAL);
	}
	else if (operation == RenderStateManager::DepthOperationGreater) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
	}
	else if (operation == RenderStateManager::DepthOperationLessOrEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_GREATER);
	}
	else if (operation == RenderStateManager::DepthOperationNotEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_NOTEQUAL);
	}
	else if (operation == RenderStateManager::DepthOperationGreaterOrEqual) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_GEQUAL);
	}
	else if (operation == RenderStateManager::DepthOperationAlways) 
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
	}
}

void OpenGLRenderingHelper::SetLights(const RenderStateManager::LightsOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightsEnable", value);
	if (operation == RenderStateManager::LightsOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == RenderStateManager::LightsOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLight1(const RenderStateManager::LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightEnable0", value);
	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightPosition0", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightDiffuseColor0", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);

}

void OpenGLRenderingHelper::SetLight2(const RenderStateManager::LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightEnable1", value);
	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightPosition1", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightDiffuseColor1", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);
}

void OpenGLRenderingHelper::SetLight3(const RenderStateManager::LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightEnable2", value);
	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightPosition2", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightDiffuseColor2", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);
}

void OpenGLRenderingHelper::SetLight4(const RenderStateManager::LightOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightEnable3", value);
	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		glUniform1i(value, 1);
	}
}

void OpenGLRenderingHelper::SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightPosition3", value);
	glUniform4f(value, position.values[0], position.values[1], position.values[2], distance);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uLightDiffuseColor3", value);
	glUniform4f(value, diffuse.values[0], diffuse.values[1], diffuse.values[2], 0);
}

void OpenGLRenderingHelper::SetFog(const RenderStateManager::FogOperation& operation) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uFogMode", value);
	if (operation == RenderStateManager::FogOperationDisabled) 
	{
		glUniform1i(value, 0);
	}
	else if (operation == RenderStateManager::FogOperationExponent) 
	{
		glUniform1i(value, 1);
	}
	else if (operation == RenderStateManager::FogOperationExponent2) 
	{
		glUniform1i(value, 2);
	}
	else if (operation == RenderStateManager::FogOperationLinear) 
	{
		glUniform1i(value, 3);
	}
}

void OpenGLRenderingHelper::SetFogInstance(const MathUtility::Color3I& color, const float& start, const float& end, const float& density) 
{
	uint32_t value;
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uFogColor", value);
	glUniform4f(value, color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 0);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uFogStart", value);
	glUniform1f(value, start);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uFogEnd", value);
	glUniform1f(value, end);
	GetShaderLookupValue(RenderStateManager::GetRenderState()->shaderState.shader, "uFogDensity", value);
	glUniform1f(value, density);
}

void OpenGLRenderingHelper::SetViewport(const MathUtility::RectI rect) 
{
	glViewport(rect.x, rect.y, rect.width, rect.height);
}

void OpenGLRenderingHelper::SetScissor(const RenderStateManager::ScissorOperation& operation, const MathUtility::RectI& rect) 
{
	if (operation == RenderStateManager::ScissorOperationDisabled) 
	{
		glDisable(GL_SCISSOR_TEST);
	}
	else if (operation == RenderStateManager::ScissorOperationEnabled) 
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

bool OpenGLRenderingHelper::RenderMesh(uint32_t meshID)
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

	uint32_t requestedSize =  mesh->vertices.size() * sizeof(MeshUtility::Vertex);
	ResizeDynamicBufferIfNeeded(requestedSize);

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
	if (RenderStateManager::GetRenderState()->drawModeState.operation == RenderStateManager::DrawModeTriangles) 
	{
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
	}
	else if (RenderStateManager::GetRenderState()->drawModeState.operation == RenderStateManager::DrawModeLines) 
	{
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
	}
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