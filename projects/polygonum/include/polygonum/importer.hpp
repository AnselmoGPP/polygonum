#ifndef IMPORTER_HPP
#define IMPORTER_HPP

//#include "polygonum/environment.hpp"
//#include "polygonum/ubo.hpp"
//#include "polygonum/renderer.hpp"
#include "polygonum/vertex.hpp"
#include "polygonum/shader.hpp"

//#include <unordered_map>

/*
	1. A VertexesLoader, ShaderLoader and TextureLoader objects are passed to our ModelData.
	2. ModelData uses it to create a ResourceLoader member.
	3. Later, in ModelData::fullConstruction(), ResourceLoader::loadResources() is used for loading resources.

	VertexesLoader has a VLModule* member. A children of VLModule is built, depending upon the VertexesLoader constructor used.
	ResourceLoader::loadResources()
		VertexesLoader::loadVertices()
			VLModule::loadVertices()
				getRawData() (polymorphic)
				createBuffers()
		ShaderLoader::loadShader()
		TextureLoader::loadTexture()
	
	ModelData
*/

// Forward declarations ----------

class Renderer;
class ModelData;

// Declarations ----------

//class VertexType;
//class VertexSet;
//struct VertexData;
//class VertexesLoader;
//	class VL_fromFile;
//	class VL_fromBuffer;

//class Shader;
//class SMod;
//class ShaderLoader;
//	class SL_fromFile;
//	class SL_fromBuffer;
//class ShaderIncluder;
//class ShaderCreator;

//class Texture;
//class TextureLoader;// <<< quit
//	class Tex_fromFile;
//	class Tex_fromBuffer;

//struct ResourcesLoader;
//
//class OpticalDepthTable;
//class DensityVector;

// Objects ----------

//typedef std::list<Shader >::iterator shaderIter;
//typedef std::list<Texture>::iterator texIter;

//extern std::vector<TextureLoader> noTextures;		//!< Vector with 0 TextureLoader objects
//extern std::vector<uint16_t   > noIndices;			//!< Vector with 0 indices

// Enums ----------


enum ShaderType { vertexS, fragS };


// RESOURCES --------------------------------------------------------

/// Encapsulates data required for loading resources (vertices, indices, shaders, textures) and loading methods.
struct ResourcesLoader
{
	ResourcesLoader(VertexesLoader* vertexesLoader, std::vector<ShaderLoader*>& shadersInfo);

	std::shared_ptr<VertexesLoader> vertices;
	std::vector<ShaderLoader*> shaders;
	//std::vector<TextureLoader*> textures;

	/// Get resources (vertices + indices, shaders, textures) from any source (file, buffer...) and upload them to Vulkan. If a shader or texture exists in Renderer, it just takes the iterator. As a result, `ModelData` can get the Vulkan buffers (`VertexData`, `shaderIter`s, `textureIter`s).
	void loadResources(ModelData& model, Renderer& rend);
};

#endif