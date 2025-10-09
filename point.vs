#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 instancePos;
layout(location=2) in vec3 instanceColor;

uniform mat4 projection;
uniform mat4 view;

out vec3 fragColor;

void main() {
    gl_Position = projection * view * vec4(aPos + instancePos,1.0);
    gl_PointSize = 5.0;
    fragColor = instanceColor;
}
