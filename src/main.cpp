#include <mujoco/mujoco.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

// MuJoCo model, simulation data, and drawing objects.
constexpr mjtNum kExperimentDuration = 5.0;
mjModel *m = nullptr;
mjData *d = nullptr;
mjvCamera cam;
mjvOption opt;
mjvScene scn;
mjrContext con;

// Mouse and simulation controls.
bool button_left = false;
bool button_middle = false;
bool button_right = false;
bool paused = true;
bool experiment_finished = false;
double lastx = 0.0;
double lasty = 0.0;
double next_reading_time = 0.0;
int recording_run = 0;

void keyboard(GLFWwindow *window, int key, int scancode, int act, int mods)
{
    if (act != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if (key == GLFW_KEY_SPACE)
    {
        if (experiment_finished == false) {
            paused = !paused;
        }
        std::cout << (paused ? "Paused\n" : "Running\n");
    }
    else if (key == GLFW_KEY_BACKSPACE && m && d)
    {
        mj_resetData(m, d);
        mj_forward(m, d);

        next_reading_time = 0.0;
        ++recording_run;
        experiment_finished = false;

        // Reset preserves whether the simulation is paused or running.
        std::cout << "Reset: time = " << d->time
                  << " s | Run: " << recording_run << '\n';
    }
}

void mouse_button(GLFWwindow *window, int button, int act, int mods)
{
    button_left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    button_middle = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    button_right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    glfwGetCursorPos(window, &lastx, &lasty);
}

void mouse_move(GLFWwindow *window, double xpos, double ypos)
{
    if (!m || (!button_left && !button_middle && !button_right))
        return;

    const double dx = xpos - lastx;
    const double dy = ypos - lasty;
    lastx = xpos;
    lasty = ypos;

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    if (height <= 0)
        return;

    const bool shift =
        glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    mjtMouse action;
    if (button_right)
        action = shift ? mjMOUSE_MOVE_H : mjMOUSE_MOVE_V;
    else if (button_left)
        action = shift ? mjMOUSE_ROTATE_H : mjMOUSE_ROTATE_V;
    else
        action = mjMOUSE_ZOOM;

    mjv_moveCamera(m, action, dx / height, dy / height, &cam);
}

void scroll(GLFWwindow *window, double xoffset, double yoffset)
{
    if (m)
        mjv_moveCamera(m, mjMOUSE_ZOOM, 0, -0.05 * yoffset, &cam);
}

int main(int argc, char **argv)
{
    // 1. Open the window.
    if (!glfwInit())
    {
        std::cerr << "Could not initialize GLFW\n";
        return 1;
    }

    GLFWwindow *window = glfwCreateWindow(
        1200, 900, "MuJoCo OpenGL Simulation", nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Could not create the window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, keyboard);
    glfwSetMouseButtonCallback(window, mouse_button);
    glfwSetCursorPosCallback(window, mouse_move);
    glfwSetScrollCallback(window, scroll);

    // 2. Load the selected XML scene.
    const char *scene_path = (argc > 1) ? argv[1] : "scenes/demo1.xml";
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
    if (!d)
    {
        std::cerr << "Could not create simulation data\n";
        mj_deleteModel(m);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Prepare the initial positions, including for scenes that start paused.
    mj_forward(m, d);
    const int target_id = mj_name2id(m, mjOBJ_BODY, "target");
    const int target_mocap_id =
        target_id >= 0 ? m->body_mocapid[target_id] : -1;

    // 3. Open the recording file when this scene contains a ball.
    std::ofstream motion_log;
    const char *log_path = "results/linear_target.csv";

    if (target_id >= 0)
    {
        std::cout << "Target body ID: " << target_id << '\n'
                  << "Target mocap ID: " << target_mocap_id << '\n'
                  << "Starting time: " << d->time << " s\n"
                  << "Target x: " << d->xpos[3 * target_id] << " m\n"
                  << "Target y: " << d->xpos[3 * target_id + 1] << " m\n"
                  << "Target z: " << d->xpos[3 * target_id + 2] << " m\n";

        std::error_code folder_error;
        std::filesystem::create_directories("results", folder_error);
        if (!folder_error)
            motion_log.open(log_path);

        if (folder_error || !motion_log)
        {
            std::cerr << "Could not create recording file: " << log_path << '\n';
            if (folder_error)
                std::cerr << folder_error.message() << '\n';

            mj_deleteData(d);
            mj_deleteModel(m);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }

        motion_log.precision(10);
        motion_log << "run,time_s,x_m,y_m,z_m\n";
        std::cout << "Recording to " << log_path
                  << " (replaced each program launch)\n";
    }
    else
    {
        // Other sample scenes still work without creating or replacing a log.
        std::cout << "No body named target: viewing scene without target recording.\n";
    }

    std::cout << "Space: pause/run | Backspace: reset | Escape: close\n";

    // 4. Prepare drawing.
    mjv_defaultCamera(&cam);
    mjv_defaultOption(&opt);
    mjv_defaultScene(&scn);
    mjr_defaultContext(&con);
    mjv_makeScene(m, &scn, 2000);
    mjr_makeContext(m, &con, mjFONTSCALE_150);

    // 5. Run, take readings, and draw.
    int exit_status = 0;
    while (!glfwWindowShouldClose(window))
    {
        if (!paused)
        {
            // Advance roughly 1/60 simulation second per displayed frame.
            const mjtNum simstart = d->time;
            while (d->time - simstart < 1.0 / 60.0)
            {
                if (target_id >= 0 && target_mocap_id >= 0)
                {
                    d->mocap_pos[3 * target_mocap_id] =
                        -0.10 + 0.26 * d->time;
                }

                if (target_id >= 0 && d->time + 1e-9 >= next_reading_time)
                {
                    // Read the position and time from the same simulation state.
                    mj_forward(m, d);
                    const mjtNum x = d->xpos[3 * target_id];
                    const mjtNum y = d->xpos[3 * target_id + 1];
                    const mjtNum z = d->xpos[3 * target_id + 2];

                    std::cout << "Run: " << recording_run
                              << " | Time: " << d->time
                              << " s | Height: " << z << " m\n";

                    motion_log << recording_run << ',' << d->time << ','
                               << x << ',' << y << ',' << z << '\n';

                    if (!motion_log)
                    {
                        std::cerr << "Could not write movement data\n";
                        exit_status = 1;
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                        break;
                    }

                    next_reading_time += 0.1;
                }

                if (d->time >= kExperimentDuration)
                {
                    experiment_finished = true;
                    paused = true;
                    std::cout << "Experiment finished after "
                              << kExperimentDuration << " seconds\n";
                    break;
                }

                mj_step(m, d);
            }
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        char title[150];
        std::snprintf(title, sizeof(title),
                      "MuJoCo | %s | Run: %d | Time: %.2f seconds",
                      paused ? "Paused" : "Running", recording_run,
                      static_cast<double>(d->time));
        glfwSetWindowTitle(window, title);

        if (width > 0 && height > 0)
        {
            const mjrRect viewport = {0, 0, width, height};
            mjv_updateScene(m, d, &opt, nullptr, &cam, mjCAT_ALL, &scn);
            mjr_render(viewport, &scn, &con);
            glfwSwapBuffers(window);
        }

        glfwPollEvents();
    }

    // 6. Finish saving before closing.
    if (motion_log.is_open())
    {
        motion_log.close();
        if (motion_log.fail() || exit_status != 0)
        {
            std::cerr << "Movement recording could not be completed\n";
            exit_status = 1;
        }
        else
        {
            std::cout << "Saved " << log_path << '\n';
        }
    }

    // Free drawing resources while the graphics window still exists.
    mjr_freeContext(&con);
    mjv_freeScene(&scn);
    mj_deleteData(d);
    mj_deleteModel(m);
    glfwDestroyWindow(window);
    glfwTerminate();

    return exit_status;
}
