#include "Material.h"
#include "../Image2D.h"
#include "../Context.h"
#include "../GraphicsPipeline.h"
Material::Material(Context &context, const char* name)
    : m_context(context)
	, name(name==nullptr?"":name)
    , properties()
    , isWireframe(false)
    , faceCull(true)
	, shaderMode(Phong)
	//, alphaBlendMode{ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA }//C++11 required (maybe usable VS2015?)
{
	//alphaBlendMode[0] = GL_SRC_ALPHA;
	//alphaBlendMode[1] = GL_ONE_MINUS_SRC_ALPHA;
}

Material::~Material()
{

}

void Material::updatePropertiesUniform()
{
	//Update descriptor pool?
}
/**
 * Todo: compare textures
 */
bool  Material::operator==(Material& other) const
{
    if (
        this->properties.diffuse == other.getDiffuse() &&
        this->properties.specular == other.getSpecular() &&
        this->properties.ambient == other.getAmbient() &&
        this->properties.emissive == other.getEmissive() &&
        this->properties.transparent == other.getTransparent() &&
        this->properties.opacity == other.getOpacity() &&
        this->properties.shininess == other.getShininess() &&
        this->properties.shininessStrength == other.getShininessStrength() &&
        this->properties.refractionIndex == other.getRefractionIndex() &&
        this->isWireframe == other.getWireframe() &&
        this->faceCull == !other.getTwoSided() &&
        this->shaderMode == other.getShadingMode()// &&
        //this->getAlphaBlendMode() == other.getAlphaBlendMode()
        )
        return true;
    return false;
}
/**
 * Temporary texture solution before proper materials
 */
void Material::addTexture(std::shared_ptr<Image2D> texPtr, TextureType type)
{
	TextureFrame t;
	t.texture = texPtr;
	textures[type].push_back(t);
	assert(type == Diffuse);

	vk::DescriptorImageInfo imageInfo;
	{
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texPtr->ImageView();
		imageInfo.sampler = texPtr->Sampler();
	}
	std::array<vk::WriteDescriptorSet, 1> descWrites;
	{
		descWrites[0].dstSet = m_descriptorSet;
		descWrites[0].dstBinding = 0;
		descWrites[0].dstArrayElement = 0;
		descWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descWrites[0].descriptorCount = 1;
		descWrites[0].pImageInfo = &imageInfo;
		descWrites[0].pBufferInfo = nullptr;
		descWrites[0].pTexelBufferView = nullptr;
	}
	m_context.Device().updateDescriptorSets((unsigned int)descWrites.size(), descWrites.data(), 0, nullptr);
}

void Material::use(vk::CommandBuffer &cb)
{
	//In future choose pipeline (or specialisation) here

	//Set Texture
	cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_context.Pipeline().Layout().get(), 1, { m_descriptorSet }, {});
}

void Material::setDescriptorSet(vk::DescriptorSet descSet)
{
	m_descriptorSet = descSet;
}