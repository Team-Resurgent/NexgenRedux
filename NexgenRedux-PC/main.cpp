#include "GLAD/glad.h"
#include "GLFW/glfw3.h"

#include "Icon.h"
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include "ConfigLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"

using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

const char *vs = 
"(#version 310 es" 
"precision highp float;" 
"uniform vec2 window;" 
"uniform float T;"
"layout(location=0) in vec3 bubble;"
"layout(location=1) in vec2 speed;"
"layout(location=2) in vec4 incolor;"
"out vec2 coord;"
"out vec4 color;"
"void main() {"
"    vec2 offset = vec2((gl_VertexID & 1) == 0 ? -1.0 : 1.0, (gl_VertexID & 2) == 0 ? -1.0 : 1.0);"
"    coord = offset;"
"    color = incolor;"
"    float r = bubble.z;"
"    vec2 center = bubble.xy + speed * T;"
"    vec2 span = window - 2.0 * r;"
"    center = span - abs(span - mod(center - r, span * 2.0)) + r;"
"    gl_Position.xy = (center + offset * r) * 2.0 / window - 1.0;"
"    gl_Position.zw = vec2(0, 1);"
"})";

const char *fs = 
"(#version 310 es"
"precision mediump float;"
"in vec2 coord;"
"in vec4 color;"
"layout(binding=0, r32ui) uniform highp coherent writeonly uimage2D framebuffer;"
"void main() {"
"    ivec2 pixelCoord = ivec2(floor(gl_FragCoord.xy));"
"    float f = coord.x * coord.x + coord.y * coord.y - 1.0;"
"    float coverage = clamp(.5 - f/fwidth(f), 0.0, 1.0);"
"    vec4 s = vec4(color.rgb, 1) * (color.a * mix(.25, 1.0, dot(coord, coord)) * coverage);"
"    imageStore(framebuffer, pixelCoord, uvec4(packUnorm4x8(s)));"
"})";

static bool compile_and_attach_shader(GLuint program, GLuint type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to compile shader\n");
        int l = 1;
        std::stringstream stream(source);
        std::string lineStr;
        while (std::getline(stream, lineStr, '\n'))
        {
            fprintf(stderr, "%4i| %s\n", l++, lineStr.c_str());
        }
        fprintf(stderr, "%s\n", &infoLog[0]);
        fflush(stderr);
        glDeleteShader(shader);
        return false;
    }
    glAttachShader(program, shader);
    glDeleteShader(shader);
    return true;
}

static bool link_program(GLuint program)
{
    glLinkProgram(program);
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
        fprintf(stderr, "Failed to link program %s\n", &infoLog[0]);
        fflush(stderr);
        return false;
    }
    return true;
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

int main(int argc, const char* argv[])
{
	//Gensys::Test::RunTests();

	ConfigLoader::ConfigData configData;
	memset(&configData, 0, sizeof(configData));
	if (ConfigLoader::LoadConfig(configData) == false) 
	{
		return 0;
	}

    if (AngelScriptRunner::Init(configData.LaunchFolder) == false)
    {
        return 0;
    }
    if (AngelScriptRunner::ExecuteInit() == false)
    {
        return 0;
    }

    if (WindowManager::RenderLoop() == false)
    {
        return 0;
    }

    WindowManager::Close();
    AngelScriptRunner::Close();

    return 0;
}