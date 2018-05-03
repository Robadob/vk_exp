#include "Buffer.h"
#include "Context.h"
#include "GraphicsPipeline.h"


Buffer::Buffer(Context &ctxt, const vk::DeviceSize &size, const vk::BufferUsageFlags &usage, const vk::MemoryPropertyFlags &properties)
    : m_context(ctxt)
    , m_usageFlags(usage)
    , m_memoryProperties(properties)
{
    resize(size);
}
Buffer::~Buffer()
{
    free();
}
void Buffer::resize(const vk::DeviceSize &size)
{
    //Free existing buffer
    free();
    //Define buffer
    vk::BufferCreateInfo bufferInfo;
    {
        bufferInfo.flags = {};
        bufferInfo.size = size;
        bufferInfo.usage = m_usageFlags;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
    }
    m_buffer = m_context.Device().createBuffer(bufferInfo);
    //Allocate memory
    const vk::MemoryRequirements memReq = m_context.Device().getBufferMemoryRequirements(m_buffer);
    m_size = memReq.size;
    vk::MemoryAllocateInfo allocInfo;
    {
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = m_context.findMemoryType(memReq.memoryTypeBits, m_memoryProperties);
    }
    m_bufferMemory = m_context.Device().allocateMemory(allocInfo);
    m_context.Device().bindBufferMemory(m_buffer, m_bufferMemory, 0);//Offset must be divisible by memReq.alignment
}
void Buffer::free()
{
    if(m_buffer)
        m_context.Device().destroyBuffer(m_buffer);
    m_buffer = nullptr;
    if(m_bufferMemory)
        m_context.Device().freeMemory(m_bufferMemory);
    m_bufferMemory = nullptr;
    m_size = 0;
}
void Buffer::copy(const vk::Buffer &src, const vk::Buffer &dest, const vk::DeviceSize &size, const vk::DeviceSize &srcOffset, const vk::DeviceSize &dstOffset)
{
    vk::CommandBuffer cb = m_context.beginSingleTimeCommands();
    vk::BufferCopy copyRegion;
    {
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = size;
    }
    cb.copyBuffer(src, dest, 1, &copyRegion);
    m_context.endSingleTimeCommands(cb);
}
void Buffer::setData(const void *src, const size_t size, const vk::DeviceSize &destOffset)
{
#ifdef _DEBUG
    assert(destOffset + size <= m_size);
#endif
    if ((vk::MemoryPropertyFlags(m_memoryProperties) &= vk::MemoryPropertyFlagBits::eHostCoherent) == vk::MemoryPropertyFlagBits::eHostCoherent)
    {//Mapped copy if memory is coherent
        void* data;
        m_context.Device().mapMemory(m_bufferMemory, destOffset, size, {}, &data);
        memcpy(data, src, size);
        m_context.Device().unmapMemory(m_bufferMemory);
    }
    else if ((vk::BufferUsageFlags(m_usageFlags) &= vk::BufferUsageFlagBits::eTransferDst) == vk::BufferUsageFlagBits::eTransferDst)
    {//Copy to with staging buffer
        StagingBuffer b(m_context, size);
        b.setData(src, size);
        setData(b, size, 0, destOffset);
    }
    else
        throw std::runtime_error("Buffer::copyTo() failed, unsupported buffer/memory properties.");
}