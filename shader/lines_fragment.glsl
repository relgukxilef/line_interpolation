#version 450
in float vertex_selected;

out vec3 color;

void main(void) {
    color = mix(vec3(0), vec3(1, 0, 0), vertex_selected);
}
