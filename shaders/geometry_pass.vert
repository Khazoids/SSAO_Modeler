#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormals;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normals;

uniform bool invertedNormals;
uniform bool invertedAxisYZ;

uniform mat4 mvp;
uniform mat4 mv;


void main()
{

	
		vec4 viewPos = mv * vec4(aPos, 1.0);
		FragPos = viewPos.xyz;
		TexCoords = aTexCoords;

		mat3 normalMatrix = transpose(inverse(mat3(mv)));
		Normals = normalMatrix * (invertedNormals ? -aNormals : aNormals);

		gl_Position = mvp * vec4(aPos, 1.0);
	
}