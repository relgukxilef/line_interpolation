#version 450
uniform mat3x2 view_matrix;
uniform uint selection_count;

in vec2 position;
in uint selection;

out float vertex_selected;


void main(void) {
    gl_Position = vec4(view_matrix * vec3(position, 1.0), 0.0, 1.0);
    vertex_selected = float(selection < selection_count);
}
