#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ge1/vertex_array.h"
#include "ge1/program.h"

using namespace std;
using namespace glm;

struct unique_glfw {
    unique_glfw() {
        if (!glfwInit()) {
            throw runtime_error("Couldn't initialize GLFW!");
        }
    }

    ~unique_glfw() {
        glfwTerminate();
    }
};

struct context;
struct operation;

struct context {
    context() = default;

    operation* current_operation;
    glm::mat3 view_matrix;
    unsigned int width, height;
};

struct operation {
    operation() = default;
    virtual ~operation() = default;

    virtual void trigger(context &c, double x, double y) = 0;
    virtual void mouse_move_event(context &c, double x, double y) {};
    virtual void mouse_button_event(
        context &c, int button, int action, int modifiers
    ) {};
    virtual void key_event(
        context &c, int key, int scancode, int modifiers
    ) {};
};

struct pan_operation : public operation {
    void trigger(context &c, double x, double y) override {
        c.current_operation = this;
        offset = {x, y};
    }
    void mouse_move_event(context &c, double x, double y) override {
        glm::vec2 position = glm::vec2{x, y};
        auto delta = position - offset;

        c.view_matrix[2].x += 2 * delta.x / c.width;
        c.view_matrix[2].y -= 2 * delta.y / c.height;

        offset = position;
    }
    void mouse_button_event(
        context &c, int button, int action, int modifiers
    ) override {
        if (action == GLFW_RELEASE) {
            c.current_operation = nullptr;
        }
    }

    glm::vec2 offset;
};

static unique_glfw* glfw;
static context* current_context;

static pan_operation pan_operation;

void mouse_button_callback(
    GLFWwindow* window, int button, int action, int modifiers
) {
    if (current_context->current_operation) {
        current_context->current_operation->mouse_button_event(
            *current_context, button, action, modifiers
        );

    } else if (action == GLFW_PRESS) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (modifiers & GLFW_MOD_SHIFT) {

        } else if (modifiers & GLFW_MOD_CONTROL) {

        } else if (modifiers & GLFW_MOD_ALT) {
            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                // current_operation = &rotate_view_operation;
            } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
                current_context->current_operation = &pan_operation;
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                // current_operation = &dolly_view_operation;
            }
        }

        if (current_context->current_operation) {
            current_context->current_operation->trigger(*current_context, x, y);
        }
    }
}

void cursor_position_callback(GLFWwindow*, double x, double y) {
    if (current_context->current_operation) {
        current_context->current_operation->mouse_move_event(
            *current_context, x, y
        );
    }
}

void window_size_callback(GLFWwindow*, int width, int height) {
    current_context->width = static_cast<unsigned int>(width);
    current_context->height = static_cast<unsigned int>(height);

    glViewport(0, 0, width, height);
}

int main() {
    unique_glfw glfw;
    ::glfw = &glfw;

    GLFWwindow* window;

    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_MAXIMIZED , GL_TRUE);
    glfwSwapInterval(1);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    int screen_width = mode->width, screen_height = mode->height;

    window = glfwCreateWindow(
        screen_width, screen_height, "demo", nullptr, nullptr
    );
    if (!window) {
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        return -1;
    }

    enum : GLuint {
        position,
    };

    GLuint position_buffer, line_buffer;

    unsigned position_capacity = 1024;
    unsigned line_capacity = 1024;

    ge1::unique_vertex_array line_array = ge1::create_vertex_array(
        position_capacity,
        {{
            {{position, 2, GL_FLOAT, GL_FALSE, 0}},
            0, GL_DYNAMIC_DRAW, &position_buffer
        }},
        line_capacity, &line_buffer, GL_UNSIGNED_SHORT
    );

    glBindBuffer(GL_COPY_WRITE_BUFFER, position_buffer);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, position_capacity * sizeof(glm::vec2), nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    auto positions = reinterpret_cast<glm::vec2*>(glMapBufferRange(
        GL_COPY_WRITE_BUFFER, 0, position_capacity * sizeof(glm::vec2),
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    ));

    positions[0] = {0, 0};
    positions[1] = {0.1, 0.1};
    positions[2] = {0.2, 0.1};

    glBindBuffer(GL_COPY_WRITE_BUFFER, line_buffer);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, line_capacity * sizeof(glm::vec2), nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    auto lines = reinterpret_cast<unsigned short*>(glMapBufferRange(
        GL_COPY_WRITE_BUFFER, 0, line_capacity * sizeof(unsigned short),
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    ));

    positions[0] = {0, 0};
    positions[1] = {0.1, 0.1};
    positions[2] = {0.2, 0.1};

    lines[0] = 0;
    lines[1] = 1;
    lines[2] = 1;
    lines[3] = 2;

    enum : GLuint {
        position_location,
    };

    ge1::unique_program program = ge1::compile_program(
        "shader/position_vertex.glsl", nullptr, nullptr, nullptr, nullptr, {},
        {{"position", position_location}}
    );

    GLuint view_matrix_location;

    ge1::get_uniform_locations(
        program.get_name(), {{"view_matrix", &view_matrix_location}}
    );

    context c;
    current_context = &c;
    c.current_operation = nullptr;
    c.view_matrix = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };

    glfwSetCursorPosCallback(window, &cursor_position_callback);
    glfwSetMouseButtonCallback(window, &mouse_button_callback);
    glfwSetWindowSizeCallback(window, &window_size_callback);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    window_size_callback(window, width, height);

    while (!glfwWindowShouldClose(window)) {
        glClearColor(255, 255, 255, 255);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program.get_name());
        glUniformMatrix3fv(
            view_matrix_location, 1, GL_FALSE, glm::value_ptr(c.view_matrix)
        );
        glBindVertexArray(line_array.get_name());
        glDrawElements(GL_LINES, 4, GL_UNSIGNED_SHORT, 0);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    return 0;
}
