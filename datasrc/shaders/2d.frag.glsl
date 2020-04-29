#version 450

layout(set = 1, binding = 0) uniform texture2D t_Color;
layout(set = 1, binding = 1) uniform sampler s_Color;

layout(location = 0) in vec3 v_TexCoord;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 f_Color;

void main() {
	f_Color = texture(sampler2D(t_Color, s_Color), v_TexCoord.xy) * v_Color;
}
