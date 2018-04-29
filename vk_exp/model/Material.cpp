#include "Material.h"
#include "../Image2D.h"
Material::Material(const char* name)
    : name(name==nullptr?"":name)
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
}

void Material::use(glm::mat4 &transform)
{

}

void Material::setDescriptorSet(vk::DescriptorSet descSet)
{
	m_descriptorSet = descSet;
}