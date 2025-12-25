#ifndef SHADER_HPP
#define SHADER_HPP

#include "polygonum/bindings.hpp"
#include "polygonum/vertex.hpp"

class Shader;
class SMod;
class ShaderLoader;
   class SL_fromBuffer;
   class SL_fromFile;
class ShaderIncluder;
class ShaderCreator;

enum RPtype { geometry, lighting, forward, postprocessing };

// Container for a shader.
class Shader : public InterfaceForPointersManagerElements<std::string, Shader>
{
public:
	Shader(VulkanCore& c, const std::string id, VkShaderModule shaderModule);
	~Shader();

	VulkanCore& c;   //!< Used in destructor.
	const std::string id;   //!< Used for checking whether a shader to load is already loaded.
	const VkShaderModule shaderModule;
};

/// Shader modification. Change that can be applied to a shader (via applyModification()) before compilation (preprocessing operations). Constructible through a factory method. 
class SMod
{
public:
	bool applyModification(std::string& shader);
	unsigned getModType();
	std::vector<std::string> getParams();

	// Factory methods
	static SMod none();   // nothing changes
	static SMod albedo(std::string index);   // get albedo map from texture sampler
	static SMod specular(std::string index);   // get specular map from texture sampler
	static SMod roughness(std::string index);   // get roughness map from texture sampler
	static SMod normal();   // get normal map from texture sampler
	static SMod discardAlpha();   // discard fragments with less than X alpha value
	static SMod backfaceNormals();   // 
	static SMod sunfaceNormals();   // 
	static SMod verticalNormals();   // 
	static SMod wave(std::string speed, std::string amplitude, std::string minHeight);   // make mesh wave (sine wave)
	static SMod distDithering(std::string near, std::string far);   // apply dithering to distant objects
	static SMod earlyDepthTest();   // 
	static SMod dryColor(std::string color, std::string minHeight, std::string maxHeight);   // 
	static SMod changeHeader(std::string path);   // 

private:
	SMod(unsigned modificationType, std::initializer_list<std::string> params = { });

	bool findStrAndErase(std::string& text, const std::string& str);										//!< Find string and erase it.
	bool findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement);		//!< Find string and replace it with another.
	bool findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement);	//!< Find string and replace from beginning of string to end-of-line.
	bool findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement);	//!< Find two sub-strings and replace what is in between the beginning of string 1 and end of string 2.

	unsigned modificationType;
	std::vector<std::string> params;
};

/// ADT for loading a shader from any source. Subclasses will define how data is taken from source (getRawData): from file, from buffer, etc.
class ShaderLoader
{
	std::vector<SMod> mods;				//!< Modifications to the shader.
	void applyModifications(std::string& shader);	//!< Applies modifications defined by "mods".

protected:
	ShaderLoader(const std::string& id, const std::initializer_list<SMod>& modifications);
	virtual void getRawData(std::string& glslData) = 0;

	std::string id;

public:
	virtual ~ShaderLoader() {};

	std::shared_ptr<Shader> loadShader(PointersManager<std::string, Shader>& loadedShaders, VulkanCore& c);	//!< Get an iterator to the shader in loadedShaders. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
	virtual ShaderLoader* clone() = 0;
};

/// Pass the shader as a string at construction time. Call to getRawData will pass that string.
class SL_fromBuffer : public ShaderLoader
{
	SL_fromBuffer(const std::string& id, const std::string& glslText, const std::initializer_list<SMod>& modifications);
	void getRawData(std::string& glslData) override;

	std::string data;

public:
	static SL_fromBuffer* factory(std::string id, const std::string& glslText, std::initializer_list<SMod> modifications = {});
	ShaderLoader* clone() override;
};

/// Pass a text file path at construction time. Call to getRawData gets the shader from that file.
class SL_fromFile : public ShaderLoader
{
	SL_fromFile(const std::string& filePath, std::initializer_list<SMod>& modifications);
	void getRawData(std::string& glslData) override;

	std::string filePath;

public:
	static SL_fromFile* factory(std::string filePath, std::initializer_list<SMod> modifications = {});
	ShaderLoader* clone() override;
};

/**
	Includer interface for being able to "#include" headers data on shaders
	Renderer::newShader():
		- readFile(shader)
		- shaderc::CompileOptions < ShaderIncluder
		- shaderc::Compiler::PreprocessGlsl()
			- Preprocessor directive exists?
				- ShaderIncluder::GetInclude()
				- ShaderIncluder::ReleaseInclude()
				- ShaderIncluder::~ShaderIncluder()
		- shaderc::Compiler::CompileGlslToSpv
*/
class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
public:
	~ShaderIncluder() {};

	// Handles shaderc_include_resolver_fn callbacks.
	shaderc_include_result* GetInclude(const char* sourceName, shaderc_include_type type, const char* destName, size_t includeDepth) override;

	// Handles shaderc_include_result_release_fn callbacks.
	void ReleaseInclude(shaderc_include_result* data) override;
};

/**
	Helper class for creating shaders for the render pipeline RP_DS_PP.
	It creates the vertex (VS) and fragment (FS) shader for a given sub-pass (RPtype).
*/
class ShaderCreator
{
public:
	ShaderCreator(RPtype rendPass, const VertexType& vertexType, const BindingSet& bindings);

	struct ShaderCode
	{
		std::vector <std::string> header;
		std::vector <std::string> includes;
		std::vector <std::string> flags;
		std::vector <std::string> structs;
		std::vector <BindingBuffer> bind_globalBuffers;
		std::vector <BindingBuffer> bind_localBuffers;
		std::vector <unsigned> bind_textures;
		std::vector <std::string> input;
		std::vector <std::string> output;
		std::vector <std::string> globals;
		std::vector <std::string> main_begin;
		std::vector <std::string> main_processing;
		std::vector <std::string> main_end;
		std::vector <std::string> others;
	};

	ShaderCode vs, fs;

	std::string getShader0(unsigned shaderType);   //!< 0 (vertex), 1 (fragment)
	std::string getShader(unsigned shaderType);   //!< 0 (vertex), 1 (fragment)
	void printShader(unsigned shaderType);   //!< 0 (vertex), 1 (fragment)
	void printAllShaders();

	ShaderCreator& replaceMainBegin(unsigned shaderType, std::string& text, const std::string& substring, const std::string& replacement);   //!< Replace an entire line in main_begin with your own if it contains certain substring.
	ShaderCreator& replaceMainEnd(unsigned shaderType, std::string& text, const std::string& substring, const std::string& replacement);   //!< Replace an entire line in main_end with your own if it contains certain substring.
	ShaderCreator& setVerticalNormals();   //!< (VS) Make all normals vertical (0,0,1) before MVP transformation.

private:
	RPtype rpType;

	void setVS();
	void setFS_forward();
	void setFS_geometry();
	ShaderCreator& setForward();   // Shaders for a Forward pass
	ShaderCreator& setGeometry();   // Shaders for a Geometry pass

	void setBasics();
	void setBindings(const BindingSet& bindings);
	void setVS_general(const VertexType& vertexType);
	void setForward(const VertexType& vertexType, const BindingSet& bindings);
	void setGeometry(const VertexType& vertexType, const BindingSet& bindings);
	void setLighting();   // Shaders for a Lighting pass
	void setPostprocess();   // Shaders for a Postprocessing pass

	unsigned firstBindingNumber(unsigned shaderType);
	BindingBufferType getDescType(const BindingBuffer* bindBuffer);
	std::string getBuffer(const BindingBuffer& buffer, unsigned bindingName, unsigned nameName, bool isGlobal);
	std::unordered_map<VertAttrib, unsigned> usedAttribTypes(const VertexType& vertexType);

	bool replaceAllIfContains(std::string& text, const std::string& substring, const std::string& replacement);
	bool findTwoAndReplaceBetween(std::string& text, const std::string& str1, const std::string& str2, const std::string& replacement);
	bool findStrAndErase(std::string& text, const std::string& str);
	bool findStrAndReplace(std::string& text, const std::string& str, const std::string& replacement);
	bool findStrAndReplaceLine(std::string& text, const std::string& str, const std::string& replacement);
};

#endif