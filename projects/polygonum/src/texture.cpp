#include "polygonum/texture.hpp"
#include "polygonum/renderer.hpp"

#define STB_IMAGE_IMPLEMENTATION		// Import textures
#include "stb_image.h"

Texture::Texture(const std::string& id, TexType type, VulkanCore& c, VkImage textureImage, VkDeviceMemory textureImageMemory, VkImageView textureImageView, VkSampler textureSampler, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: id(id), type(type), imageFormat(imageFormat), addressMode(addressMode), texture(&c, textureImage, textureImageMemory, textureImageView, textureSampler) { }

Texture::Texture(const std::string& id, TexType type, VkFormat imageFormat, VkSamplerAddressMode addressMode)
	: id(id), type(type), imageFormat(imageFormat), addressMode(addressMode) { }

Texture::~Texture()
{
#ifdef DEBUG_IMPORT
	std::cout << typeid(*this).name() << "::" << __func__ << std::endl;
#endif

	if (texture.image) texture.destroy();
}

std::shared_ptr<Texture> Texture::loadTexture(Renderer& r)
{
#ifdef DEBUG_RESOURCES
	std::cout << typeid(*this).name() << "::" << __func__ << ": " << this->id << std::endl;
#endif

	//this->c = &core;

	// Look for it in Renderer::textures.
	if (r.textures.contains(id))
		return r.textures.get(id);

	// Load an image
	unsigned char* pixels;
	int32_t texWidth, texHeight;
	getRawData(pixels, texWidth, texHeight);

	// Get arguments for creating the texture object
	uint32_t mipLevels;		//!< Number of levels (mipmaps)
	std::pair<VkImage, VkDeviceMemory> image = createTextureImage(pixels, texWidth, texHeight, mipLevels, r);
	VkImageView textureImageView = createTextureImageView(std::get<VkImage>(image), mipLevels, r.c);
	VkSampler textureSampler = createTextureSampler(mipLevels, r.c);

	// Create and save texture object
	return r.textures.emplace(id, std::ref(id), type, std::ref(r.c), std::get<VkImage>(image), std::get<VkDeviceMemory>(image), textureImageView, textureSampler, imageFormat, addressMode);
}

void Texture::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight) { }

std::pair<VkImage, VkDeviceMemory> Texture::createTextureImage(unsigned char* pixels, int32_t texWidth, int32_t texHeight, uint32_t& mipLevels, Renderer& r)
{
#ifdef DEBUG_RESOURCES
	std::cout << "   " << __func__ << std::endl;
#endif

	VkDeviceSize imageSize = texWidth * texHeight * 4;												// 4 bytes per rgba pixel
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;	// Calculate the number levels (mipmaps)

	// Create a staging buffer (temporary buffer in host visible memory so that we can use vkMapMemory and copy the pixels to it)
	VkBuffer	   stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	r.c.createBuffer(
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	// Copy directly the pixel values from the image we loaded to the staging-buffer.
	void* data;
	vkMapMemory(r.c.device, stagingBufferMemory, 0, imageSize, 0, &data);	// vkMapMemory retrieves a host virtual address pointer (data) to a region of a mappable memory object (stagingBufferMemory). We have to provide the logical device that owns the memory (e.device).
	memcpy(data, pixels, static_cast<size_t>(imageSize));					// Copies a number of bytes (imageSize) from a source (pixels) to a destination (data).
	vkUnmapMemory(r.c.device, stagingBufferMemory);						// Unmap a previously mapped memory object (stagingBufferMemory).
	stbi_image_free(pixels);	// Clean up the original pixel array

	// Create the texture image
	VkImage			textureImage;
	VkDeviceMemory	textureImageMemory;

	r.c.createImage(
		textureImage,
		textureImageMemory,
		texWidth, texHeight,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		imageFormat,			// VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R64_SFLOAT
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	// Copy the staging buffer to the texture image
	r.commander.transitionImageLayout(textureImage, imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);					// Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	r.commander.copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));											// Execute the buffer to image copy operation
	// Transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
	// transitionImageLayout(textureImage, imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);	// To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access
	r.commander.generateMipmaps(textureImage, imageFormat, texWidth, texHeight, mipLevels);

	// Cleanup the staging buffer and its memory
	r.c.destroyBuffer(r.c.device, stagingBuffer, stagingBufferMemory);

	return std::pair(textureImage, textureImageMemory);
}

VkImageView Texture::createTextureImageView(VkImage textureImage, uint32_t mipLevels, VulkanCore& c)
{
#ifdef DEBUG_RESOURCES
	std::cout << "   " << __func__ << std::endl;
#endif

	VkImageView imageView;
	c.createImageView(imageView, textureImage, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

	return imageView;
}

VkSampler Texture::createTextureSampler(uint32_t mipLevels, VulkanCore& c)
{
#ifdef DEBUG_RESOURCES
	std::cout << "   " << __func__ << std::endl;
#endif

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;					// How to interpolate texels that are magnified (oversampling) or ...
	samplerInfo.minFilter = VK_FILTER_LINEAR;					// ... minified (undersampling). Choices: VK_FILTER_NEAREST, VK_FILTER_LINEAR
	samplerInfo.addressModeU = addressMode;						// Addressing mode per axis (what happens when going beyond the image dimensions). In texture space coordinates, XYZ are UVW. Available values: VK_SAMPLER_ADDRESS_MODE_ ... REPEAT (repeat the texture), MIRRORED_REPEAT (like repeat, but inverts coordinates to mirror the image), CLAMP_TO_EDGE (take the color of the closest edge), MIRROR_CLAMP_TO_EDGE (like clamp to edge, but taking the opposite edge), CLAMP_TO_BORDER (return solid color).
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;

	if (c.deviceData.samplerAnisotropy)						// If anisotropic filtering is available (see isDeviceSuitable) <<<<<
	{
		samplerInfo.anisotropyEnable = VK_TRUE;							// Specify if anisotropic filtering should be used
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(c.physicalDevice, &properties);
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;		// another option:  samplerInfo.maxAnisotropy = 1.0f;
	}
	else
	{
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
	}

	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	// Color returned (black, white or transparent, in format int or float) when sampling beyond the image with VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER. You cannot specify an arbitrary color.
	samplerInfo.unnormalizedCoordinates = VK_FALSE;				// Coordinate system to address texels in an image. False: [0, 1). True: [0, texWidth) & [0, texHeight). 
	samplerInfo.compareEnable = VK_FALSE;						// If a comparison function is enabled, then texels will first be compared to a value, and the result of that comparison is used in filtering operations. This is mainly used for percentage-closer filtering on shadow maps (https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing). 
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;		// VK_SAMPLER_MIPMAP_MODE_ ... NEAREST (lod selects the mip level to sample from), LINEAR (lod selects 2 mip levels to be sampled, and the results are linearly blended)
	samplerInfo.minLod = 0.0f;									// minLod=0 & maxLod=mipLevels allow the full range of mip levels to be used
	samplerInfo.maxLod = static_cast<float>(mipLevels);			// lod: Level Of Detail
	samplerInfo.mipLodBias = 0.0f;								// Used for changing the lod value. It forces to use lower "lod" and "level" than it would normally use

	VkSampler textureSampler;
	c.createSampler(textureSampler, samplerInfo);
	return textureSampler;

	/*
	* VkImage holds the mipmap data. VkSampler controls how that data is read while rendering.
	* The sampler selects a mip level according to this pseudocode:
	*
	*	lod = getLodLevelFromScreenSize();						// Smaller when the object is close (may be negative)
	*	lod = clamp(lod + mipLodBias, minLod, maxLod);
	*
	*	level = clamp(floor(lod), 0, texture.miplevels - 1);	// Clamped to the number of miplevels in the texture
	*
	*	if(mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST)		// Sample operation
	*		color = sampler(level);
	*	else
	*		color = blend(sample(level), sample(level + 1));
	*
	*	if(lod <= 0)											// Filter
	*		color = readTexture(uv, magFilter);
	*	else
	*		color = readTexture(uv, minFilter);
	*/
}

Tex_fromBuffer::Tex_fromBuffer(const std::string& id, unsigned char* pixels, int texWidth, int texHeight, VkFormat imageFormat, VkSamplerAddressMode addressMode, TexType type)
	: Texture(id, type, imageFormat, addressMode), texWidth(texWidth), texHeight(texHeight)
{
	size_t size = sizeof(float) * texWidth * texHeight;
	data.resize(size);
	std::copy(pixels, pixels + size, data.data());		// memcpy(data, pixels, size);
}

std::shared_ptr<Tex_fromBuffer> Tex_fromBuffer::factory(const std::string id, unsigned char* pixels, int texWidth, int texHeight, TexType texType, VkFormat imageFormat, VkSamplerAddressMode addressMode)
{
	return std::make_shared<Tex_fromBuffer>(id, pixels, texWidth, texHeight, imageFormat, addressMode, texType);
}

//TextureLoader* Tex_fromBuffer::clone() { return new TL_fromBuffer(*this); }

void Tex_fromBuffer::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight)
{
	texWidth = this->texWidth;
	texHeight = this->texHeight;

	pixels = new unsigned char[data.size()];
	std::copy(data.data(), data.data() + data.size(), pixels);
}

Tex_fromFile::Tex_fromFile(const std::string& filePath, VkFormat imageFormat, VkSamplerAddressMode addressMode, TexType type)
	: Texture(filePath, type, imageFormat, addressMode), filePath(filePath) { }

std::shared_ptr<Tex_fromFile> Tex_fromFile::factory(const std::string filePath, TexType texType, VkFormat imageFormat, VkSamplerAddressMode addressMode)
{
	return std::make_shared<Tex_fromFile>(filePath, imageFormat, addressMode, texType);
}

//TextureLoader* Tex_fromFile::clone() { return new TL_fromFile(*this); }

void Tex_fromFile::getRawData(unsigned char*& pixels, int32_t& texWidth, int32_t& texHeight)
{
	int texChannels;
	pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);	// Returns a pointer to an array of pixel values. STBI_rgb_alpha forces the image to be loaded with an alpha channel, even if it doesn't have one.
	if (!pixels) throw std::runtime_error("Failed to load texture image!");
}

