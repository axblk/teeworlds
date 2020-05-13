#version 450

layout(set = 0, binding = 0) uniform texture2D t_Color;
layout(set = 0, binding = 1) uniform sampler s_Color;

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 o_Target;

void main() {
	o_Target = textureLod(sampler2D(t_Color, s_Color), v_TexCoord, 0.0);
}
