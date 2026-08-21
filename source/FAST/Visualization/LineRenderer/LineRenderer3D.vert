#version 330 core
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;
layout (location = 2) in uint in_label;

layout (std140) uniform Colors {
    vec4 color[256];
};

out vec4 vertexColor;

uniform mat4 transform;
uniform mat4 viewTransform;
uniform mat4 perspectiveTransform;
uniform bool useGlobalColor;
uniform bool useLabelColor;
uniform vec3 globalColor;
uniform float opacity;

void main() {
    gl_Position = perspectiveTransform * viewTransform * transform * vec4(in_position, 1.0);
    if(useGlobalColor) {
        vertexColor = vec4(globalColor, opacity);
    } else if(useLabelColor) {
        vertexColor = vec4(color[in_label].rgb, opacity);
    } else {
        vertexColor = vec4(in_color, opacity);
    }
}
