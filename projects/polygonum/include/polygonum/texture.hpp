#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "polygonum/environment.hpp"
#include "polygonum/toolkit.hpp"

#include <string>

class Texture;
   class Tex_fromBuffer;
   class Tex_fromFile;

class Renderer;

enum TexType { tAlb, tSpec, tRoug, tSpecroug, tNorm, tUndef, texMax };

/// Container for a texture.
class Texture : public InterfaceForPointersManagerElements<std::string, Texture>
{
	virtual void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight);

	std::pair<VkImage, VkDeviceMemory> createTextureImage(unsigned char* pixels, int32_t texWidth, int32_t texHeight, uint32_t& mipLevels, Renderer& r);
	VkImageView createTextureImageView(VkImage textureImage, uint32_t mipLevels, VulkanCore& c);
	VkSampler createTextureSampler(uint32_t mipLevels, VulkanCore& c);

public:
	Texture(const std::string& id, TexType type, VulkanCore& c, VkImage textureImage, VkDeviceMemory textureImageMemory, VkImageView textureImageView, VkSampler textureSampler, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	Texture(const std::string& id, TexType type, VkFormat imageFormat, VkSamplerAddressMode addressMode);
	~Texture();

	const std::string id;   //!< Used for checking whether the texture to load is already loaded.
	const TexType type;   //!< Used for shader creation.
	VkFormat imageFormat;   //!< Used for creating texture.
	VkSamplerAddressMode addressMode;   //!< Used for creating texture.

	Image texture;

	std::shared_ptr<Texture> loadTexture(Renderer& r);	//!< Get an iterator to the Texture in Renderer::textures list. If it's not in that list, it loads it, saves it in the list, and gets the iterator. 
};

/// Pass the texture as vector of bytes (unsigned char) at construction time. Call to getRawData will pass that string.
class Tex_fromBuffer : public Texture
{
	void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) override;

	std::vector<unsigned char> data;
	int32_t texWidth, texHeight;

public:
	Tex_fromBuffer(const std::string& id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode, TexType type);

	static std::shared_ptr<Tex_fromBuffer> factory(const std::string id, unsigned char* pixels, int texWidth, int texHeight, TexType texType = tUndef, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	//TextureLoader* clone() override;
};

/// Pass a texture file path at construction time. Call to getRawData gets the texture from that file.
class Tex_fromFile : public Texture
{
	void getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) override;

	std::string filePath;

public:
	Tex_fromFile(const std::string& filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode, TexType type);

	static std::shared_ptr<Tex_fromFile> factory(const std::string filePath, TexType texType = tUndef, VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	//TextureLoader* clone() override;
};


#endif