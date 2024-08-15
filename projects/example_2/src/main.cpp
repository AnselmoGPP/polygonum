#include "renderer.hpp"
#include "toolkit.hpp"
#include "physics.hpp"
#include "ECSarch.hpp"

/* Example project: 3D cube */

// Globals ----------

modelIter skybox;
modelIter cube;

glm::mat4 view;
glm::mat4 proj;
glm::vec3 camPos;
double currentTime;
float deltaTime;
float fov;
Light light;
glm::dvec2 cursorPos;

// Prototypes ----------

void update(Renderer& rend);		// Callback that will be called for each frame (useful for updating model view matrices)
void setGlobalVariables(Renderer& rend);
void updateGlobalVariables(Renderer& rend);
void updateGlobalUBOsState(Renderer& rend);

void createCube(Renderer& rend);
void updateCubeState(Renderer& rend);

void createSkyBox(Renderer& rend);
void updateSkyboxState(Renderer& rend);

// Definitions ----------

int main(int argc, char* argv[])
{
	IOmanager io(1920/2, 1080/2);

	Renderer renderer(update, io,
		UBOinfo(1, 1, 2 * size.mat4 + size.vec4),	// global UBO (vertex shader): View, Proj, camPos_Time
		UBOinfo(1, 1, size.vec4 + sizeof(Light)));	// global UBO (fragment shader): camPos_Time, Lights

	renderer.getTimer().setMaxFPS(30);
	setGlobalVariables(renderer);

	createCube(renderer);							// Geometry pass
	renderer.createLightingPass( 1,					// Lighting pass
		std::string("../../../resources/shaders/lightingPass_v.vert"),
		std::string("../../../resources/shaders/lightingPass_f.frag"));			
	createSkyBox(renderer);							// Forward pass
	renderer.createPostprocessingPass(				// Postprocessing pass
		std::string("../../../resources/shaders/postprocessing_v.vert"),
		std::string("../../../resources/shaders/postprocessing_f.frag"));

	renderer.renderLoop();							// Start render loop

	return 0;
}

void update(Renderer& rend)
{
	updateGlobalVariables(rend);
	updateGlobalUBOsState(rend);

	updateCubeState(rend);
	rend.updateLightingPass(camPos, &light, 1);
	updateSkyboxState(rend);
	rend.updatePostprocessingPass();
}

void createCube(Renderer& rend)
{
	VerticesLoader vertexData(vt_3332.vertexSize, v_cube.data(), 4*6, i_cube);

	std::vector<ShaderLoader> usedShaders{ 
		ShaderLoader("../../../projects/example_2/resources/shaders/GLSL/box_v.vert"),
		ShaderLoader("../../../projects/example_2/resources/shaders/GLSL/box_f.frag") };

	std::vector<TextureLoader> usedTextures{ 
		TextureLoader("../../../projects/example_2/resources/textures/bricks_albedo.png"),
		TextureLoader("../../../projects/example_2/resources/textures/bricks_normals.png"),
		TextureLoader("../../../projects/example_2/resources/textures/bricks_specular.png"),
		TextureLoader("../../../projects/example_2/resources/textures/bricks_roughness.png"),
		TextureLoader("../../../projects/example_2/resources/textures/bricks_height.png") };

	ModelDataInfo modelInfo;
	modelInfo.name = "cube";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_3332;
	modelInfo.verticesLoader = &vertexData;
	modelInfo.shadersInfo = &usedShaders;
	modelInfo.texturesInfo = &usedTextures;
	modelInfo.maxDescriptorsCount_vs = 1;
	modelInfo.maxDescriptorsCount_fs = 0;
	modelInfo.UBOsize_vs = 2 * size.mat4;		// Model matrix, Normal matrix
	modelInfo.UBOsize_fs = 0;
	modelInfo.globalUBO_vs = &rend.globalUBO_vs;
	modelInfo.globalUBO_fs = &rend.globalUBO_fs;
	modelInfo.transparency = true;
	modelInfo.renderPassIndex = 0;
	modelInfo.subpassIndex = 0;
	modelInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	cube = rend.newModel(modelInfo);
}

void updateCubeState(Renderer& rend)
{
	glm::mat4 modelMatrix  = getModelMatrix(glm::vec3(4, 4, 4), getRotQuat(glm::vec3(0, 0, 1), currentTime * 0.1), glm::vec3(0, 0, 0));
	glm::mat4 normalMatrix = getModelMatrixForNormals(modelMatrix);
	uint8_t* dest;

	for (int i = 0; i < cube->vsUBO.numActiveDescriptors; i++)
	{
		dest = cube->vsUBO.getDescriptorPtr(i);
		memcpy(dest, &modelMatrix, size.mat4);
		dest += size.mat4;
		memcpy(dest, &normalMatrix, size.mat4);
	}

	/*
	for (int i = 0; i < skybox->fsUBO.numActiveDescriptors; i++)
	{
		dest = skybox->fsUBO.getDescriptorPtr(i);
		// ...
	}
	*/
}

void createSkyBox(Renderer& rend)
{
	VerticesLoader vertexData(vt_32.vertexSize, v_skybox.data(), 6 * 4, i_skybox);

	std::vector<ShaderLoader> shaders{
		ShaderLoader("../../../projects/example_2/resources/shaders/GLSL/skybox_v.vert"),
		ShaderLoader("../../../projects/example_2/resources/shaders/GLSL/skybox_f.frag") };

	std::vector<TextureLoader> textures{
		TextureLoader("../../../projects/example_2/resources/textures/skybox/front.jpg",	VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		TextureLoader("../../../projects/example_2/resources/textures/skybox/back.jpg",		VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		TextureLoader("../../../projects/example_2/resources/textures/skybox/up.jpg",		VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		TextureLoader("../../../projects/example_2/resources/textures/skybox/down.jpg",		VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		TextureLoader("../../../projects/example_2/resources/textures/skybox/right.jpg",	VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT),
		TextureLoader("../../../projects/example_2/resources/textures/skybox/left.jpg",		VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT) };

	ModelDataInfo modelInfo;
	modelInfo.name = "skyBox";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.verticesLoader = &vertexData;
	modelInfo.shadersInfo = &shaders;
	modelInfo.texturesInfo = &textures;
	modelInfo.maxDescriptorsCount_vs = 1;
	modelInfo.maxDescriptorsCount_fs = 0;
	modelInfo.UBOsize_vs = 2 * size.mat4;		// Model matrix, Normal matrix
	modelInfo.UBOsize_fs = 0;
	modelInfo.globalUBO_vs = &rend.globalUBO_vs;
	modelInfo.globalUBO_fs = &rend.globalUBO_fs;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 2;
	modelInfo.subpassIndex = 0;
	modelInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	skybox = rend.newModel(modelInfo);
}

void updateSkyboxState(Renderer& rend)
{
	glm::mat4 modelMatrix = getModelMatrix(glm::vec3(10, 10, 10), noRotQuat, glm::vec3(0, 0, 0));
	glm::mat4 normalMatrix = getModelMatrixForNormals(modelMatrix);
	uint8_t* dest;

	for (int i = 0; i < skybox->vsUBO.numActiveDescriptors; i++)
	{
		dest = skybox->vsUBO.getDescriptorPtr(i);
		memcpy(dest, &modelMatrix, size.mat4);
		dest += size.mat4;
		memcpy(dest, &normalMatrix, size.mat4);
	}

	/*
	for (int i = 0; i < skybox->fsUBO.numActiveDescriptors; i++)
	{
		dest = skybox->fsUBO.getDescriptorPtr(i);
		// ...
	}
	*/
}

void setGlobalVariables(Renderer& rend)
{
	camPos = glm::vec3(-10, 10, 10);
	currentTime = rend.getTimer().getTime();
	deltaTime = 0;
	fov = 1.f;
	float aspectRatio = rend.getIO().getAspectRatio();

	view = getViewMatrix(camPos, -glm::normalize(camPos), glm::vec3(0, 0, 1));
	proj = getProjMatrix(fov, aspectRatio, 0.1, 1000);

	light.type = 1;
	light.direction = glm::normalize(glm::vec3(1, 0.6, -0.5));
	light.ambient = glm::vec3(0.1, 0.1, 0.1);
	light.diffuse = glm::vec3(1, 1, 1);
	light.specular = glm::vec3(1, 1, 1);
}

void updateGlobalVariables(Renderer& rend)
{
	// Time
	currentTime = rend.getTimer().getTime();
	deltaTime = rend.getTimer().getDeltaTime();

	// User input (left mouse button)
	if (rend.getIO().getMouseButton(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
	{
		rend.getIO().setInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		glm::dvec2 cursorPos2;
		rend.getIO().getCursorPos(&cursorPos2.x, &cursorPos2.y);
		cursorPos2 -= cursorPos;
		
		camPos = rotatePoint(
			productQuat(
				getRotQuat(glm::vec3(0, 0, 1), -cursorPos2.x * 0.005),
				getRotQuat(glm::normalize(glm::cross(-camPos, glm::vec3(0,0,1))), -cursorPos2.y * 0.005) ),
			camPos);

		view = getViewMatrix(camPos, -glm::normalize(camPos), glm::vec3(0, 0, 1));
	}
	else rend.getIO().setInputMode(GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	rend.getIO().getCursorPos(&cursorPos.x, &cursorPos.y);

	// User input (mouse scroll)
	float yScrollOffset = rend.getIO().getYscrollOffset();
	if (yScrollOffset)
	{
		fov -= yScrollOffset * 0.1;
		if (fov < 0.1) fov = 0.1;
		else if (fov > 1.0) fov = 1.0;
	}
	
	proj = getProjMatrix(fov, rend.getIO().getAspectRatio(), 0.1, 1000);
}

void updateGlobalUBOsState(Renderer& rend)
{
	uint8_t* dest;
	glm::vec4 camPos_time = glm::vec4(camPos.x, camPos.y, camPos.z, currentTime);

	for (int i = 0; i < rend.globalUBO_vs.numActiveDescriptors; i++)		// view, proj, camPos_time
	{
		dest = rend.globalUBO_vs.getDescriptorPtr(0);
		memcpy(dest, &view, size.mat4);
		dest += size.mat4;
		memcpy(dest, &proj, size.mat4);
		dest += size.mat4;
		memcpy(dest, &camPos_time, size.vec4);
	}

	for (int i = 0; i < rend.globalUBO_fs.numActiveDescriptors; i++)	    // camPos_time, light
	{
		dest = rend.globalUBO_fs.getDescriptorPtr(0);
		memcpy(dest, &camPos_time, size.vec4);
		dest += size.vec4;
		memcpy(dest, &light, sizeof(Light));
	}
}
