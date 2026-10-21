#include <mujoco/mujoco.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <iostream>

// MuJoCo visualization structures
mjModel *m = nullptr;
mjData *d = nullptr;
mjvCamera cam;  // abstract camera
mjvOption opt;  // visualization options
mjvScene scn;   // abstract scene
mjrContext con; // custom GPU context

// Mouse interaction state
bool button_left = false;
bool button_middle = false;
bool button_right = false;
double lastx = 0;
double lasty = 0;

// Keyboard callback for closing the window
void keyboard(GLFWwindow *window, int key, int scancode, int act, int mods)
{
    if (act == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

// Mouse button callback for camera manipulation
void mouse_button(GLFWwindow *window, int button, int act, int mods)
{
    button_left = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    button_middle = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    button_right = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    glfwGetCursorPos(window, &lastx, &lasty);
}

// Mouse move callback for camera rotation/translation
void mouse_move(GLFWwindow *window, double xpos, double ypos)
{
    if (!button_left && !button_middle && !button_right)
        return;

    double dx = xpos - lastx;
    double dy = ypos - lasty;
    lastx = xpos;
    lasty = ypos;

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Get current keyboard modifiers from GLFW to check for SHIFT key
    int mods = 0;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        mods |= GLFW_MOD_SHIFT;
    }

    mjtMouse action;
    if (button_right)
    {
        action = (mods & GLFW_MOD_SHIFT) ? mjMOUSE_MOVE_H : mjMOUSE_MOVE_V;
    }
    else if (button_left)
    {
        action = (mods & GLFW_MOD_SHIFT) ? mjMOUSE_ROTATE_H : mjMOUSE_ROTATE_V;
    }
    else
    {
        action = mjMOUSE_ZOOM;
    }

    // Correct API signature: camera pointer comes first, no scene argument needed
    mjv_moveCamera(m, action, dx / height, dy / height, &cam);
}
// Scroll callback for zooming
void scroll(GLFWwindow *window, double xoffset, double yoffset)
{   // Correct API signature
    mjv_moveCamera(m, mjMOUSE_ZOOM, 0, -0.05 * yoffset, &cam);
}

int main(int argc, char **argv)
{
    // 1. Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Could not initialize GLFW" << std::endl;
        return 1;
    }

    // Create a window (60fps vsync by default)
    GLFWwindow *window = glfwCreateWindow(1200, 900, "MuJoCo OpenGL Simulation", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Set up input callbacks
    glfwSetKeyCallback(window, keyboard);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetCursorPosCallback(window, mouse_move);
    glfwSetScrollCallback(window, scroll);

    // 2. Load MuJoCo Model (Make sure to place a 'scene.xml' in your directory)
    
    const char *scene_path =
        (argc > 1) ? argv[1] : "scenes/demo1.xml";

    char error[1000] = {};
    m = mj_loadXML(scene_path, nullptr, error, sizeof(error));

    if (!m)
    {
        std::cerr << "Could not load scene: " << scene_path
                  << "\nMuJoCo error: " << error << '\n';

        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    d = mj_makeData(m);

    // 3. Initialize MuJoCo visualization objects
    mjv_defaultCamera(&cam);
    mjv_defaultOption(&opt);
    mjv_defaultScene(&scn);
    mjr_defaultContext(&con);

    mjv_makeScene(m, &scn, 2000);              // Make scene with maximum geometric limit
    mjr_makeContext(m, &con, mjFONTSCALE_150); // Create GPU context

    // 4. Main Simulation Loop
    while (!glfwWindowShouldClose(window))
    {
        // Advance physics simulation based on wall clock time vs sim time
        mjtNum simstart = d->time;
        while (d->time - simstart < 1.0 / 60.0)
        {
            mj_step(m, d);
        }

        // Get framebuffer size for rendering viewport
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        mjrRect viewport = {0, 0, width, height};

        // Update abstract scene and render
        mjv_updateScene(m, d, &opt, nullptr, &cam, mjCAT_ALL, &scn);
        mjr_render(viewport, &scn, &con);

        // Swap buffers and poll OS events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 5. Clean up MuJoCo and GLFW
    mj_deleteData(d);
    mj_deleteModel(m);
    mjr_freeContext(&con);
    mjv_freeScene(&scn);

    glfwTerminate();
    return 0;
}
