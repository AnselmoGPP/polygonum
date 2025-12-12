//#include <iostream>
//#include <array>

#include "polygonum/importer.hpp"
#include "polygonum/renderer.hpp"


//std::vector<Texture> noTextures;
//std::vector<uint16_t> noIndices;

// RESOURCES --------------------------------------------------------

ResourcesLoader::ResourcesLoader(VertexesLoader* vertexesLoader, std::vector<ShaderLoader*>& shadersInfo)
	: vertices(vertexesLoader), shaders(shadersInfo) { }

void ResourcesLoader::loadResources(ModelData& model, Renderer& rend)
{
	#ifdef DEBUG_RESOURCES
		std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
	#endif
	
	// Load vertexes and indices
	vertices->loadVertexes(model.vert, rend, model.bindSets);
	
	{
		const std::lock_guard<std::mutex> lock(rend.worker.mutResources);
		
		// Load shaders
		for (unsigned i = 0; i < shaders.size(); i++)
			model.shaders.push_back(shaders[i]->loadShader(rend.shaders, rend.c));
		
		// Load textures
		//for (unsigned i = 0; i < textures.size(); i++)
		//	model.binds.fsTextures.push_back(textures[i]->loadTexture(rend.textures, rend));
	}
}

