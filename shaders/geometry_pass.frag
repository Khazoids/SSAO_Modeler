#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normals;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform bool readTexture;

void main()
{
	gPosition = FragPos;
	gNormal = normalize(Normals);

	if(readTexture)
	{
		gAlbedo.rgb = texture(texture_diffuse1, TexCoords).rgb;
		gAlbedo.a = texture(texture_specular1, TexCoords).r;
	}
	else
	{
		gAlbedo = vec4(0.95f, 0.95f, 0.95f, 1.0);
	}
	
}