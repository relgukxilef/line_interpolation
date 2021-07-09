#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ge1/vertex_array.h"
#include "ge1/program.h"
#include "ge1/resources.h"
#include "ge1/algorithm.h"

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
    ge1::span<glm::vec2> vertices_position;
    ge1::span<unsigned short> vertices_selection;
    ge1::span<unsigned short> lines_vertex;
    ge1::span<unsigned short> selection_vertex;
    unsigned vertex_count;
    unsigned line_count;
    unsigned selection_count;

    glm::vec2 view_center, view_right;
    glm::mat3x2 view_matrix;

    unsigned width, height;
};

void add_vertex(context& c) {
    assert(c.vertex_count <= c.vertices_position.size());

    ge1::permutation_push_back(
        c.vertices_selection, c.selection_vertex, c.vertex_count
    );

    c.vertex_count++;
}

template<class T>
ge1::unique_buffer allocate_buffer(unsigned size, ge1::span<T>& data) {
    GLuint name;
    glCreateBuffers(1, &name);
    glBindBuffer(GL_ARRAY_BUFFER, name);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, size * sizeof(T), nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    auto begin = reinterpret_cast<T*>(glMapBufferRange(
        GL_COPY_WRITE_BUFFER, 0, size * sizeof(T),
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    ));
    data = {begin, begin + size};
    return {name};
}

glm::vec2 to_canvas(context &c, glm::vec2 screen_position) {
    auto ndc = 2.f * screen_position / glm::vec2(c.width, c.height) - 1.f;
    ndc.y = -ndc.y;
    return glm::inverse(glm::mat2(c.view_matrix)) * (ndc - c.view_center);
}

unsigned get_closest_vertex(context &c, glm::vec2 position) {
    auto offset = c.vertices_position[0] - position;
    float closest_distance = dot(offset, offset);
    auto index = 0;

    for (unsigned i = 1; i < c.vertex_count; i++) {
        auto offset = c.vertices_position[i] - position;
        auto distance = dot(offset, offset);
        if (distance < closest_distance) {
            closest_distance = distance;
            index = i;
        }
    }

    return index;
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
        if (c.vertex_count > 0) {
            c.current_operation = this;
            old_position = to_canvas(c, {x, y});

            index = get_closest_vertex(c, old_position);
        }
    }
    void mouse_move_event(context &c, double x, double y) override {
        glm::vec2 position = to_canvas(c, {x, y});
        auto delta = position - old_position;

        c.vertices_position[index] += delta;

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

struct extrude_vertex : public operation {
    void trigger(context& c, double x, double y) override {
        if (c.selection_count == 1) {
            glm::vec2 position = to_canvas(c, {x, y});
            c.lines_vertex[c.line_count++] = c.selection_vertex[0];
            c.lines_vertex[c.line_count++] = c.vertex_count;

            add_vertex(c);
            c.vertices_position[c.vertex_count - 1] = position;
        }
    }
    void mouse_move_event(context&, double, double) override {}
    void mouse_button_event(context&, int, int, int) override {}
    void key_event(context&, int, int, int) override {}
};

struct select_vertex : public operation {
    void trigger(context &c, double x, double y) override {
        if (c.vertex_count > 0) {
            auto position = to_canvas(c, {x, y});
            auto index = get_closest_vertex(c, position);
            if (c.vertices_selection[index] < c.selection_count) {
                c.selection_count--;
                ge1::permutation_swap(
                    c.selection_vertex, c.vertices_selection,
                    c.selection_vertex[c.selection_count], index
                );
            } else {
                ge1::permutation_swap(
                    c.selection_vertex, c.vertices_selection,
                    c.selection_vertex[c.selection_count], index
                );
                c.selection_count++;
            }
        }
    }
    void mouse_move_event(context &, double, double) override {};
    void mouse_button_event(context &, int, int, int) override {};
    void key_event(context &, int, int, int) override {};
};

static context* current_context;

static pan_operation pan_operation;
static drag_operation drag_operation;
static extrude_vertex extrude_vertex_operation;
static select_vertex select_vertex_operation;

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

        if (button == GLFW_MOUSE_BUTTON_LEFT && !modifiers) {
            drag_operation.trigger(*current_context, x, y);
        } else if (
            button == GLFW_MOUSE_BUTTON_LEFT && modifiers & GLFW_MOD_CONTROL
        ) {
            select_vertex_operation.trigger(*current_context, x, y);
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE && !modifiers) {
            pan_operation.trigger(*current_context, x, y);
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {

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

void key_callback(
    GLFWwindow* window, int key, int scancode, int action, int modifiers
) {
    if (current_context->current_operation) {
        current_context->current_operation->key_event(
            *current_context, key, scancode, modifiers
        );

    } else {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_E) {
                extrude_vertex_operation.trigger(*current_context, x, y);
            } else if (key == GLFW_KEY_DELETE) {

            }
        }
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
        position, selected,
    };

    GLuint position_buffer, selection_buffer, line_buffer;

    unsigned vertex_capacity = 1024;
    unsigned line_capacity = 1024;

    ge1::unique_vertex_array line_array = ge1::create_vertex_array(
        vertex_capacity, {
            {
                {{position, 2, GL_FLOAT, GL_FALSE, 0}},
                0, GL_DYNAMIC_DRAW, &position_buffer
            }, {
                // TODO
                {{selected, 1, GL_UNSIGNED_SHORT, GL_FALSE, 0}},
                0, GL_DYNAMIC_DRAW, &selection_buffer
            },
        }, line_capacity, &line_buffer, GL_UNSIGNED_SHORT
    );
    glBindBuffer(GL_ARRAY_BUFFER, selection_buffer);
    glVertexAttribIPointer(selected, 1, GL_UNSIGNED_SHORT, 0, 0);

    ge1::unique_vertex_array vertex_array = ge1::create_vertex_array(
        {
            {position_buffer, position, 2, GL_FLOAT, GL_FALSE, 0, 0},
        }
    );
    glBindBuffer(GL_ARRAY_BUFFER, selection_buffer);
    glEnableVertexAttribArray(selected);
    glVertexAttribIPointer(selected, 1, GL_UNSIGNED_SHORT, 0, 0);

    glBindBuffer(GL_COPY_WRITE_BUFFER, position_buffer);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, vertex_capacity * sizeof(glm::vec2), nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    {
        auto positions = reinterpret_cast<glm::vec2*>(glMapBufferRange(
            GL_COPY_WRITE_BUFFER, 0, vertex_capacity * sizeof(glm::vec2),
            GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
        ));
        c.vertices_position = {positions, positions + vertex_capacity};
    }

    glBindBuffer(GL_COPY_WRITE_BUFFER, selection_buffer);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, vertex_capacity * sizeof(unsigned short),
        nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    {
        auto selected = reinterpret_cast<unsigned short*>(glMapBufferRange(
            GL_COPY_WRITE_BUFFER, 0, vertex_capacity * sizeof(unsigned short),
            GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
        ));
        c.vertices_selection = {selected, selected + vertex_capacity};
    }


    c.selection_vertex = ge1::span<unsigned short>(vertex_capacity);
    c.vertex_count = 0;
    c.line_count = 0;
    c.selection_count = 0;

    for (auto i = 0u; i < 3; i++) {
        add_vertex(c);
    }

    c.vertices_position[0] = {0, 0};
    c.vertices_position[1] = {0.1, 0.1};
    c.vertices_position[2] = {0.2, 0.1};

    glBindBuffer(GL_COPY_WRITE_BUFFER, line_buffer);
    glBufferStorage(
        GL_COPY_WRITE_BUFFER, line_capacity * sizeof(glm::vec2), nullptr,
        GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
    );
    {
        auto lines = reinterpret_cast<unsigned short*>(glMapBufferRange(
            GL_COPY_WRITE_BUFFER, 0, line_capacity * sizeof(unsigned short),
            GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT
        ));
        c.lines_vertex = {lines, lines + line_capacity};
    }

    c.lines_vertex[0] = 0;
    c.lines_vertex[1] = 1;
    c.lines_vertex[2] = 1;
    c.lines_vertex[3] = 2;

    c.line_count = 4;

    enum : GLuint {
        position_location,
    };

    ge1::unique_program program = ge1::compile_program(
        "shader/position_vertex.glsl", nullptr, nullptr, nullptr,
        "shader/lines_fragment.glsl", {},
        {{"position", position_location}}
    );

    GLuint view_matrix_location, selection_count_location;

    ge1::get_uniform_locations(
        program.get_name(), {
            {"view_matrix", &view_matrix_location},
            {"selection_count", &selection_count_location},
        }
    );

    glPointSize(10.0f);

    current_context = &c;
    c.current_operation = nullptr;


    glfwSetCursorPosCallback(window, &cursor_position_callback);
    glfwSetMouseButtonCallback(window, &mouse_button_callback);
    glfwSetKeyCallback(window, &key_callback);
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
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program.get_name());
        glUniformMatrix3x2fv(
            view_matrix_location, 1, GL_FALSE, glm::value_ptr(c.view_matrix)
        );
        glUniform1ui(selection_count_location, c.selection_count);
        glBindVertexArray(line_array.get_name());
        glDrawElements(GL_LINES, c.line_count, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(vertex_array.get_name());
        glDrawArrays(GL_POINTS, 0, c.vertex_count);

        glFinish();

        glfwSwapBuffers(window);

        glfwWaitEvents();
    }

    return 0;
}
