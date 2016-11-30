#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <vulkan/vulkan.h>
#include "vulkantools.h"
#include "vulkandebug.h"
#include "vulkanbuffer.hpp"
#include "VkCoreDevice.hpp"

#include "../external/stb/stb_font_consolas_24_latin1.inl"

// Defines for the STB font used
// STB font files can be found at http://nothings.org/stb/font/
#define STB_FONT_NAME stb_font_consolas_24_latin1
#define STB_FONT_WIDTH STB_FONT_consolas_24_latin1_BITMAP_WIDTH
#define STB_FONT_HEIGHT STB_FONT_consolas_24_latin1_BITMAP_HEIGHT 
#define STB_FIRST_CHAR STB_FONT_consolas_24_latin1_FIRST_CHAR
#define STB_NUM_CHARS STB_FONT_consolas_24_latin1_NUM_CHARS

// Max. number of chars the text overlay buffer can hold
#define MAX_CHAR_COUNT 2048

/**
* @brief Mostly self-contained text overlay class
* @note Will only work with compatible render passes
*/ 
class VulkanTextOverlay
{
private:
	VkCoreDevice *mVulkanDevice;

	VkQueue mQueue;
	VkFormat mColorFormat;
	VkFormat mDepthFormat;

	uint32_t *mFrameBufferWidth;
	uint32_t *mFrameBufferHeight;

	VkSampler mSampler;
	VkImage mImage;
	VkImageView mImageView;
	vk::Buffer mVertexBuffer;
	VkDeviceMemory mImageMemory;
	VkDescriptorPool mDescriptorPool;
	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorSet mDescriptorSet;
	VkPipelineLayout mPipelineLayout;
	VkPipelineCache mPipelineCache;
	VkPipeline mPipeline;
	VkRenderPass mRenderPass;
	VkCommandPool mCommandPool;
	std::vector<VkFramebuffer*> mFrameBuffers;
	std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;
	VkFence mFence;

	// Used during text updates
	glm::vec4 *mMappedLocal = nullptr;

	stb_fontchar mStbFontData[STB_NUM_CHARS];
	uint32_t mNumLetters;

public:

	enum TextAlign { alignLeft, alignCenter, alignRight };

	bool mVisible = true;
	bool mInvalidated = false;

	std::vector<VkCommandBuffer> mCmdBuffers;

	/**
	* Default constructor
	*
	* @param vulkanDevice Pointer to a valid VulkanDevice
	*/
	VulkanTextOverlay(
		VkCoreDevice *vulkanDevice,
		VkQueue queue,
		std::vector<VkFramebuffer> &framebuffers,
		VkFormat colorformat,
		VkFormat depthformat,
		uint32_t *framebufferwidth,
		uint32_t *framebufferheight,
		std::vector<VkPipelineShaderStageCreateInfo> shaderstages);

	/**
	* Default destructor, frees up all Vulkan resources acquired by the text overlay
	*/
	~VulkanTextOverlay();

	/**
	* Prepare all vulkan resources required to render the font
	* The text overlay uses separate resources for descriptors (pool, sets, layouts), pipelines and command buffers
	*/
	void prepareResources();

	/**
	* Prepare a separate pipeline for the font rendering decoupled from the main application
	*/
	void preparePipeline();

	/**
	* Prepare a separate render pass for rendering the text as an overlay
	*/
	void prepareRenderPass();

	/**
	* Maps the buffer, resets letter count
	*/
	void beginTextUpdate();

	/**
	* Add text to the current buffer
	*
	* @param text Text to add
	* @param x x position of the text to add in window coordinate space
	* @param y y position of the text to add in window coordinate space
	* @param align Alignment for the new text (left, right, center)
	*/
	void addText(std::string text, float x, float y, TextAlign align);

	/**
	* Unmap buffer and update command buffers
	*/
	void endTextUpdate();

	/**
	* Update the command buffers to reflect text changes
	*/
	void updateCommandBuffers();

	/**
	* Submit the text command buffers to a queue
	*/
	void submit(VkQueue queue, uint32_t bufferindex);

	/**
	* Reallocate command buffers for the text overlay
	* @note Frees the existing command buffers
	*/
	void reallocateCommandBuffers();

};