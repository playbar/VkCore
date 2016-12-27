#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "vulkantools.h"

namespace vk
{	

	struct Buffer
	{
		VkBuffer buffer;
		VkDevice device;
		VkDeviceMemory memory;
		VkDescriptorBufferInfo descriptor;
		VkDeviceSize size = 0;
		VkDeviceSize alignment = 0;
		void* mapped = nullptr;

		VkBufferUsageFlags usageFlags;
		VkMemoryPropertyFlags memoryPropertyFlags;

		VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			return vkMapMemory(device, memory, offset, size, 0, &mapped);
		}

		void unmap()
		{
			if (mapped)
			{
				vkUnmapMemory(device, memory);
				mapped = nullptr;
			}
		}

		VkResult bind(VkDeviceSize offset = 0)
		{
			return vkBindBufferMemory(device, buffer, memory, offset);
		}

		void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			descriptor.offset = offset;
			descriptor.buffer = buffer;
			descriptor.range = size;
		}

		void copyTo(void* data, VkDeviceSize size)
		{
			assert(mapped);
			memcpy(mapped, data, size);
		}

		VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			VkMappedMemoryRange mappedRange = {};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = memory;
			mappedRange.offset = offset;
			mappedRange.size = size;
			return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
		}

		VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0)
		{
			VkMappedMemoryRange mappedRange = {};
			mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			mappedRange.memory = memory;
			mappedRange.offset = offset;
			mappedRange.size = size;
			return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
		}

		void destroy()
		{
			if (buffer)
			{
				vkDestroyBuffer(device, buffer, nullptr);
			}
			if (memory)
			{
				vkFreeMemory(device, memory, nullptr);
			}
		}

	};
}