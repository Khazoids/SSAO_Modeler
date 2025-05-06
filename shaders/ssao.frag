#version 330 core

layout (location = 0) out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform float windowWidth;
uniform float windowHeight;

float kernelSize = 64;
float radius = 0.5;
float bias = 0.025;

void main()
{

	vec3 fragPos = texture(gPosition, TexCoords).xyz;
	vec3 normal = normalize(texture(gNormal, TexCoords).rgb);

	vec2 noiseScale = vec2(windowWidth / 4.0, windowHeight / 4.0);
	vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz);

	vec3 T = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 B = cross(normal, T);
	mat3 TBN = mat3(T, B, normal);

	float occlusion = 0.0;
	for(int i = 0; i < kernelSize; i++)
	{
		vec3 samplePos = TBN * samples[i];
		samplePos = fragPos + samplePos * radius;

		vec4 offset = vec4(samplePos, 1.0);
		offset = projection * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5 + 0.5;

		float sampleDepth = texture(gPosition, offset.xy).z;

		float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = 1.0 - (occlusion / kernelSize);

	FragColor = occlusion;
}