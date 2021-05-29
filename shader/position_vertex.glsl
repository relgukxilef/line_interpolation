#version 450
in vec2 position;

uniform mat3x2 view_matrix;

void main(void) {
    gl_Position = vec4(view_matrix * vec3(position, 1.0), 0.0, 1.0);
}
