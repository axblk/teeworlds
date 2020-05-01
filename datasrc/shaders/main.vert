#version 450

layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec3 a_TexCoord;
layout(location = 2) in vec4 a_Color;

layout(location = 0) out vec3 v_TexCoord;
layout(location = 1) out vec4 v_Color;

layout(set = 0, binding = 0) uniform Locals {
    mat4 u_Transform;
};

void main() {
	v_TexCoord = a_TexCoord;
	v_Color = a_Color;
	gl_Position = u_Transform * vec4(a_Pos.xy, 0.0, 1.0);
}
