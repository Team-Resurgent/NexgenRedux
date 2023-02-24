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


//using namespace NexgenRedux;
using namespace Gensys;
//using namespace AngelScript;

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

static int W = 640;
static int H = 480;

static float lerp(float a, float b, float t) { return a + (b - a) * t; }
static float frand() { return (float)rand() / RAND_MAX; }
static float frand(float lo, float hi) { return lerp(lo, hi, frand()); }

double now()
{
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(now).time_since_epoch().count() *
           1e-9;
}

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

void drop_callback(GLFWwindow* window, int count, const char** paths)
{
    for (int i = 0; i < count; i++) {
        printf("drop %i,%s\n", count, paths[i]);
    }
}

int main(int argc, const char* argv[])
{
	Gensys::Test::RunTests();

    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize glfw.\n");
        return 1;
    }

    glfwWindowHint(GLFW_SAMPLES, 0);
    
    GLFWwindow* window = glfwCreateWindow(W, H, "Nexgen Redux", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        fprintf(stderr, "Failed to create window.\n");
        return -1;
    }

    GLFWimage iconImages[1]; 
    iconImages[0].pixels = iconData;
    iconImages[0].width = iconWidth;
    iconImages[0].height = iconHeight;
    glfwSetWindowIcon(window, 1, iconImages); 

    glfwMakeContextCurrent(window);
    glfwSetWindowIconifyCallback(window, window_iconify);
    glfwSetWindowMaximizeCallback(window, window_maximize);
    glfwSetWindowSizeCallback(window, window_size);
    glfwSetWindowFocusCallback(window, window_focus);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, character_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetJoystickCallback(joystick_callback);
    glfwSetDropCallback(window, drop_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    if (!gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize glad.\n");
        return -1;
    }

    printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION: %s\n", glGetString(GL_VERSION));

    int totalFrames = 0;
    int frames = 0;
    double start = now();
    int lastWidth = 0, lastHeight = 0;

    while (!glfwWindowShouldClose(window))
    {
        glClearColor(1.0f, .1f, .1f, .1f);
        glClear(GL_COLOR_BUFFER_BIT);
        // int width, height;
        // glfwGetFramebufferSize(window, &width, &height);
        // if (lastWidth != width || lastHeight != height)
        // {
        //     printf("rendering %i bubbles at %i x %i\n", n, width, height);
        //     glViewport(0, 0, width, height);
        //     glUniform2f(uniformWindow, static_cast<float>(width), static_cast<float>(height));

        //     glDeleteTextures(1, &tex);
        //     glGenTextures(1, &tex);
        //     glBindTexture(GL_TEXTURE_2D, tex);
        //     glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

        //     glBindFramebuffer(GL_FRAMEBUFFER, blitFBO);
        //     glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        //     glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
        //     glDrawBuffers(0, nullptr);
        //     glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, width);
        //     glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, height);
        //     glBindImageTexture(0, tex, 0, 0, 0, GL_WRITE_ONLY, GL_R32UI);

        //     lastWidth = width;
        //     lastHeight = height;
        // }

        // glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
        // glUniform1f(uniformT, static_cast<float>(totalFrames++));
        // glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, n);

        // glBindFramebuffer(GL_READ_FRAMEBUFFER, blitFBO);
        // glBlitFramebuffer(0,
        //                   0,
        //                   width,
        //                   height,
        //                   0,
        //                   0,
        //                   width,
        //                   height,
        //                   GL_COLOR_BUFFER_BIT,
        //                   GL_NEAREST);

        glfwSwapBuffers(window);

        ++frames;
        double end = now();
        double seconds = end - start;
        if (seconds >= 2)
        {
            printf("%f fps\n", frames / seconds);
            fflush(stdout);
            frames = 0;
            start = end;
        }

        glfwPollEvents();
    }

    glfwTerminate();
}