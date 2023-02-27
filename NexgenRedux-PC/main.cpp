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

#include "BootLoader.h"
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
static float frand() { return (float)rand() / RAND_MAX; }
static float frand(float lo, float hi) { return lerp(lo, hi, frand()); }


void window_iconify(GLFWwindow* window, int iconified)
{
    printf("window iconify: %i\n", iconified);
}

void window_maximize(GLFWwindow* window, int maximized)
{
    printf("window maximize: %i\n", maximized);
}

void window_size(GLFWwindow* window, int width, int height)
{
    printf("window size: %i,%i\n", width, height);
}

void window_focus(GLFWwindow* window, int focused)
{
    printf("window focus: %i\n", focused);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    printf("keyboard state: %i,%i,%i,%i\n", key, scancode, action, mods);
}

void character_callback(GLFWwindow* window, unsigned int codepoint)
{
    printf("keyboard key: %i\n", codepoint);
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    printf("mouse moved: %f,%f\n", xpos, ypos);
}

void cursor_enter_callback(GLFWwindow* window, int entered)
{
    if (entered)
    {
        printf("mouse entered\n");
    }
    else
    {
        printf("mouse left\n");
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    printf("mouse button %i,%i,%i\n", button, action, mods);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    printf("mouse scroll %f,%f\n", xoffset, yoffset);
}

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    for (int i = 0; i < count; i++) {
        printf("drop %i,%s\n", count, paths[i]);
    }
}

void joystick_callback(int jid, int event)
{
    if (event == GLFW_CONNECTED)
    {
        printf("joystick connected %i,%i\n", jid, event);
    }
    else if (event == GLFW_DISCONNECTED)
    {
         printf("joystick disconnected %i,%i\n", jid, event);
    }
}

int main(int argc, const char* argv[])
{
	//Gensys::Test::RunTests();

	BootLoader::Run();

    AngelScriptRunner::Init();
    AngelScriptRunner::ExecuteCalc();
    AngelScriptRunner::Close();

    uint32_t monitorCount;
    if (WindowManager::GetAvailableMonitorCount(monitorCount) == false)
    {
        return 0;
    }

    std::vector<WindowManager::MonitorVideoMode> videoModes;
    if (WindowManager::GetMonitorVideoModes(0, videoModes) == false)
    {
        return 0;
    }

    WindowManager::MonitorVideoMode videoMode;
    if (WindowManager::GetMonitorVideoMode(0, videoMode) == false)
    {
        return 0;
    }

    uint32_t windowHandle1;
    if (WindowManager::WindowCreate(640, 480, "EqUiNoX was here...", windowHandle1) == false)
    {
        return 0;
    }

    // uint32_t windowHandle2;
    // if (WindowManager::WindowCreate(videoMode, "EqUiNoX was here again...", windowHandle2) == false)
    // {
    //     return 0;
    // }

    if (WindowManager::RenderLoop() == false)
    {
        return 0;
    }

    if (WindowManager::WindowClose(windowHandle1) == false)
    {
        return 0;
    }

    // if (WindowManager::WindowClose(windowHandle2) == false)
    // {
    //     return 0;
    // }

    return 0;
}