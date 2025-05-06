#include <GL/glew.h>
#include <GL/freeglut.h>

#include <cyTriMesh.h>
#include <cyVector.h>
#include <cyCore.h>
#include <cyGL.h>
#include <cyMatrix.h>

#include "ClientState.h"
#include "Model.h"
#include "Scene.h"
#include "Transformation.h"

#include <random>

#define WINDOW_HEIGHT 1080
#define WINDOW_WIDTH 1920
#define PI 3.14159265358979323846
#define DEG2RAD(degrees) (degrees * PI / 180.0)

unsigned int lightIndex = 0;
const cyVec3f lightColors[3]{ cyVec3f(0.5, 0.5, 0.5), cyVec3f(0.2, 0.2, 0.7), cyVec3f(0.7, 0.2, 0.2) };

const unsigned int NUM_SAMPLES = 64;

bool ambientOcclusionOn, ssaoBlurOn, attenuationOn = false;

GLuint CubeVAO, QuadVAO;

GLuint gBuffer;
GLuint gPosition, gNormal, gAlbedo, noiseTexture;

GLuint ssaoFBO, ssaoColorBuffer;
GLuint ssaoBlurFBO, ssaoColorBufferBlur;

cyGLSLProgram GeometryPassProgram, SSAO_Program, LightingPassProgram, BlurProgram;

ClientState client;

Transformation CubeTransformation, QuadTransformation;
Model* TerrariumModel, * TeapotModel, * BackpackModel, * StairModel, * DoorModel, * AmeModel, * ObjectModel;
std::vector<Model*> scene;

// Render delcarations
void RenderCube(cyGLSLProgram& p, Transformation& t);
void RenderQuad(cyGLSLProgram& p, Transformation& t);

// GLUT callback delcarations
void MouseAction(int b, int s, int x, int y);
void MouseMove(int x, int y);
void KeyboardAction(unsigned char k, int x, int y);
void SpecialInput(int k, int x, int y);
void Idle();
static void RenderSceneCB();

// Helper method declarations
void Init();
void Init(char** argv);

void InitializeGlutCallBacks();
bool InitGBuffer();
cyMatrix4f GetModelViewProjection(Transformation t);
cyMatrix4f GetModelViewTransformation(Transformation t);
static void CompileShaders();
float GetRelativeDisplacement(Transformation ob1, Transformation ob2, float displacementAmt);
void CreateQuadVAO();
void CreateCubeVAO();
bool CreateRenderBuffer();
std::vector<cyVec3f> GenerateSampleKernel(unsigned int num_samples);
void GenerateNoiseTexture(unsigned int num_samples);

int main(int argc, char** argv) {
	glutInit(&argc, argv);

	// window initialization
	glutInitContextFlags(GLUT_DEBUG);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutInitWindowPosition(100, 100);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutCreateWindow("Final Project");

	// GLEW initialization
	GLenum res = glewInit();
	if (res != GLEW_OK) {
		fprintf(stderr, "Error initializing GLEW.");
		exit(1);
	}

	CY_GL_REGISTER_DEBUG_CALLBACK;

	// callback registrations
	InitializeGlutCallBacks();


	if (argc < 2) Init();
	else Init(argv);


	glutMainLoop();

	for (Model* model : scene)
	{
		delete model;
	}

	return 0;
}

void InitializeGlutCallBacks()
{
	glutIdleFunc(Idle);
	glutDisplayFunc(RenderSceneCB);
	glutMouseFunc(MouseAction);
	glutMotionFunc(MouseMove);
	glutKeyboardFunc(KeyboardAction);
	glutSpecialFunc(SpecialInput);
}

void Init(char** argv)
{
	CompileShaders();

	ObjectModel = new Model(argv[1], false);
	ObjectModel->transformation.SetScale(0.25f);
	scene.push_back(ObjectModel);
	CubeTransformation.SetScale(2.0f);

	// SSAO setup
	if (!CreateRenderBuffer())
	{
		fprintf(stderr, "Error initializing SSAO frame buffer object");
		exit(1);
	}

	if (!InitGBuffer()) {
		fprintf(stderr, "Error initializing gBuffer.");
		exit(1);
	}

	GenerateNoiseTexture(NUM_SAMPLES);

	// setup for hard-coded models
	CreateCubeVAO();
	CreateQuadVAO();

	glEnable(GL_DEPTH_TEST);
}

void Init()
{
	CompileShaders();

	// Load models
	TerrariumModel = new Model("resources/ame_terrarium/scene.gltf");
	TeapotModel = new Model("resources/teapot/teapot.obj");
	BackpackModel = new Model("resources/backpack/backpack.obj", false);
	StairModel = new Model("resources/staircase/scene.gltf");
	DoorModel = new Model("resources/wooden_door/scene.gltf");
	AmeModel = new Model("resources/ame/scene.gltf");

	// Place model in world space according to scene
	CubeTransformation.SetScale(2.0f);

	TerrariumModel->transformation.SetScale(0.25f);
	TerrariumModel->transformation.IncrementTranslation(0.0f, -7.9f, 0.0f);

	AmeModel->transformation.SetScale(0.25f);
	AmeModel->transformation.IncrementTranslation(2.0f, -8.0f, -2.0f);

	TeapotModel->transformation.SetScale(0.02f);
	TeapotModel->transformation.IncrementRotation(-DEG2RAD(90), 0.0f, 0.0f);
	TeapotModel->transformation.IncrementTranslation(-30.0f, 5.0f, -(CubeTransformation.GetUniformScale() / TeapotModel->transformation.GetUniformScale()));
	TeapotModel->invertZ = true;

	StairModel->transformation.SetScale(cyVec3f(0.005));
	StairModel->transformation.IncrementTranslation(0.0f, -400.0f, 200.0f);
	StairModel->transformation.IncrementRotation(0.0, -DEG2RAD(180), 0.0f);
	StairModel->invertY = true;

	BackpackModel->transformation.SetScale(0.25f);
	BackpackModel->transformation.IncrementTranslation(0.0f, 2.0f, -(CubeTransformation.GetUniformScale() / BackpackModel->transformation.GetUniformScale()) + 1.5f);

	DoorModel->transformation.SetScale(0.25f);
	DoorModel->transformation.IncrementRotation(-DEG2RAD(90), 0.0f, 0.0f);
	DoorModel->transformation.IncrementTranslation(
		0.0f,
		(CubeTransformation.GetUniformScale() / DoorModel->transformation.GetUniformScale() - 0.1f),
		-6.5);
	DoorModel->invertZ = true;

	// push pointers to model objects so that operations affecting the entire scene can be calculated with a loop
	scene.push_back(TerrariumModel);
	scene.push_back(TeapotModel);
	scene.push_back(BackpackModel);
	scene.push_back(StairModel);
	scene.push_back(DoorModel);
	scene.push_back(AmeModel);

	// SSAO setup
	if (!CreateRenderBuffer())
	{
		fprintf(stderr, "Error initializing SSAO frame buffer object");
		exit(1);
	}

	if (!InitGBuffer()) {
		fprintf(stderr, "Error initializing gBuffer.");
		exit(1);
	}

	GenerateNoiseTexture(NUM_SAMPLES);

	// setup for hard-coded models
	CreateCubeVAO();
	CreateQuadVAO();

	glEnable(GL_DEPTH_TEST);
}

static void CompileShaders()
{
	// compile gBuffer shaders
	if (!GeometryPassProgram.BuildFiles("shaders/geometry_pass.vert", "shaders/geometry_pass.frag")) exit(1);

	// compile ssao shaders
	if (!SSAO_Program.BuildFiles("shaders/ssao.vert", "shaders/ssao.frag")) exit(1);
	SSAO_Program.Bind();
	SSAO_Program.SetUniform("gPosition", 0);
	SSAO_Program.SetUniform("gNormal", 1);
	SSAO_Program.SetUniform("texNoise", 2);
	SSAO_Program.SetUniform("windowWidth", (float)WINDOW_WIDTH);
	SSAO_Program.SetUniform("windowHeight", (float)WINDOW_HEIGHT);

	// set one time uniforms
	std::vector<cyVec3f> sampleKernel = GenerateSampleKernel(NUM_SAMPLES);
	for (unsigned int i = 0; i < NUM_SAMPLES; i++)
	{
		std::string locationStr = "samples[" + std::to_string(i) + "]";

		float buffer[3];
		sampleKernel[i].Get(buffer);
		glUniform3fv(glGetUniformLocation(SSAO_Program.GetID(), locationStr.c_str()), 1, buffer);
	}


	float perspective[16];
	cy::Matrix4f::Perspective(DEG2RAD(45.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 1000.0f).Get(perspective);
	SSAO_Program.SetUniformMatrix4("projection", perspective);

	// compile ssao blur shaders
	if (!BlurProgram.BuildFiles("shaders/ssao.vert", "shaders/blur.frag")) exit(1);
	BlurProgram.Bind();
	BlurProgram.SetUniform("ssaoInput", 0);

	// compile lighting pass shaders
	if (!LightingPassProgram.BuildFiles("shaders/ssao.vert", "shaders/lighting_pass.frag")) exit(1);

	// set texture uniforms
	LightingPassProgram.Bind();
	LightingPassProgram.SetUniform("gPosition", 0);
	LightingPassProgram.SetUniform("gNormal", 1);
	LightingPassProgram.SetUniform("gAlbedo", 2);
	LightingPassProgram.SetUniform("ssao", 3);

	// set light uniforms
	LightingPassProgram.SetUniform3("light.Position", &cyVec3f(2.0, 4.0, 6.0)[0]);
}

bool InitGBuffer()
{
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);


	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);


	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);


	glGenTextures(1, &gAlbedo);
	glBindTexture(GL_TEXTURE_2D, gAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);


	GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	GLuint rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WINDOW_WIDTH, WINDOW_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	return true;
}

bool CreateRenderBuffer()
{
	glGenFramebuffers(1, &ssaoFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

	glGenTextures(1, &ssaoColorBuffer);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	glGenFramebuffers(1, &ssaoBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);

	glGenTextures(1, &ssaoColorBufferBlur);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return true;
}

void GenerateNoiseTexture(unsigned int num_samples)
{
	std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0);
	std::default_random_engine generator;

	std::vector<cyVec3f> ssaoNoise;
	for (unsigned int i = 0; i < num_samples; i++)
	{
		cyVec3f noise(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			0.0f
		);

		ssaoNoise.push_back(noise);
	}


	glGenTextures(1, &noiseTexture);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// Set to repeat so that the texture tiles the screen
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void CreateQuadVAO()
{
	GLuint QuadVBO[2];
	glGenVertexArrays(1, &QuadVAO);
	glBindVertexArray(QuadVAO);
	glGenBuffers(2, QuadVBO);

	std::vector<cy::Vec3f> vertices = {
		cyVec3f(-1.0f, 1.0f, 0.0f),
		cyVec3f(-1.0f, -1.0f, 0.0f),
		cyVec3f(1.0f, 1.0f, 0.0f),
		cyVec3f(1.0f, -1.0f, 0.0f),
	};

	std::vector<cy::Vec2f> tex_coords = {
		cyVec2f(0.0f, 1.0f),
		cyVec2f(0.0f, 0.0f),
		cyVec2f(1.0f, 1.0f),
		cyVec2f(1.0f, 0.0f),
	};

	glBindBuffer(GL_ARRAY_BUFFER, QuadVBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

	glBindBuffer(GL_ARRAY_BUFFER, QuadVBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec2f) * tex_coords.size(), &tex_coords[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}
void CreateCubeVAO()
{
	GLuint CubeVBO[3];
	glGenVertexArrays(1, &CubeVAO);
	glBindVertexArray(CubeVAO);
	glGenBuffers(3, CubeVBO);

	// positions
	cyVec3f pos1(-1.0f, -1.0f, -1.0f);
	cyVec3f pos2(-1.0f, 1.0f, -1.0f);
	cyVec3f pos3(1.0f, 1.0f, -1.0f);
	cyVec3f pos4(1.0f, -1.0f, -1.0f);
	cyVec3f pos5(-1.0f, -1.0f, 1.0f);
	cyVec3f pos6(1.0f, -1.0f, 1.0f);
	cyVec3f pos7(1.0f, 1.0f, 1.0f);
	cyVec3f pos8(-1.0f, 1.0f, 1.0f);

	// normals
	cyVec3f norm1(0.0f, 0.0f, -1.0f);
	cyVec3f norm2(0.0f, 0.0f, 1.0f);
	cyVec3f norm3(-1.0f, 0.0, 0.0);
	cyVec3f norm4(1.0f, 0.0f, 0.0f);
	cyVec3f norm5(0.0f, -1.0f, 0.0f);
	cyVec3f norm6(0.0f, 1.0f, 0.0f);

	// texture coordinates
	cyVec2f txc1(0.0f, 0.0f);
	cyVec2f txc2(1.0f, 1.0f);
	cyVec2f txc3(1.0f, 0.0f);
	cyVec2f txc4(0.0f, 1.0f);

	std::vector<cyVec3f> vertices =
	{
		pos1, pos3, pos4, pos3, pos1, pos2,
		pos5, pos6, pos7, pos7, pos8, pos5,
		pos8, pos2, pos1, pos1, pos5, pos8,
		pos7, pos4, pos3, pos4, pos7, pos6,
		pos1, pos4, pos6, pos6, pos5, pos1,
		pos2, pos7, pos3, pos7, pos2, pos8
	};

	std::vector<cyVec3f> normals =
	{
		norm1, norm1, norm1, norm1, norm1, norm1,
		norm2, norm2, norm2, norm2, norm2, norm2,
		norm3, norm3, norm3, norm3, norm3, norm3,
		norm4, norm4, norm4, norm4, norm4, norm4,
		norm5, norm5, norm5, norm5, norm5, norm5,
		norm6, norm6, norm6, norm6, norm6, norm6
	};

	std::vector<cyVec2f> texCoords =
	{
		txc1, txc2, txc3, txc2, txc1, txc4,
		txc1, txc3, txc2, txc2, txc4, txc1,
		txc3, txc2, txc4, txc4, txc1, txc3,
		txc3, txc4, txc2, txc4, txc3, txc1,
		txc4, txc2, txc3, txc3, txc1, txc4,
		txc4, txc3, txc2, txc3, txc4, txc1
	};

	glBindBuffer(GL_ARRAY_BUFFER, CubeVBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

	glBindBuffer(GL_ARRAY_BUFFER, CubeVBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f) * normals.size(), &normals[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

	glBindBuffer(GL_ARRAY_BUFFER, CubeVBO[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec2f) * 36, &texCoords[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

static void RenderSceneCB()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	float mvp_buffer[16];
	float mv_buffer[16];

	// Geometry pass. Render into gBuffer
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindVertexArray(CubeVAO);
	GeometryPassProgram.Bind();
	GeometryPassProgram.SetUniform("readTexture", false);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	RenderCube(::GeometryPassProgram, ::CubeTransformation);
	GeometryPassProgram.SetUniform("invertedNormals", false);
	GetModelViewProjection(::TerrariumModel->transformation).Get(mvp_buffer);
	GetModelViewTransformation(::TerrariumModel->transformation).Get(mv_buffer);
	GeometryPassProgram.SetUniform("readTexture", true);
	GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	GeometryPassProgram.SetUniform("invertedNormals", false);
	glCullFace(GL_BACK);

	TerrariumModel->Draw(::GeometryPassProgram);

	GetModelViewProjection(::AmeModel->transformation).Get(mvp_buffer);
	GetModelViewTransformation(::AmeModel->transformation).Get(mv_buffer);
	GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	AmeModel->Draw(::GeometryPassProgram);

	GetModelViewProjection(::StairModel->transformation).Get(mvp_buffer);
	GetModelViewTransformation(::StairModel->transformation).Get(mv_buffer);
	GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	StairModel->Draw(::GeometryPassProgram);

	GetModelViewProjection(::DoorModel->transformation).Get(mvp_buffer);
	GetModelViewTransformation(::DoorModel->transformation).Get(mv_buffer);
	GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	DoorModel->Draw(::GeometryPassProgram);

	glDisable(GL_CULL_FACE);

	GetModelViewProjection(::BackpackModel->transformation).Get(mvp_buffer);
	GetModelViewTransformation(::BackpackModel->transformation).Get(mv_buffer);
	GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	BackpackModel->Draw(::GeometryPassProgram);

	GetModelViewProjection(::TeapotModel->transformation).Get(mvp_buffer);
	GetModelViewTransformation(::TeapotModel->transformation).Get(mv_buffer);
	GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	TeapotModel->Draw(::GeometryPassProgram);

	//GetModelViewProjection(::ObjectModel->transformation).Get(mvp_buffer);
	//GetModelViewTransformation(::ObjectModel->transformation).Get(mv_buffer);
	//GeometryPassProgram.SetUniformMatrix4("mvp", mvp_buffer);
	//GeometryPassProgram.SetUniformMatrix4("mv", mv_buffer);
	//ObjectModel->Draw(::GeometryPassProgram);

	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Generate SSAO Texture
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
	glClear(GL_COLOR_BUFFER_BIT);
	SSAO_Program.Bind();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosition);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);

	glBindVertexArray(QuadVAO);
	RenderQuad(::SSAO_Program, ::QuadTransformation);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// Blur SSAO texture
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
	glClear(GL_COLOR_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);

	RenderQuad(::BlurProgram, ::QuadTransformation);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Lighting pass
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	LightingPassProgram.Bind();
	LightingPassProgram.SetUniform("ambientOcclusionOn", ambientOcclusionOn);
	LightingPassProgram.SetUniform("attenuationOn", attenuationOn);
	LightingPassProgram.SetUniform3("light.Color", &lightColors[lightIndex][0]);


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gAlbedo);
	glActiveTexture(GL_TEXTURE3); // add extra SSAO texture to lighting pass

	ssaoBlurOn ? glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur) : glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);

	RenderQuad(::LightingPassProgram, ::QuadTransformation);

	glutSwapBuffers();
}

void RenderCube(cyGLSLProgram& Program, Transformation& Model)
{
	Program.Bind();

	float obj_mvp_buffer[16];
	GetModelViewProjection(Model).Get(obj_mvp_buffer);
	Program.SetUniformMatrix4("mvp", obj_mvp_buffer);

	float obj_mv_buffer[16];
	GetModelViewTransformation(Model).Get(obj_mv_buffer);
	Program.SetUniformMatrix4("mv", obj_mv_buffer);

	Program.SetUniform("invertedNormals", true);

	glDrawArrays(GL_TRIANGLES, 0, 36);
}

void RenderQuad(cyGLSLProgram& Program, Transformation& Model)
{
	Program.Bind();

	float obj_mvp_buffer[16];
	GetModelViewProjection(Model).Get(obj_mvp_buffer);
	Program.SetUniformMatrix4("mvp", obj_mvp_buffer);

	float obj_mv_buffer[16];
	GetModelViewTransformation(Model).Get(obj_mv_buffer);
	Program.SetUniformMatrix4("mv", obj_mv_buffer);

	Program.SetUniform("invertedNormals", false);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

float cameraZ = 1.5f;
void KeyboardAction(unsigned char key, int x, int y) {
	switch (key) {
	case 27: // esc
		glutLeaveMainLoop();
		break;
	case 32: // space
		ambientOcclusionOn = !ambientOcclusionOn;
		break;
	case 48:
		lightIndex = 0;
		break;
	case 49:
		lightIndex = 1;
		break;
	case 50:
		lightIndex = 2;
		break;
	case 87:
	case 119:
		cameraZ -= 0.05f;
		break;
	case 115:
	case 83:
		cameraZ += 0.05f;
	}
}

void SpecialInput(int key, int x, int y)
{
	float sceneDisplacement = 0.1f;
	switch (key)
	{
	case GLUT_KEY_CTRL_L:
		attenuationOn = !attenuationOn;
		break;
	case GLUT_KEY_ALT_L:
		ssaoBlurOn = !ssaoBlurOn;
		break;
	case GLUT_KEY_UP:
		for (Model* m : scene)
		{
			if (m->invertZ)
				m->transformation.IncrementTranslation(0.0f, 0.0f, -(GetRelativeDisplacement(CubeTransformation, m->transformation, sceneDisplacement)));
			else
				m->transformation.IncrementTranslation(0.0f, -(GetRelativeDisplacement(CubeTransformation, m->transformation, sceneDisplacement)), 0.0f);
		}
		CubeTransformation.IncrementTranslation(0.0f, -sceneDisplacement, 0.0f);
		break;
	case GLUT_KEY_DOWN:
		for (Model* m : scene)
		{
			if (m->invertZ)
				m->transformation.IncrementTranslation(0.0f, 0.0, GetRelativeDisplacement(CubeTransformation, m->transformation, sceneDisplacement));
			else
				m->transformation.IncrementTranslation(0.0f, GetRelativeDisplacement(CubeTransformation, m->transformation, sceneDisplacement), 0.0f);
		}
		CubeTransformation.IncrementTranslation(0.0f, sceneDisplacement, 0.0f);
		break;
	}
}

/*
* Returns float value that can be used in an Transformation object's translation component relative to another object.
* The second object's displacement will be relative to the first object. For example, you translate the first object
* and then translate the second object relative to the first object to prevent clipping. This simulates movement
* of a scene.
*
* This calculation is relative to the scale of the objects.
*/
float GetRelativeDisplacement(Transformation ob1, Transformation ob2, float displacementAmount)
{
	return (displacementAmount / ob2.GetUniformScale()) * ob1.GetUniformScale();
}

cyMatrix4f GetModelViewProjection(Transformation t)
{
	cy::Matrix4f model_matrix = cy::Matrix4f::Scale(t.scale)
		* cy::Matrix4f::RotationXYZ(t.rotation.x, t.rotation.y, t.rotation.z)
		* cy::Matrix4f::Translation(t.translation);

	cy::Matrix4f view_matrix = cy::Matrix4f::View(
		cyVec3f(0.0f, 0.0f, cameraZ),
		cyVec3f(0.0f, 0.0f, 0.0f),
		cyVec3f(0.0f, 1.0f, 0.0f)
	);

	cy::Matrix4f projection_matrix = cy::Matrix4f::Perspective(
		DEG2RAD(t.perspective_degrees),
		(float)WINDOW_WIDTH / (float)WINDOW_HEIGHT,
		0.1f, 1000.0f);

	return projection_matrix * view_matrix * model_matrix;
}

cyMatrix4f GetModelViewTransformation(Transformation t)
{
	cy::Matrix4f model_matrix = cy::Matrix4f::Scale(t.scale)
		* cy::Matrix4f::RotationXYZ(t.rotation.x, t.rotation.y, t.rotation.z)
		* cy::Matrix4f::Translation(t.translation);

	cy::Matrix4f view_matrix = cy::Matrix4f::View(
		cyVec3f(0.0, 0.0f, cameraZ),
		cyVec3f(0.0, 0.0, 0.0),
		cyVec3f(0.0, 1.0, 0.0)
	);

	return view_matrix * model_matrix;
}


void MouseAction(int button, int state, int x, int y) {
	client.x = x;
	client.y = y;
	if (GLUT_RIGHT_BUTTON == button) {
		client.rightMouseDown = !state;
		client.y = y;
	}

}

void MouseMove(int x, int y) {


	for (Model* m : scene)
	{
		if (m->invertY)
			m->transformation.IncrementRotation(-(client.y - y) / WINDOW_HEIGHT * 5, (client.x - x) / WINDOW_WIDTH * 5, 0.0);
		else
			m->transformation.IncrementRotation((client.y - y) / WINDOW_HEIGHT * 5, (client.x - x) / WINDOW_WIDTH * 5, 0.0);
	}
	::CubeTransformation.IncrementRotation((client.y - y) / WINDOW_HEIGHT * 5, (client.x - x) / WINDOW_WIDTH * 5, 0.0);
	client.x = x;
	client.y = y;

}

void Idle()
{
	glutPostRedisplay();
}

float lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

std::vector<cyVec3f> GenerateSampleKernel(unsigned int num_samples)
{
	std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0);
	std::default_random_engine generator;

	std::vector<cyVec3f> kernel;
	for (unsigned int i = 0; i < num_samples; i++)
	{
		// generate random samples
		cyVec3f sample(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator)
		);

		sample.Normalize();
		sample *= randomFloats(generator);

		// adjust samples to keep them near the origin.
		float scale = float(i) / num_samples;
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;

		kernel.push_back(sample);
	}

	return kernel;
}