#include "renderer.hpp"
#include "toolkit.hpp"
#include "physics.hpp"
#include "ECSarch.hpp"

/* Example project: Basic triangle */

// Globals ----------

modelIter basicTriangle;
modelIter background;

// Prototypes ----------

void update(Renderer& rend);		// Callback that will be called for each frame (useful for updating model view matrices)
void createTriangle(Renderer& rend);
void createBackground(Renderer& rend);

// Definitions ----------

int main(int argc, char* argv[])
{
	IOmanager io(1920/2, 1080/2);

	Renderer renderer(update, io);
	renderer.getTimer().setMaxFPS(30);

	createTriangle(renderer);				// Forward pass
	createBackground(renderer);				// Forward pass
	renderer.createPostprocessingPass(		// Postprocessing pass
		std::string("../../../resources/shaders/postprocessing_v.vert"),
		std::string("../../../resources/shaders/postprocessing_f.frag"));

	renderer.renderLoop();					// Start render loop

	return 0;
}

void update(Renderer& rend)
{
	rend.updatePostprocessingPass();
}

void createTriangle(Renderer& rend)
{
	std::vector<float> v_triangle = {		// Vulkan NDCs (Vulkan Normalized Device Coordinates)
		-0.5f, 0.5f, 0.0f,   0.0f, 0.0f,
		 0.5f, 0.5f, 0.0f,   1.0f, 0.0f,
		 0.0f,-0.5f, 0.0f,   0.5f, 1.0f };
	std::vector<uint16_t> i_triangle = { 0, 1, 2 };

	VerticesLoader vertexData(vt_32.vertexSize, v_triangle.data(), 3, i_triangle);

	std::vector<ShaderLoader> shaders{
		ShaderLoader("../../../projects/example_1/resources/shaders/GLSL/basic_v.vert"),
		ShaderLoader("../../../projects/example_1/resources/shaders/GLSL/basic_f.frag") };

	std::vector<TextureLoader> textures{
		TextureLoader("../../../projects/example_1/resources/textures/bricks.png") };

	ModelDataInfo modelInfo;
	modelInfo.name = "triangle";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.verticesLoader = &vertexData;
	modelInfo.shadersInfo = &shaders;
	modelInfo.texturesInfo = &textures;
	modelInfo.maxDescriptorsCount_vs = 0;
	modelInfo.maxDescriptorsCount_fs = 0;
	modelInfo.UBOsize_vs = 0;
	modelInfo.UBOsize_fs = 0;
	modelInfo.globalUBO_vs = nullptr;
	modelInfo.globalUBO_fs = nullptr;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 2;
	modelInfo.subpassIndex = 0;
	modelInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	basicTriangle = rend.newModel(modelInfo);
}

void createBackground(Renderer& rend)
{
	std::vector<float> v_background = {		// Vulkan NDCs (Vulkan Normalized Device Coordinates)
	-1.f, 1.f, 0.1f,   0.f, 0.f,
	 1.f, 1.f, 0.1f,   1.f, 0.f,
	 1.f,-1.f, 0.1f,   1.f, 1.f,
	-1.f,-1.f, 0.1f,   0.f, 1.f };
	std::vector<uint16_t> i_background = { 0, 1, 2,  0, 2, 3 };

	VerticesLoader vertexData(vt_32.vertexSize, v_background.data(), 4, i_background);

	std::vector<ShaderLoader> shaders{
		ShaderLoader("../../../projects/example_1/resources/shaders/GLSL/basic_v.vert"),
		ShaderLoader("../../../projects/example_1/resources/shaders/GLSL/basic_f.frag") };

	std::vector<TextureLoader> textures{
		TextureLoader("../../../projects/example_1/resources/textures/wood.png") };

	ModelDataInfo modelInfo;
	modelInfo.name = "background";
	modelInfo.activeInstances = 1;
	modelInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	modelInfo.vertexType = vt_32;
	modelInfo.verticesLoader = &vertexData;
	modelInfo.shadersInfo = &shaders;
	modelInfo.texturesInfo = &textures;
	modelInfo.maxDescriptorsCount_vs = 0;
	modelInfo.maxDescriptorsCount_fs = 0;
	modelInfo.UBOsize_vs = 0;
	modelInfo.UBOsize_fs = 0;
	modelInfo.globalUBO_vs = nullptr;
	modelInfo.globalUBO_fs = nullptr;
	modelInfo.transparency = false;
	modelInfo.renderPassIndex = 2;
	modelInfo.subpassIndex = 0;
	modelInfo.cullMode = VK_CULL_MODE_BACK_BIT;

	background = rend.newModel(modelInfo);
}