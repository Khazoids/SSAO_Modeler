#version 330 core

layout (location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D ssao;

uniform bool ambientOcclusionOn;
uniform bool attenuationOn;

struct Light {
	vec3 Position;
	vec3 Color;

};

uniform Light light;

void main()
{
	vec3 FragPos = texture(gPosition, TexCoords).rgb;
	vec3 Normal = texture(gNormal, TexCoords).rgb;
	vec3 Diffuse = texture(gAlbedo, TexCoords).rgb;
	float AmbientOcclusion = texture(ssao, TexCoords).r;

	
	vec3 ambient = ambientOcclusionOn ? vec3(0.3 * Diffuse * AmbientOcclusion) : vec3(0.3 * Diffuse);
	vec3 viewDir = normalize(-FragPos);

	vec3 lightDir = normalize(light.Position - FragPos);
	float geometryTerm = max(dot(Normal, lightDir), 0.0);
	vec3 diffuse = geometryTerm * Diffuse * light.Color;

	float shininess = 32.0;
	vec3 halfAngle = normalize(lightDir + viewDir);
	float specularTerm = max(0.0, dot(Normal, halfAngle));
	vec3 specular = pow(specularTerm, shininess) * light.Color;

	float linear = 0.09f;
	float quadratic = 0.032f;
	float dist = length(light.Position - FragPos);
	float attenuation = 1.0 / (1.0 + linear * dist * quadratic * dist * dist);

	if (attenuationOn)
	{
		diffuse *= attenuation;
		specular *= attenuation;
	}
	

	FragColor = vec4(ambient + diffuse + specular, 1.0);
}