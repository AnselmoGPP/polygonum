#include "polygonum/shader.hpp"
#include "polygonum/texture.hpp"

#include <iostream>
#include <sstream>

Shader::Shader(VulkanCore& c, const std::string id, VkShaderModule shaderModule)
	: c(c), id(id), shaderModule(shaderModule) {
}

Shader::~Shader()
{
#ifdef DEBUG_IMPORT
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	vkDestroyShaderModule(c.device, shaderModule, nullptr);
}

SMod::SMod(unsigned modificationType, std::initializer_list<std::string> params)
	: modificationType(modificationType), params(params) {
}

bool SMod::applyModification(std::string& shader)
{
	bool result = false;

	switch (modificationType)
	{
	case 1:   // - sm_albedo: (FS) Sampler used
		result = findTwoAndReplaceBetween(shader, "vec4 albedo", ";",
			"vec4 albedo = texture(tex[" + params[0] + "], inUVs)");
		break;

	case 2:   // sm_specular: (FS) Sampler used
		result = findTwoAndReplaceBetween(shader, "vec3 specular", ";",
			"vec3 specular = texture(tex[" + params[0] + "], inUVs).xyz");
		break;

	case 3:   // sm_roughness: (FS) Sampler used
		result = findTwoAndReplaceBetween(shader, "float roughness", ";",
			"float roughness = texture(tex[" + params[0] + "], inUVs).x");
		break;

	case 4:   // sm_normal: (VS, FS) Sampler used   <<< doesn't work yet
		result = findTwoAndReplaceBetween(shader, "vec3 normal", ";",
			"vec3 normal = planarNormal(tex[" + params[0] + "], inUVs, inTB, inNormal, 1)");
		for (int i = 0; i < 3; i++)
			if (!findStrAndErase(shader, "//normal: ")) break;
		findStrAndReplace(shader, "layout(location = 4) flat", "layout(location = 5) flat");
		break;

	case 5:   // - sm_discardAlpha: (FS) Discard fragments with low alpha
		result = findStrAndErase(shader, "//discardAlpha: ");
		break;

	case 6:   // sm_backfaceNormals: (VS) Recalculate normal based on backfacing
		result = findStrAndErase(shader, "//backfaceNormals: ");
		break;

	case 7:   // sm_sunfaceNormals: (VS) Recalculate normal based on light facing
		result = findStrAndErase(shader, "//sunfaceNormals: ");
		break;

	case 8:   // - sm_verticalNormals: (VS) Make all normals (before MVP transformation) vertical (vec3(0,0,1))
		result = findStrAndErase(shader, "outNormal = mat3(lUbo.ins[i].normalMat) * inNormal;");
		findStrAndErase(shader, "//verticalNormals: ");
		break;

	case 9:   // - sm_waving: (VS) Make mesh wave (wind)
		result = findStrAndErase(shader, "//waving: ");
		findStrAndReplace(shader, "<speed>", params[0]);
		findStrAndReplace(shader, "<amplitude>", params[1]);
		findStrAndReplace(shader, "<minHeight>", params[2]);
		findStrAndReplace(shader, "<minHeight>", params[2]);
		break;

	case 10:   // - sm_distDithering: (FS) Apply dithering to distant fragments
		result = findStrAndErase(shader, "//distDithering: ");
		findStrAndReplace(shader, "<near>", params[0]);
		findStrAndReplace(shader, "<far>", params[1]);
		break;

	case 11:   // - sm_earlyDepthTest: (FS) Apply Early Depth/Fragment Test
		result = findStrAndErase(shader, "//earlyDepthTest: ");
		break;

	case 12:   // - sm_dryColor: (FS) Apply dry color filter to albedo depending on fragment height
		result = findStrAndErase(shader, "//dryColor: ");
		findStrAndReplace(shader, "<dryColor>", params[0]);
		findStrAndReplace(shader, "<minHeight>", params[1]);
		findStrAndReplace(shader, "<maxHeight>", params[2]);
		break;

	case 13:   // sm_changeHeader: Change header (#include header_path)
		result = findStrAndReplaceLine(shader, "#include", "#include \"" + params[0] + '\"');
		break;

	case 0:   // sm_none
	default:
		break;
	}

	return result;
}

SMod SMod::none() { return SMod(0); }
SMod SMod::albedo(std::string index) { return SMod(1, { index }); }
SMod SMod::specular(std::string index) { return SMod(2, { index }); }
SMod SMod::roughness(std::string index) { return SMod(3, { index }); }
SMod SMod::normal() { return SMod(4); }
SMod SMod::discardAlpha() { return SMod(5); }
SMod SMod::backfaceNormals() { return SMod(6); }
SMod SMod::sunfaceNormals() { return SMod(7); }
SMod SMod::verticalNormals() { return SMod(8); }
SMod SMod::wave(std::string speed, std::string amplitude, std::string minHeight) { return SMod(9, { speed, amplitude, minHeight }); }
SMod SMod::distDithering(std::string near, std::string far) { return SMod(10, { near, far }); }
SMod SMod::earlyDepthTest() { return SMod(11); }
SMod SMod::dryColor(std::string color, std::string minHeight, std::string maxHeight) { return SMod(12, { color, minHeight, maxHeight }); }
SMod SMod::changeHeader(std::string path) { return SMod(13, { path }); }

unsigned SMod::getModType() { return modificationType; }

std::vector<std::string> SMod::getParams() { return params; }

bool SMod::findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement)
{
	size_t pos1 = text.find(str1, 0);
	size_t pos2 = text.find(str2, pos1);
	if (pos1 == text.npos || pos2 == text.npos) return false;

	text.replace(pos1, pos2 - pos1, (replacement));
	return true;
}

bool SMod::findStrAndErase(std::string& text, const std::string& str)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.erase(pos, str.size());
	return true;
}

bool SMod::findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.replace(text.begin() + pos, text.begin() + pos + str.size(), replacement);
	return true;
}

bool SMod::findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	size_t eol = text.find('\n', pos) - 1;
	if (eol == text.npos) return false;
	eol++;	// <<< why is this needed? Otherwise, something in the text is messed up (#line)

	text.replace(text.begin() + pos, text.begin() + eol, replacement);
	return true;
}

ShaderLoader::ShaderLoader(const std::string& id, const std::initializer_list<SMod>& modifications)
	: id(id), mods(modifications)
{
	if (mods.size())	// if there're modifiers, id has to change. Otherwise, it's possible that 2 different shaders have same name when the original shader is the same.
		for (SMod mod : mods)
		{
			this->id += "_" + std::to_string((int)mod.getModType());

			for (const std::string& str : mod.getParams())
				this->id += "_" + str;
		}
}

std::shared_ptr<Shader> ShaderLoader::loadShader(PointersManager<std::string, Shader>& loadedShaders, VulkanCore& c)
{
#ifdef DEBUG_RESOURCES
	std::cout << typeid(*this).name() << "::" << __func__ << ": " << this->id << std::endl;
#endif

	// Look for it in loadedShaders
	if (loadedShaders.contains(id))
		return loadedShaders.get(id);

	// Load shader (if not loaded yet)
	std::string glslData;
	getRawData(glslData);

	// Make some changes to the shader string.
	if (mods.size()) applyModifications(glslData);

	// Compile data (preprocessing > compilation):
	shaderc::CompileOptions options;
	options.SetIncluder(std::make_unique<ShaderIncluder>());
	options.SetGenerateDebugInfo();
	//if (optimize) options.SetOptimizationLevel(shaderc_optimization_level_performance);	// This option makes shaderc::CompileGlslToSpv fail when Assimp::Importer is present in code, even if an Importer object is not created (odd) (Importer is in DataFromFile2::loadVertex).

	shaderc::Compiler compiler;

	shaderc::PreprocessedSourceCompilationResult preProcessed = compiler.PreprocessGlsl(glslData.data(), glslData.size(), shaderc_glsl_infer_from_source, id.c_str(), options);
	if (preProcessed.GetCompilationStatus() != shaderc_compilation_status_success)
		std::cerr << "Shader module preprocessing failed - " << preProcessed.GetErrorMessage() << std::endl;

	std::string ppData(preProcessed.begin());
	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(ppData.data(), ppData.size(), shaderc_glsl_infer_from_source, id.c_str(), options);

	if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		std::cerr << "Shader module compilation failed - " << module.GetErrorMessage() << std::endl;

	std::vector<uint32_t> spirv = { module.cbegin(), module.cend() };

	//Create shader module:

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = spirv.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(spirv.data());	// The default allocator from std::vector ensures that the data satisfies the alignment requirements of `uint32_t`.

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(c.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("Failed to create shader module!");

	// Create and save shader object
	return loadedShaders.emplace(id, c, id, shaderModule);
}

void ShaderLoader::applyModifications(std::string& shader)
{
	for (auto& mod : mods)
		mod.applyModification(shader);
}

SL_fromBuffer::SL_fromBuffer(const std::string& id, const std::string& glslText, const std::initializer_list<SMod>& modifications)
	: ShaderLoader(id, modifications), data(glslText) {
}

ShaderLoader* SL_fromBuffer::clone() { return new SL_fromBuffer(*this); }

void SL_fromBuffer::getRawData(std::string& glslData) { glslData = data; }

SL_fromBuffer* SL_fromBuffer::factory(std::string id, const std::string& glslText, std::initializer_list<SMod> modifications)
{
	return new SL_fromBuffer(id, glslText, modifications);
}

SL_fromFile::SL_fromFile(const std::string& filePath, std::initializer_list<SMod>& modifications)
	: ShaderLoader(filePath, modifications), filePath(filePath) {
};

ShaderLoader* SL_fromFile::clone() { return new SL_fromFile(*this); }

void SL_fromFile::getRawData(std::string& glslData) { readFile(filePath.c_str(), glslData); }

SL_fromFile* SL_fromFile::factory(std::string filePath, std::initializer_list<SMod> modifications)
{
	return new SL_fromFile(filePath, modifications);
}

shaderc_include_result* ShaderIncluder::GetInclude(const char* sourceName, shaderc_include_type type, const char* destName, size_t includeDepth)
{
	auto container = new std::array<std::string, 2>;
	(*container)[0] = std::string(sourceName);
	readFile(sourceName, (*container)[1]);

	auto data = new shaderc_include_result;
	data->user_data = container;
	data->source_name = (*container)[0].data();
	data->source_name_length = (*container)[0].size();
	data->content = (*container)[1].data();
	data->content_length = (*container)[1].size();

	return data;
}

void ShaderIncluder::ReleaseInclude(shaderc_include_result* data)
{
	delete static_cast<std::array<std::string, 2>*>(data->user_data);
	delete data;
}

BindingBufferType ShaderCreator::getDescType(const BindingBuffer* bindBuffer)
{
	switch (bindBuffer->type)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		return ubo;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		return ssbo;
	default:
		return undef;
	}
}

std::string ShaderCreator::getBuffer(const BindingBuffer& buffer, unsigned bindingNumber, unsigned nameNumber, bool isGlobal)
{
	std::string str;
	BindingBufferType descType = getDescType(&buffer);
	std::string bufferType = descType == ubo ? "uniform" : (descType == ssbo ? "buffer" : "XXX");
	std::string structName = isGlobal ? "GlobalBuffer" : "LocalBuffer";
	std::string objName = isGlobal ? "gBuf" : "lBuf";
	if (nameNumber) {
		structName += std::to_string(nameNumber);
		objName += std::to_string(nameNumber);
	}
	if (buffer.numDescriptors > 1) objName += "[" + std::to_string(buffer.numDescriptors) + "]";

	str += "layout(set = 0, binding = " + std::to_string(bindingNumber) + ") " + bufferType + " " + structName + " {\n";
	for (const auto& descAttrib : buffer.glslLines)
		str += "\t" + descAttrib + ";\n";
	str += "} " + objName + ";\n\n";

	return str;
}

std::unordered_map<VertAttrib, unsigned> ShaderCreator::usedAttribTypes(const VertexType& vertexType)
{
	std::unordered_map<VertAttrib, unsigned> typesCount;

	for (const auto& type : vertexType.attribsTypes)
		typesCount[type]++;

	return typesCount;
}

/*
/// Example of how to use stb_image.h for loading OBJ files
void DataFromFile::loadVertex()
{
	// Load model
	tinyobj::attrib_t					 attrib;			// Holds all of the positions, normals and texture coordinates.
	std::vector<tinyobj::shape_t>		 shapes;			// Holds all of the separate objects and their faces. Each face consists of an array of vertices. Each vertex contains the indices of the position, normal and texture coordinate attributes.
	std::vector<tinyobj::material_t>	 materials;			// OBJ models can also define a material and texture per face, but we will ignore those.
	std::string							 warn, err;			// Errors and warnings that occur while loading the file.

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath))
		throw std::runtime_error(warn + err);

	// Combine all the faces in the file into a single model
	std::unordered_map<VertexPCT, uint32_t> uniqueVertices{};	// Keeps track of the unique vertices and the respective indices, avoiding duplicated vertices (not indices).

	for (const auto& shape : shapes)
		for (const auto& index : shape.mesh.indices)
		{
			// Get each vertex
			VertexPCT vertex{};	// <<< out of the loop better?

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],			// attrib.vertices is an array of floats, so we need to multiply the index by 3 and add offsets for accessing XYZ components.
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
					   attrib.texcoords[2 * index.texcoord_index + 0],	// attrib.texcoords is an array of floats, so we need to multiply the index by 3 and add offsets for accessing UV components.
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]	// Flip vertical component of texture coordinates: OBJ format assumes Y axis go up, but Vulkan has top-to-bottom orientation.
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			// Check if we have already seen this vertex. If not, assign an index to it and save the vertex.
			if (uniqueVertices.count(vertex) == 0)			// Using a user-defined type (Vertex struct) as key in a hash table requires us to implement two functions: equality test (override operator ==) and hash calculation (implement a hash function for Vertex).
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(destVertices->size());	// Set new index for this vertex
				destVertices->push_back(&vertex);										// Save vertex
			}

			// Save the index
			destIndices->push_back(uniqueVertices[vertex]);								// Save index
		}
}
*/

ShaderCreator::ShaderCreator(RPtype rendPass, const VertexType& vertexType, const BindingSet& bindings)
	: rpType(rendPass)
{
	setBasics();
	setBindings(bindings);

	switch (rendPass)
	{
	case forward:
		setForward(vertexType, bindings);
		break;
	case geometry:
		setGeometry(vertexType, bindings);
		break;
	case lighting:
		setLighting();
		break;
	case postprocessing:
		setPostprocess();
		break;
	default:
		std::cout << "ShaderCreator: Invalid pass selected" << std::endl;
		break;
	}
}

void ShaderCreator::setBasics()
{
	vs.header = {
	"#version 450",
	"#extension GL_ARB_separate_shader_objects : enable",
	"#pragma shader_stage(vertex)" };

	vs.includes = { "#include \"..\\..\\extern\\polygonum\\resources\\shaders\\vertexTools.vert\"" };

	vs.globals = { "int i = gl_InstanceIndex" };

	fs.header = {
		"#version 450",
		"#extension GL_ARB_separate_shader_objects : enable",
		"#pragma shader_stage(fragment)" };

	fs.includes = { "#include \"..\\..\\extern\\polygonum\\resources\\shaders\\fragTools.vert\"" };

	fs.flags = { "layout(early_fragment_tests) in" };
}

void ShaderCreator::setBindings(const BindingSet& bindings)
{
	for (const auto& buf : bindings.vsGlobal)
		vs.bind_globalBuffers.push_back(BindingBuffer(getDescType(buf), buf->numDescriptors, buf->numSubDescriptors, buf->descriptorSize, buf->glslLines));

	for (const auto& buf : bindings.fsGlobal)
		fs.bind_globalBuffers.push_back(BindingBuffer(getDescType(buf), buf->numDescriptors, buf->numSubDescriptors, buf->descriptorSize, buf->glslLines));

	vs.bind_localBuffers = bindings.vsLocal;

	fs.bind_localBuffers = bindings.fsLocal;

	for (const auto& texSet : bindings.vsTextures)
		vs.bind_textures.push_back(texSet.size());

	for (const auto& texSet : bindings.fsTextures)
		fs.bind_textures.push_back(texSet.size());
}

ShaderCreator& ShaderCreator::setForward()
{
	setVS();
	setFS_forward();

	return *this;
}

ShaderCreator& ShaderCreator::setGeometry()
{
	setVS();
	setFS_geometry();

	return *this;
}

void ShaderCreator::setLighting()
{
	// Vertex shader
	vs.includes.clear();
	vs.input = {
		"in vec3 inPos",   // NDC position. Since it's in NDCs, no MVP transformation is required
		"in vec2 inUV"
	};
	vs.output = {
		"out vec3 outUV"
	};
	vs.main_end = {
		"gl_Position = vec4(inPos, 1.0f)",
		"outUVs = inUVs"
	};

	// Fragment shader
	fs.flags.clear();
	fs.bind_globalBuffers = { BindingBuffer(ubo, 1, 1, 0, {"vec4 camPos_t", "Light lights[NUMLIGHTS]"}) };
	fs.bind_textures = { 4 };   // pos, albedo, normal specRough (sampler2D for single-sample | sampler2DMS for multisampling)
	fs.input = { "in vec2 inUV" };
	fs.output = { "out vec4 outColor" };
	fs.globals = {
		"vec4 showPositions(float divider) { return vec4(texture(inputAtt[0], inUV).xyz / divider, 1.0); }",
		"vec4 showAlbedo() { return vec4(texture(inputAtt[1], inUV).xyz, 1.0); }",
		"vec4 showNormals() { return vec4(texture(inputAtt[2], inUV).xyz, 1.0); }",
		"vec4 showSpecularity() { return vec4(texture(inputAtt[3], inUV).xyz, 1.0); }",
		"vec4 showRoughness() { return vec4(vec3(texture(inputAtt[3], inUV).w), 1.0); }"
	};
	fs.main_begin = {
		"vec3 fragPos = texture(inputAtt[0], inUVs).xyz",
		"vec3 albedo = texture(inputAtt[1], inUVs).xyz",
		"vec3 normal = texture(inputAtt[2], inUVs, 1).xyz",   //unpackNormal(texture(inputAttachments[2], unpackUV(inUVs, 1)).xyz);	//unpackNormal(texture(inputAttachments[2], unpackUV(inUVs, 1)).xyz);
		"vec4 specRough = texture(inputAtt[3], inUVs)"
	};
	fs.main_processing = {
		"//outColor = showPositions(5000); return",
		"//outColor = showAlbedo(); return",
		"//outColor = showNormals(); return",
		"//outColor = showSpecularity(); return",
		"//outColor = showRoughness(); return"
	};
	fs.main_end = {
		"outColor = getFragColor(albedo, normal, specRough.xyz, specRough.w * 255, ubo.lights, fragPos, ubo.camPos.xyz)"
	};
}

void ShaderCreator::setPostprocess()
{
	// Vertex shader
	vs.includes.clear();
	vs.input = {
		"in vec3 inPos",   // NDC position. Since it's in NDCs, no MVP transformation is required.
		"in vec2 inUV"
	};
	vs.output = { "out vec2 outUV" };
	vs.main_end = {
		"gl_Position = vec4(inPos, 1.0f)",
		"outUV = inUV"
	};

	// Fragment shader
	fs.bind_textures = { 2 };   // Color (sampler2D for single-sample | sampler2DMS for multisampling)
	fs.input = { "in vec2 inUV" };
	fs.output = { "out vec4 outColor" };
	fs.main_end = { "outColor = vec4(texture(inputAtt[0], inUV).rgb, 1.0)" };
}

void ShaderCreator::setVS()
{
	vs.header = {
		"#version 450",
		"#extension GL_ARB_separate_shader_objects : enable",
		"#pragma shader_stage(vertex)" };

	vs.includes = { "#include \"..\..\extern\polygonum\resources\shaders\vertexTools.vert\"" };

	vs.flags;

	vs.bind_globalBuffers = { BindingBuffer(ubo, 1, 1, 0, { "mat4 view", "mat4 proj", "vec4 camPos_t" }) };
	vs.bind_localBuffers = { BindingBuffer(ubo, 1, 1, 0, {"mat4 model", "mat4 normalMat" }) };

	vs.input = { "vec3 inPos", "vec2 inUV", "vec3 inNormal", "vec3 inTan" };

	vs.output = { "vec3 outPos", "vec2 outUV", "vec3 outNormal", "TB outTB" };

	vs.globals = { "int i = gl_InstanceIndex" };

	vs.main_begin = {
		"vec3 worldPos = (lBuf.ins[i].model * vec4(inPos, 1.0)).xyz",
		"vec4 clipPos = gBuf.proj * gBuf.view * vec4(worldPos, 1.0)",
		"vec2 uv = inUV",
		"vec3 normal = mat3(lBuf.ins[i].normalMat) * inNormal",
		"TB tb = getTB(inNormal, inTan)" };

	vs.main_processing;

	vs.main_end = {
		"gl_Position = clipPos",
		"outPos = worldPos",
		"outUV = uv",
		"outNormal = normal",
		"outTB = tb" };
}

void ShaderCreator::setFS_forward()
{
	fs.header = {
		"#version 450",
		"#extension GL_ARB_separate_shader_objects : enable",
		"#pragma shader_stage(fragment)" };

	fs.includes = { "#include \"..\..\extern\polygonum\resources\shaders\fragTools.vert\"" };

	fs.flags = { "layout(early_fragment_tests) in" };

	fs.bind_globalBuffers = { BindingBuffer(ubo, 1, 1, 0, { "vec4 camPos_t", "Light light[]" }) };
	fs.bind_textures = { 4 };   // albedo, spec, rough, normal

	fs.input = { "vec3 inPos", "vec2 inUV", "vec3 inNormal", "TB inTB" };

	fs.output = { "vec4 outColor" };

	fs.globals;

	fs.main_begin = {
		"vec3 albedo = texture(texSampler[0], inUV).xyz",
		"vec4 specRough = texture(texSampler[1], inUV)",
		"vec3 normal = planarNormal(texSampler[2], inNormal, inTB, inUV, 1.f)" };

	fs.main_processing;

	fs.main_end = {
		"outColor = getFragColor(albedo, normal, specRough.xyz, specRough.w * 255, gBuf.light, inPos, gBuf.camPos_t.xyz)" };
}

void ShaderCreator::setFS_geometry()
{
	fs.header = {
		"#version 450",
		"#extension GL_ARB_separate_shader_objects : enable",
		"#pragma shader_stage(fragment)" };

	fs.includes = { "#include \"..\..\extern\polygonum\resources\shaders\fragTools.vert\"" };

	fs.flags = { "layout(early_fragment_tests) in" };

	fs.bind_globalBuffers = { BindingBuffer(ubo, 1, 1, 0, { "vec4 camPos_t", "Light light[NUMLIGHTS]" }) };
	fs.bind_textures = { 4 };   // albedo, spec, rough, normal

	fs.input = { "vec3 inPos", "vec2 inUV", "vec3 inNormal", "TB inTB" };

	fs.output = { "vec4 outPos", "vec4 outAlbedo", "vec4 outSpecRoug", "vec4 outNormal" };

	fs.globals;

	fs.main_begin = {
		"vec4 albedo = vec4(texture(texSampler[0], inUV).xyz, 1.f)",
		"vec4 specRough = texture(texSampler[1], inUV)",
		"vec3 normal = planarNormal(texSampler[2], inNormal, inTB, inUV, 1.f)" };

	fs.main_processing;

	fs.main_end = {
		"outPos = vec4(inPos, 1.f)",
		"outAlbedo = albedo",
		"outSpecRoug = vec4(specular, roughness)",
		"outNormal = vec4(normalize(inNormal), 1.0)" };
}

void ShaderCreator::setVS_general(const VertexType& vertexType)
{
	for (auto attrib : vertexType.attribsTypes)
		std::cout << attrib << ", ";
	std::cout << std::endl;

	for (auto attrib : vertexType.attribsTypes)
		switch (attrib)
		{
		case vaPos:
			vs.input.push_back("in vec3 inPos");
			vs.output.push_back("out vec3 outPos");
			vs.main_begin.push_back("vec3 worldPos = (lBuf.ins[i].model * vec4(inPos, 1.0)).xyz");
			vs.main_begin.push_back("vec4 clipPos = gBuf.proj * gBuf.view * vec4(worldPos, 1.0)");
			vs.main_end.push_back("gl_Position = clipPos");
			vs.main_end.push_back("outPos = worldPos");
			fs.input.push_back("in vec3 inPos");
			continue;
		case vaNorm:
			vs.input.push_back("in vec3 inNormal");
			vs.output.push_back("out vec3 outNormal");
			vs.main_begin.push_back("vec3 normal = mat3(lBuf.ins[i].normalMat) * inNormal");
			vs.main_end.push_back("outNormal = normal");
			fs.input.push_back("in vec3 inNormal");
			continue;
		case vaTan:
			vs.input.push_back("in vec3 inTan");
			vs.output.push_back("out TB outTB");
			vs.main_begin.push_back("TB tb = getTB(inNormal, inTan)");
			vs.main_end.push_back("outTB = tb");
			fs.input.push_back("in TB inTB");
			continue;
		case vaCol:
			vs.input.push_back("in vec4 inColor");
			vs.output.push_back("out vec4 outColor");
			vs.main_begin.push_back("vec4 color = inColor");
			vs.main_end.push_back("outColor = color");
			fs.input.push_back("in vec4 inColor");
			continue;
		case vaCol4:
			vs.input.push_back("in u8vec4 inColor");
			vs.output.push_back("out u8vec4 outColor");
			vs.main_begin.push_back("u8vec4 color = inColor");
			vs.main_end.push_back("outColor = color");
			fs.input.push_back("in u8vec4 inColor");
			continue;
		case vaUv:
			vs.input.push_back("in vec2 inUV");
			vs.output.push_back("out vec2 outUV");
			vs.main_begin.push_back("vec2 uv = inUV");
			vs.main_end.push_back("outUV = uv");
			fs.input.push_back("in vec2 inUV");
			continue;
		case vaFixes:
			vs.input.push_back("in vec3 inFixes");
			vs.output.push_back("out vec3 outFixes");
			continue;
		case vaBoneWeights:
			vs.input.push_back("in vec4 inBoneWeights");
			vs.output.push_back("out vec4 outBoneWeights");
			vs.main_begin.push_back("vec4 boneWeights = inBoneWeights");
			vs.main_end.push_back("outBoneWeights = boneWeights");
			fs.input.push_back("in vec4 inBoneWeights");
			continue;
		case vaBoneIndices:
			vs.input.push_back("in uvec4 inBoneIndices");
			vs.output.push_back("out uvec4 outBoneIndices");
			vs.main_begin.push_back("uvec4 boneIndices = inBoneIndices");
			vs.main_end.push_back("outBoneIndices = boneIndices");
			fs.input.push_back("in uvec4 inBoneIndices");
			continue;
			//case vaInstanceTransform:
			//	continue;
		default:
			throw std::runtime_error("Attribute not mapped in ShaderCreator");
		}
}

void ShaderCreator::setForward(const VertexType& vertexType, const BindingSet& bindings)
{
	std::unordered_map<VertAttrib, unsigned> attribs = usedAttribTypes(vertexType);
	//std::unordered_map<TexType, unsigned> tex = usedTextureTypes(textures);

	setVS_general(vertexType);

	fs.output.push_back("out vec4 outColor");

	fs.main_begin.push_back("vec3 worldPos = inPos");
	fs.main_begin.push_back("vec3 albedo = texture(tex[0], inUV).xyz");
	fs.main_begin.push_back("vec4 specRough = vec4(texture(tex[1], inUV).xyz, texture(tex[2], inUV).x)");
	fs.main_begin.push_back("vec3 normal = planarNormal(tex[3], inNormal, inTB, inUV, 1.f)");

	fs.main_end.push_back("outColor = getFragColor(albedo, normal, specRough.xyz, specRough.w * 255, gBuf[0].light, worldPos, gBuf[0].camPos_t.xyz)");

	if (attribs.find(vaTan) == attribs.end())
		fs.main_begin[3] = "vec3 normal = inNormal";
}

void ShaderCreator::setGeometry(const VertexType& vertexType, const BindingSet& bindings)
{
	setVS_general(vertexType);

	fs.output.push_back("out vec4 outPos");
	fs.output.push_back("out vec4 outAlbedo");
	fs.output.push_back("out vec4 outNormal");
	fs.output.push_back("out vec4 outSpecRoug");

	fs.main_begin.push_back("vec3 worldPos = inPos");
	fs.main_begin.push_back("vec4 albedo = vec4(1.f, 0.1f, 0.1f, 1.f)");
	fs.main_begin.push_back("vec3 specularity = vec3(0.f, 0.f, 0.f)");
	fs.main_begin.push_back("float roughness = 0.f");
	fs.main_begin.push_back("vec3 normal = inNormal;");
	
	for (unsigned i = 0; i < bindings.fsTextures.size(); i++)
		switch (bindings.fsTextures[0][i]->type)
		{
		case tAlb:
			fs.main_begin[1] = "vec4 albedo = vec4(texture(tex[" + std::to_string(i) + "], inUV).xyz, 1.f)";
			continue;
		case tSpec:
			fs.main_begin[2] = "vec3 specularity = texture(tex[" + std::to_string(i) + "], inUV).xyz";
			continue;
		case tRoug:
			fs.main_begin[3] = "float roughness = texture(tex[" + std::to_string(i) + "], inUV).x";
			continue;
		case tSpecroug:
			fs.main_begin[2] = "vec4 specRough = texture(tex[" + std::to_string(i) + "], inUV)\n";
			fs.main_begin[2] += "\tvec3 specularity = specRough.xyz";
			fs.main_begin[3] = "float roughness = specRough.w";
			continue;
		case tNorm:
			fs.main_begin[4] = "vec3 normal = inNormal";
			for (const auto& type : vertexType.attribsTypes)
				if (type == vaTan)
					fs.main_begin[4] = "vec3 normal = planarNormal(tex[" + std::to_string(i) + "], inNormal, inTB, inUV, 1.f)";
			continue;
		default:
			continue;
		}

	fs.main_end.push_back("outPos = vec4(worldPos, 1.f)");
	fs.main_end.push_back("outAlbedo = albedo");
	fs.main_end.push_back("outSpecRoug = vec4(specularity, roughness)");
	fs.main_end.push_back("outNormal = vec4(normalize(normal), 1.0)");
}

unsigned ShaderCreator::firstBindingNumber(unsigned shaderType)
{
	if (shaderType == 1)   // if in fragment shader
		return vs.bind_globalBuffers.size() + vs.bind_localBuffers.size() + vs.bind_textures.size();

	return 0;
}


ShaderCreator& ShaderCreator::replaceMainBegin(unsigned shaderType, std::string& text, const std::string& substring, const std::string& replacement)
{
	ShaderCode& shader = shaderType ? fs : vs;

	for (auto& line : shader.main_begin)
		if (!replaceAllIfContains(text, substring, replacement))
			std::cout << "Substring not found" << std::endl;

	return *this;
}

ShaderCreator& ShaderCreator::replaceMainEnd(unsigned shaderType, std::string& text, const std::string& substring, const std::string& replacement)
{
	ShaderCode& shader = shaderType ? fs : vs;

	for (auto& line : shader.main_end)
		if (!replaceAllIfContains(text, substring, replacement))
			std::cout << "Substring not found" << std::endl;

	return *this;
}

ShaderCreator& ShaderCreator::setVerticalNormals()
{
	for (auto& line : vs.main_begin)
		if (findStrAndReplace(line,
			"\tvec3 normal = mat3(ubo[i].normalMat) * inNormal; \n",
			"\tvec3 normal = mat3(ubo[i].normalMat) * vec3(0,0,1) \n"))
			break;

	return *this;
}

std::string ShaderCreator::getShader(unsigned shaderType)
{
	ShaderCode& code = (shaderType ? fs : vs);

	std::ostringstream shader;

	// Header

	for (auto& line : code.header)
		shader << line << "\n";

	if (code.header.size()) shader << "\n";

	// Includes

	for (auto& line : code.includes)
		shader << line << "\n";

	if (code.includes.size()) shader << "\n";

	// Flags

	for (auto& line : code.flags)
		shader << line << ";\n";

	if (code.flags.size()) shader << "\n";

	// Structs

	for (auto& line : code.structs)
		shader << line << ";\n";

	if (code.structs.size()) shader << "\n";

	// Bindings

	unsigned bindingNumber = firstBindingNumber(shaderType);

	//   - Global buffers

	for (unsigned i = 0; i < code.bind_globalBuffers.size(); i++)
		shader << getBuffer(code.bind_globalBuffers[i], bindingNumber++, i, true);

	//   - Local buffers

	for (unsigned i = 0; i < code.bind_localBuffers.size(); i++)
		shader << getBuffer(code.bind_localBuffers[i], bindingNumber++, i, false);

	//   - Textures

	for (unsigned i = 0; i < code.bind_textures.size(); i++)
	{
		shader << "layout(set = 0, binding = " << std::to_string(bindingNumber++) << ") uniform sampler2D tex";
		if (i) shader << std::to_string(i);
		shader << "[" << std::to_string(code.bind_textures[i]) << "];\n\n";
	}

	// Input

	for (unsigned i = 0; i < code.input.size(); i++)
		shader << "layout(location = " << std::to_string(i) << ") " << code.input[i] << ";\n";

	if (code.input.size()) shader << "\n";

	// Output

	for (unsigned i = 0; i < code.output.size(); i++)
		shader << "layout(location = " << std::to_string(i) << ") " << code.output[i] << ";\n";

	if (code.output.size()) shader << "\n";

	// Globals

	for (const auto& line : code.globals)
		shader << line << ";\n";

	if (code.globals.size()) shader << "\n";

	// main

	shader << "void main()\n{\n";

	for (const auto& line : code.main_begin)
		shader << "\t" << line << ";\n";

	if (code.main_begin.size()) shader << "\n";

	for (const auto& line : code.main_processing)
		shader << "\t" << line << ";\n";

	if (code.main_processing.size()) shader << "\n";

	for (const auto& line : code.main_end)
		shader << "\t" << line << ";\n";

	shader << "}\n\n";

	// Others

	for (const auto& line : code.others)
		shader << line << "\n\n";

	return shader.str();
}

std::string ShaderCreator::getShader0(unsigned shaderType)
{
	ShaderCode& code = (shaderType ? fs : vs);

	std::string shader;

	// Header

	for (auto& line : code.header)
		shader += line + "\n";

	if (code.header.size()) shader += "\n";

	// Includes

	for (auto& line : code.includes)
		shader += line + "\n";

	if (code.includes.size()) shader += "\n";

	// Flags

	for (auto& line : code.flags)
		shader += line + ";\n";

	if (code.flags.size()) shader += "\n";

	// Structs

	for (auto& line : code.structs)
		shader += line + ";\n";

	if (code.structs.size()) shader += "\n";

	// Bindings

	unsigned bindingNumber = firstBindingNumber(shaderType);

	//   - Global buffers

	for (unsigned i = 0; i < code.bind_globalBuffers.size(); i++)
		shader += getBuffer(code.bind_globalBuffers[i], bindingNumber++, i, true);

	//   - Local buffers

	for (unsigned i = 0; i < code.bind_localBuffers.size(); i++)
		shader += getBuffer(code.bind_localBuffers[i], bindingNumber++, i, false);

	//   - Textures

	for (unsigned i = 0; i < code.bind_textures.size(); i++)
	{
		shader += "layout(set = 0, binding = " + std::to_string(bindingNumber++) + ") uniform sampler2D tex";
		if (i) shader += std::to_string(i);
		shader += "[" + std::to_string(code.bind_textures[i]) + "];\n\n";
	}

	// Input

	for (unsigned i = 0; i < code.input.size(); i++)
		shader += "layout(location = " + std::to_string(i) + ") " + code.input[i] + ";\n";

	if (code.input.size()) shader += "\n";

	// Output

	for (unsigned i = 0; i < code.output.size(); i++)
		shader += "layout(location = " + std::to_string(i) + ") " + code.output[i] + ";\n";

	if (code.output.size()) shader += "\n";

	// Globals

	for (const auto& line : code.globals)
		shader += line + ";\n";

	if (code.globals.size()) shader += "\n";

	// main

	shader += "void main()\n{\n";

	for (const auto& line : code.main_begin)
		shader += "\t" + line + ";\n";

	if (code.main_begin.size()) shader += "\n";

	for (const auto& line : code.main_processing)
		shader += "\t" + line + ";\n";

	if (code.main_processing.size()) shader += "\n";

	for (const auto& line : code.main_end)
		shader += "\t" + line + ";\n";

	shader += "}\n\n";

	// Others

	for (const auto& line : code.others)
		shader += line + "\n\n";

	return shader;
}

void ShaderCreator::printShader(unsigned shaderType) { std::cout << getShader(shaderType) << std::endl; }

void ShaderCreator::printAllShaders()
{
	std::cout << "----------\n";
	std::cout << getShader(0) << "\n";
	std::cout << "----------\n";
	std::cout << getShader(1) << "\n";
}

bool ShaderCreator::replaceAllIfContains(std::string& text, const std::string& substring, const std::string& replacement)
{
	if (text.find(substring) != std::string::npos)
	{
		text = replacement;
		return true;
	}
	else return false;
}

bool ShaderCreator::findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement)
{
	size_t pos1 = text.find(str1, 0);
	size_t pos2 = text.find(str2, pos1);
	if (pos1 == text.npos || pos2 == text.npos) return false;

	text.replace(pos1, pos2 - pos1, (replacement));
	return true;
}

bool ShaderCreator::findStrAndErase(std::string& text, const std::string& str)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.erase(pos, str.size());
	return true;
}

bool ShaderCreator::findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	text.replace(text.begin() + pos, text.begin() + pos + str.size(), replacement);
	return true;
}

bool ShaderCreator::findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement)
{
	size_t pos = text.find(str, 0);
	if (pos == text.npos) return false;

	size_t eol = text.find('\n', pos) - 1;
	if (eol == text.npos) return false;
	eol++;	// <<< why is this needed? Otherwise, something in the text is messed up (#line)

	text.replace(text.begin() + pos, text.begin() + eol, replacement);
	return true;
}
