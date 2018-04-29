#ifndef __Buffer_h__
#define __Buffer_h__
#include <vulkan/vulkan.hpp>
class Context;
/*
 * Convenience class which wraps vk::Buffer
 * TODO: Buffers utilising a shared memory pool
 */
class Buffer
{
	Context &m_context;
	vk::Buffer m_buffer = nullptr;
	vk::DeviceMemory m_bufferMemory = nullptr;
	vk::DeviceSize m_size;
	const vk::BufferUsageFlags m_usageFlags;
	const vk::MemoryPropertyFlags m_memoryProperties;
	void copy(const vk::Buffer &src, const vk::Buffer &dest, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset);
	void free();
protected:
	/*
	* Resizes the buffer, losing the existing data in the process
	*/
	void resize(const vk::DeviceSize &size);
public:
	Buffer(Context &context, const vk::DeviceSize &size, const vk::BufferUsageFlags &usage, const vk::MemoryPropertyFlags &properties);
	~Buffer();
	void setData(Buffer &src, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset)
	{
		copy(src.get(), m_buffer, size, srcOffset, dstOffset);
	}
	void getData(Buffer &dest, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset)
	{
		copy(m_buffer, dest.get(), size, srcOffset, dstOffset);
	}
	void setData(const vk::Buffer &src, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset)
	{
		copy(src, m_buffer, size, srcOffset, dstOffset);
	}
	void getData(const vk::Buffer &dest, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset)
	{
		copy(m_buffer, dest, size, srcOffset, dstOffset);
	}
	void setData(const void *src, const size_t size, const vk::DeviceSize &destOffset=0);

	vk::Buffer get() { return m_buffer; }
	vk::Buffer *getPtr() { return &m_buffer; }
};

class VertexBuffer: public Buffer
{
public:
	VertexBuffer(Context &context, const vk::DeviceSize &size)
		:Buffer(context, size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal)
	{	}

};
class IndexBuffer : public Buffer
{
public:
	IndexBuffer(Context &context, const vk::DeviceSize &size)
		:Buffer(context, size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal)
	{	}

};
class StagingBuffer : public Buffer
{
public:
	StagingBuffer(Context &context, const vk::DeviceSize &size, void *data=nullptr)
		:Buffer(context, size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
	{
		if (data)
			setData(data, size);
	}
	void resize(const vk::DeviceSize &size) { Buffer::resize(size); }
	
};
#endif //__Buffer_h__