#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ge1/vertex_array.h"
#include "ge1/program.h"

struct unique_glfw {
    unique_glfw() {
        if (!glfwInit()) {
            throw std::runtime_error("couldn't initialize GLFW");
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
    ge1::span<glm::vec2> positions;

    glm::vec2 view_center, view_right;
    glm::mat3x2 view_matrix;

    unsigned width, height;
};

glm::vec2 to_canvas(context &c, glm::vec2 screen_position) {
    auto ndc = 2.f * screen_position / glm::vec2(c.width, c.height) - 1.f;
    ndc.y = -ndc.y;
    return glm::inverse(glm::mat2(c.view_matrix)) * (ndc - c.view_center);
}

struct operation {
    operation() = default;
    virtual ~operation() = default;

    virtual void trigger(context &c, double x, double y) = 0;
    virtual void mouse_move_event(context &c, double x, double y) = 0;
    virtual void mouse_button_event(
        context &c, int button, int action, int modifiers
    ) = 0;
    virtual void key_event(
        context &c, int key, int scancode, int modifiers
    ) = 0;
};

struct pan_operation : public operation {
    void trigger(context &c, double x, double y) override {
        c.current_operation = this;
        offset = {x, y};
    }
    void mouse_move_event(context &c, double x, double y) override {
        glm::vec2 position = glm::vec2{x, y};
        auto delta = position - offset;

        c.view_center.x += 2 * delta.x / c.width;
        c.view_center.y -= 2 * delta.y / c.height;
        c.view_matrix[2] = c.view_center;

        offset = position;
    }
    void mouse_button_event(context &c, int, int action, int) override {
        if (action == GLFW_RELEASE) {
            c.current_operation = nullptr;
        }
    }
    void key_event(context &, int, int, int) override {}

    glm::vec2 offset;
};

struct drag_operation : public operation {
    void trigger(context &c, double x, double y) override {
        old_position = to_canvas(c, {x, y});

        float closest_distance = 1.0f;

        for (glm::vec2 &point : c.positions) {
            auto offset = point - old_position;
            auto distance = dot(offset, offset);
            if (distance < closest_distance * closest_distance) {
                c.current_operation = this;
                closest_distance = distance;
                index = &point - c.positions.begin();
            }
        }
    }
    void mouse_move_event(context &c, double x, double y) override {
        glm::vec2 position = to_canvas(c, {x, y});
        auto delta = position - old_position;

        c.positions[index] += delta;

        old_position = position;
    }
    void mouse_button_event(
        context &c, int, int action, int
    ) override {
        if (action == GLFW_RELEASE) {
            c.current_operation = nullptr;
        }
    }
    void key_event(context &, int, int, int) override {}

    glm::vec2 old_position;
    unsigned index;
};

static context* current_context;

static pan_operation pan_operation;
static drag_operation drag_operation;

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
                pan_operation.trigger(*current_context, x, y);
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                // current_operation = &dolly_view_operation;
            }
        } else {
            drag_operation.trigger(*current_context, x, y);
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
    auto &c = *current_context;

    c.width = static_cast<unsigned int>(width);
    c.height = static_cast<unsigned int>(height);

    float aspect_ratio = static_cast<float>(c.width) / c.height;
    c.view_matrix = {
        c.view_right.x, c.view_right.y * aspect_ratio,
        -c.view_right.y, c.view_right.x * aspect_ratio,
        c.view_center.x, c.view_center.y,
    };

    glViewport(0, 0, width, height);
}

glm::vec2 square_positions[]{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
unsigned short square_triangles[]{0, 1, 2, 2, 1, 3};

int main() {
    unique_glfw glfw;

    GLFWwindow* window;

    glfwWindowHint(GLFW_SAMPLES, 8);
    glfwWindowHint(GLFW_MAXIMIZED , GL_TRUE);
    glfwSwapInterval(1);

    context c;

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
    {
        auto positions = reinterpret_cast<glm::vec2*>(glMapBufferRange(
            GL_COPY_WRITE_BUFFER, 0, position_capacity * sizeof(glm::vec2),
            GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
        ));
        c.positions = {positions, positions + position_capacity};
    }

    c.positions[0] = {0, 0};
    c.positions[1] = {0.1, 0.1};
    c.positions[2] = {0.2, 0.1};

    glBindBuffer(GL_COPY_WRITE_BUFFER, line_buffer);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, line_capacity * sizeof(glm::vec2), nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    auto lines = reinterpret_cast<unsigned short*>(glMapBufferRange(
        GL_COPY_WRITE_BUFFER, 0, line_capacity * sizeof(unsigned short),
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    ));

    c.positions[0] = {0, 0};
    c.positions[1] = {0.1, 0.1};
    c.positions[2] = {0.2, 0.1};

    lines[0] = 0;
    lines[1] = 1;
    lines[2] = 1;
    lines[3] = 2;

    enum : GLuint {
        position_location,
    };

    ge1::unique_program program = ge1::compile_program(
        "shader/position_vertex.glsl", nullptr, nullptr, nullptr,
        "shader/lines_fragment.glsl", {},
        {{"position", position_location}}
    );

    GLuint view_matrix_location;

    ge1::get_uniform_locations(
        program.get_name(), {{"view_matrix", &view_matrix_location}}
    );

    current_context = &c;
    c.current_operation = nullptr;


    glfwSetCursorPosCallback(window, &cursor_position_callback);
    glfwSetMouseButtonCallback(window, &mouse_button_callback);
    glfwSetWindowSizeCallback(window, &window_size_callback);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    window_size_callback(window, width, height);

    c.view_center = glm::vec2(0);
    c.view_right = glm::vec2(1, 0);
    float aspect_ratio = static_cast<float>(c.width) / c.height;
    c.view_matrix = {
        c.view_right.x, c.view_right.y * aspect_ratio,
        -c.view_right.y, c.view_right.x * aspect_ratio,
        c.view_center.x, c.view_center.y,
    };

    while (!glfwWindowShouldClose(window)) {
        glClearColor(255, 255, 255, 255);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program.get_name());
        glUniformMatrix3x2fv(
            view_matrix_location, 1, GL_FALSE, glm::value_ptr(c.view_matrix)
        );
        glBindVertexArray(line_array.get_name());
        glDrawElements(GL_LINES, 4, GL_UNSIGNED_SHORT, 0);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    return 0;
}
