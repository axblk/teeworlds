#version 450

layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec3 a_TexCoord;
layout(location = 2) in vec4 a_Color;

layout(location = 0) out vec3 v_TexCoord;
layout(location = 1) out vec4 v_Color;

mat4 proj = mat4(
	2.0 / 1066.0, 0.0, 0.0, 0.0,
	0.0, 2.0 / (-600.0), 0.0, 0.0,
	0.0, 0.0, 1.0 , 0.0,
	-1.0, 1.0, 0.0, 1.0
);

void main() {
	v_TexCoord = a_TexCoord;
	v_Color = a_Color;
	gl_Position = proj * vec4(a_Pos.xy, 0.0, 1.0);
}
