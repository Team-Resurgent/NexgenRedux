#include "glfw3.h"
//#include "GL/gl.h"
#include "angle_gl.h"


int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_OPENGLES);

    if (!glfwInit())
        return -1;

// glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
// glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
// glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
// glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
// glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
// glfwWindowHint(GLFW_SAMPLES, 0);
// glfwWindowHint(GLFW_RED_BITS, 8);
// glfwWindowHint(GLFW_GREEN_BITS, 8);
// glfwWindowHint(GLFW_BLUE_BITS, 8);
 //glfwWindowHint(GLFW_ALPHA_BITS, 0);

    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Nexgen Redux", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    glClearColor( 0.8f, 0.5f, 0.4f, 0.0f );

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
        auto a = glGetError();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}