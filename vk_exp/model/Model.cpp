#include "Model.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/component_wise.hpp>
#include "Model_assimpUtils.h"
#include <vulkan/vulkan.hpp>
#include "../StringUtils.h"
#include "../Context.h"
#include "../GraphicsPipeline.h"
#include "../Image2D.h"

ModelData::ModelData(
    Context &context,
    size_t vertices,
    size_t normals,
    size_t colors,
    size_t texcoords,
    size_t bones,
    size_t materials,
    size_t faces,
    size_t transforms
)
    : m_context(context)
    , vertices(nullptr)
    , normals(nullptr)
    , colors(nullptr)
    , texcoords(nullptr)
    , bones(nullptr)
    , materials(nullptr)
    , faces(nullptr)
    , transforms(nullptr)
    , verticesSize(vertices)
    , normalsSize(normals)
    , colorsSize(colors)
    , texcoordsSize(texcoords)
    , bonesSize(bones)
    , materialsSize(materials)
    , facesSize(faces)
    , transformsSize(transforms)

{
    assert(vertices && normals && texcoords && !colors);//Currently using fixed pipeline so only this vertex layout supported.
    this->vertices = static_cast<glm::vec3 *>(malloc(vertices * sizeof(glm::vec3)));
    if (normals)
        this->normals = static_cast<glm::vec3 *>(malloc(normals * sizeof(glm::vec3)));
    if (colors)
        this->colors = static_cast<glm::vec4 *>(malloc(colors * sizeof(glm::vec4)));
    if (texcoords)
        this->texcoords = static_cast<glm::vec2 *>(malloc(texcoords * sizeof(glm::vec2)));

    if (bones)
        this->bones = static_cast<Bone *>(malloc(bones * sizeof(Bone)));

    if (materials)
    {
        this->materials = static_cast<std::shared_ptr<Material> *>(malloc(materials * sizeof(std::shared_ptr<Material>)));
        for (unsigned int i = 0; i < materials; ++i)
        {
            new(&this->materials[i]) std::shared_ptr<Material>();
            this->materials[i] = std::make_shared<Material>(m_context);
        }

        {//Allocate pools to create one for each material
            std::array<vk::DescriptorPoolSize, 1> poolSizes;
            {
                poolSizes[0].type = vk::DescriptorType::eCombinedImageSampler;
                poolSizes[0].descriptorCount = (unsigned int)materials;
            }
            vk::DescriptorPoolCreateInfo poolCreateInfo;
            {
                poolCreateInfo.poolSizeCount = (unsigned int)poolSizes.size();
                poolCreateInfo.pPoolSizes = poolSizes.data();
                poolCreateInfo.maxSets = (unsigned int)materials;//1 per diffuse texture
                poolCreateInfo.flags = {};
            }
            m_descriptorPool = m_context.Device().createDescriptorPool(poolCreateInfo);
            std::vector<vk::DescriptorSetLayout> descSets;
            descSets.resize(materials);
            std::fill(descSets.begin(), descSets.end(), *m_context.Pipeline().Layout().textureSamplerSetLayout());
            vk::DescriptorSetAllocateInfo descSetAllocInfo;
            {
                descSetAllocInfo.descriptorPool = m_descriptorPool;
                descSetAllocInfo.descriptorSetCount = (unsigned int)descSets.size();
                descSetAllocInfo.pSetLayouts = descSets.data();
            }
            //Create set per material and attach
            m_descriptorSets = m_context.Device().allocateDescriptorSets(descSetAllocInfo);
            for (unsigned int i = 0; i<materials; ++i)
            {
                this->materials[i]->setDescriptorSet(m_descriptorSets[i]);
            }
        }
    }

    this->faces = static_cast<unsigned int *>(malloc(faces * sizeof(unsigned int)));
    this->transforms = static_cast<glm::mat4 *>(malloc(transforms * sizeof(glm::mat4)));
}
ModelData::~ModelData()
{
    //Free memory
    free(vertices);
    free(normals);
    free(colors);
    free(texcoords);
    free(bones);
    //Call destructor on materials
    for (unsigned int i = 0; i < materialsSize;++i)
        this->materials[i].~shared_ptr();
    if(m_descriptorPool)
        m_context.Device().destroyDescriptorPool(m_descriptorPool);//Automatically destroys all child sets
    free(materials);
    free(faces);
    free(transforms);
}
Model::Model(Context &context, const char *modelPath, float scale)
    : m_context(context)
    , root(nullptr)
    , data(nullptr)
    , modelPath(modelPath)
    , loadScale(scale)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    //, positions(GL_FLOAT, 3, sizeof(float))
    //, normals(GL_FLOAT, 3, sizeof(float))
    //, colors(GL_FLOAT, 4, sizeof(float))
    //, texcoords(GL_FLOAT, 2, sizeof(float))
{
    loadModel();
}
Model::~Model()
{
    freeModel();
}
void Model::freeModel()
{
    //Clear hierarchy
    root.reset();
    //Clear data
    data.reset();
    //Release VBOs
    if (m_vertexBuffer)
        delete m_vertexBuffer;
    m_vertexBuffer = nullptr;
    if (m_indexBuffer)
        delete m_indexBuffer;
    m_indexBuffer = nullptr;
}


void Model::reload()
{
    freeModel();
    loadModel();
}

VFCcount countVertices(const struct aiScene* scene, const struct aiNode* nd)
{
    VFCcount vfc;
    for (unsigned int n = 0; n < nd->mNumMeshes; ++n) {
        const struct aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
        vfc.v += mesh->mNumVertices;
        vfc.b += mesh->mNumBones;
        for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {
            vfc.f += mesh->mFaces[t].mNumIndices;
#ifdef _DEBUG
            //All faces per mesh are of the same type
            assert(mesh->mFaces[0].mNumIndices == mesh->mFaces[t].mNumIndices);
#endif
        }
    }

    for (unsigned int n = 0; n < nd->mNumChildren; ++n) {
        vfc += countVertices(scene, nd->mChildren[n]);
    }
    return vfc;
}
void Model::updateBoundingBox()
{
    boundingBox.reset();
    if (this->root)
    {
        boundingBox.include(this->root->calculateBoundingBox());
    }
}
unsigned int Model::linkBones(std::shared_ptr<ModelNode> node, std::shared_ptr<ModelData> data, const std::string * const boneNames)
{
    unsigned int rtn = 0;
    //Find this bones offset
    unsigned int i;
    for(i = 0; i< data->bonesSize;++i)
    {
        if (node->getName() == boneNames[i])
        {
            rtn += 1;
            node->setBoneOffset(i);
            break;
        }
    }
    //if (i == data->bonesSize)
    //    printf("Node '%s' doesn't have a matching bone!\n", node->getName().c_str());
    //else
    //    printf("bone '%s' matched!\n", node->getName().c_str());

    //Recurse for all children
    for (auto &&c:*node->getChildren())
    {
        rtn += linkBones(c, data, boneNames);
    }
    return rtn;
}
std::shared_ptr<ModelNode> Model::buildHierarchy(const struct aiScene* scene, const struct aiNode* nd, VFCcount &vfc, std::string* t_boneNames) const
{
    //Copy transformation matrix
    aiMatrix4x4 m = nd->mTransformation;
    aiTransposeMatrix4(&m);
    //aiTransposeMatrix4(&m);
#ifdef _DEBUG
    //aiMatrix4x4 is type float
    assert(sizeof(glm::mat4) == sizeof(aiMatrix4x4));
#endif
    data->transforms[vfc.c] = *reinterpret_cast<glm::mat4*>(&m);

    //Create the ModelNode for this component (and increment data offset)
    std::shared_ptr<ModelNode> rtn = ModelNode::make_shared(this->data, vfc.c++, nd->mName.C_Str());
    //Fill it with meshes
    for (unsigned int n = 0; n < nd->mNumMeshes; ++n) {
        const struct aiMesh* aiMesh = scene->mMeshes[nd->mMeshes[n]];

        if (aiMesh->mNumFaces == 0) continue;
        //Create a new mesh
        vk::PrimitiveTopology faceType;
        unsigned int mNumFaceIndices = aiMesh->mFaces[0].mNumIndices;
        switch (mNumFaceIndices) {
        case 1: faceType = vk::PrimitiveTopology::ePointList; break;
        case 2: faceType = vk::PrimitiveTopology::eLineList; break;
        case 3: faceType = vk::PrimitiveTopology::eTriangleList; break;
        default: faceType = vk::PrimitiveTopology::ePatchList; break;
        }
        std::shared_ptr<Mesh> mesh = Mesh::make_shared(this->data, aiMesh->mNumFaces*mNumFaceIndices, vfc.f, aiMesh->mMaterialIndex, faceType);

#ifdef _DEBUG
        //aiVector3D is type float
        assert(sizeof(glm::vec3) == sizeof(aiVector3D));
        //aiColor4D is type float
        assert(sizeof(glm::vec4) == sizeof(aiColor4D));
#endif
        //Copy data for each face
        //Vertex
        memcpy(data->vertices + vfc.v, aiMesh->mVertices, aiMesh->mNumVertices*sizeof(glm::vec3));
        //Normal
        if (data->normals)
            memcpy(data->normals + vfc.v, aiMesh->mNormals, aiMesh->mNumVertices*sizeof(glm::vec3));
        //Color (just take the first set)
        if (data->colors)
            memcpy(data->colors + vfc.v, aiMesh->mColors[0], aiMesh->mNumVertices*sizeof(glm::vec4));
        //Texture Coordinates (just take the first set)
        if (data->texcoords)
        {//Downcast tex coords from vec3 to vec2
            assert(aiMesh->mNumUVComponents[0] == 2);
            for (unsigned int i = 0; i < aiMesh->mNumVertices; ++i)
            {
                data->texcoords[vfc.v + i] = glm::vec2(aiMesh->mTextureCoords[0][i].x, aiMesh->mTextureCoords[0][i].y);
            }
            //memcpy(data->texcoords + vfc.v, aiMesh->mTextureCoords[0], aiMesh->mNumVertices*sizeof(glm::vec3));  
        }
        //Map face indexes
        for (unsigned int t = 0; t < aiMesh->mNumFaces; ++t) {
            const struct aiFace* face = &aiMesh->mFaces[t];
            for (unsigned int i = 0; i < face->mNumIndices; ++i) {
                int index = face->mIndices[i];
                data->faces[vfc.f + (t*mNumFaceIndices) + i] = vfc.v + index;
            }
        }

        //Bones
        if (data->bones&&t_boneNames)
            for (unsigned int t = 0; t<aiMesh->mNumBones;++t)
            {
                aiBone *bn = aiMesh->mBones[t];
                //Log the name to a temporary name array
                t_boneNames[vfc.b] = std::string(bn->mName.C_Str());
                //Store each bone with it's offset Matrix
                data->bones[vfc.b++] = Bone(*reinterpret_cast<glm::mat4*>(&bn->mOffsetMatrix));
            }

        //Increment data offsets
        vfc.v += aiMesh->mNumVertices;
        //vfc.b += aiMesh->mNumBones;
        vfc.f += aiMesh->mNumFaces*mNumFaceIndices;

        //Link mesh to hierarchy
        rtn->addMesh(mesh);
        //mesh->setParent(rtn);
    }
    //Recursivly fill children
    for (unsigned int n = 0; n < nd->mNumChildren; ++n) {
        std::shared_ptr<ModelNode> child = buildHierarchy(scene, nd->mChildren[n], vfc);
        rtn->addChild(child);
        //child->setParent(rtn);
    }
    return rtn;
}
/**
* Loads a model from file
* @TODO Also add support for importing bones, textures and materials
*/
void Model::loadModel()
{
    //Import model with assimp
    const aiScene* scene = aiImportFile(modelPath, aiProcessPreset_TargetRealtime_MaxQuality);

    if (!scene)
    {
        fprintf(stderr, "Err Model not found: %s\n", modelPath);
        return;
    }

    //Calculate total number of vertices, faces, components and bones within hierarchy
    this->vfc = countVertices(scene, scene->mRootNode);
    //Allocate memory
    this->data = std::make_shared<ModelData>(
        m_context,
        vfc.v,
        scene->mMeshes[0]->HasNormals() ? vfc.v : 0,
        scene->mMeshes[0]->mColors[0] != nullptr ? vfc.v : 0,
        scene->mMeshes[0]->HasTextureCoords(0) ? vfc.v : 0,
        vfc.b,
        scene->mNumMaterials,
        vfc.f,
        vfc.c
        );

    //Store count in VADs
    //positions.count = this->vfc.v;
    //normals.count = normals.data ? this->vfc.v : 0;
    //colors.count = colors.data ? this->vfc.v : 0;
    //texcoords.count = texcoords.data ? this->vfc.v : 0;
    //positions.data = data->vertices;
    //normals.data = data->normals;
    //colors.data = data->colors;
    //texcoords.data = data->texcoords;

    if (scene->HasTextures()){ fprintf(stderr, "Model '%s' has embedded textures, these are currently unsupported.\n", modelPath); }
    std::string modelFolder = su::folderFromPath(modelPath);
    //Copy materials
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        int texIndex = 0;
        aiReturn texFound = AI_SUCCESS;

        aiString path;    // filename

        //while (texFound == AI_SUCCESS)
        {
            texFound = scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, texIndex, &path);
            printf("tex path: %s\n", path.data);
            if(path.length)
            {
                aiString name;
                scene->mMaterials[i]->Get(AI_MATKEY_NAME, name);
                data->materials[i]->setName(name.C_Str());
                //Temporary till proper mat tex handling added
                data->materials[i]->addTexture(std::make_shared<Image2D>(m_context, path.data, modelFolder.c_str()));
                texIndex++;
            }
        }
      //TODO
    }

    //Convert assimp hierarchy to our custom hierarchy
    VFCcount rootCount = VFCcount(0);
    //Temp array parallel to modeldata->bones
    std::string *t_boneNames = new std::string[this->vfc.b];
    
    root = buildHierarchy(scene, scene->mRootNode, rootCount, nullptr);// t_boneNames);
    
    if (this->vfc.b)
    {
        //Traverse created hierarchy, setting bone offset within each node
        unsigned int linkedBoneCt = linkBones(root, data, t_boneNames);
        printf("Bones linked %d/%d\n", linkedBoneCt, this->vfc.b);
        //TODO: Purge bones if none linked?
        printf("Animations: %d\n", scene->mNumAnimations);
        for (unsigned int i = 0; i < scene->mNumAnimations;i++)
        {
            printf("%s\n", scene->mAnimations[i]->mName.C_Str());
        }
    }
    //Free temp bone name array
    delete[] t_boneNames;
    //Free assimp model
    aiReleaseImport(scene);

    //Calculate model scale
    updateBoundingBox();

    //Scale model
    if (loadScale>0)
    {
        const float maxDim = compMax(boundingBox.size());
        const float scaleFactor = 1.0f / (maxDim / loadScale);
        for (unsigned int i = 0; i < data->verticesSize; ++i)
            data->vertices[i] *= scaleFactor;
        updateBoundingBox();
    }

    printf("%s: (%.3f, %.3f, %.3f)\n", modelPath, boundingBox.size().x, boundingBox.size().y, boundingBox.size().z);

    //Calc VBO size
    size_t vboSize = 0;
    vboSize += this->vfc.v*sizeof(glm::vec3);
    if (data->normals)
        vboSize += this->vfc.v*sizeof(glm::vec3);
    if (data->colors)
        vboSize += this->vfc.v*sizeof(glm::vec4);
    if (data->texcoords)
        vboSize += this->vfc.v*sizeof(glm::vec3);

    //Build Vertex Buffer from data
    {//Transfer data to temporary strided array
        char *t_buff = (char*)malloc(vboSize);
        vboSize = 0;
        size_t vSz = sizeof(*data->vertices);
        size_t nSz = sizeof(*data->normals);
        size_t cSz = sizeof(*data->colors);
        size_t tcSz = sizeof(*data->texcoords);
        for(unsigned int i = 0;i<this->vfc.v;++i)
        {
            memcpy(t_buff + vboSize, data->vertices+i, vSz);
            vboSize += vSz;
            if (data->normals)
            {
                memcpy(t_buff + vboSize, data->normals+i, nSz);
                vboSize += nSz;
            }
            if (data->colors)
            {
                memcpy(t_buff + vboSize, data->colors + i, cSz);
                vboSize += cSz;
            }
            if (data->texcoords)
            {
                memcpy(t_buff + vboSize, data->texcoords + i, tcSz);
                vboSize += tcSz;
            }
        }
        m_vertexBuffer = new VertexBuffer(m_context, vboSize);
        m_vertexBuffer->setData(t_buff, vboSize);
        free(t_buff);
    }
    //Copy each vertex attrib to subBuffer location
    //Store VBO is VADs
    //positions.vbo = vbo;
    //if (data->normals)
    //    normals.vbo = vbo;
    //if (data->colors)
    //    colors.vbo = vbo;
    //if (data->texcoords)
    //    texcoords.vbo = vbo;

    //Build Index Buffer
    m_indexBuffer = new IndexBuffer(m_context, vboSize);
    m_indexBuffer->setData(data->faces, this->vfc.f * sizeof(unsigned int));

    //Inform shaders
    //for (unsigned int i = 0; i < data->materialsSize; ++i)
    //    if (auto s = data->materials[i]->getShaders())
    //    {
    //        s->setPositionsAttributeDetail(positions);
    //        s->setNormalsAttributeDetail(normals);
    //        s->setColorsAttributeDetail(colors);
    //        s->setTexCoordsAttributeDetail(texcoords);
    //        s->setFaceVBO(fbo);
    //    }
}

void Model::render(vk::CommandBuffer &cb, GraphicsPipeline &pipeline)
{
#if _DEBUG
    static bool aborted = false;
    if (!this->root)
    {
        if (!aborted)
            fprintf(stderr, "Model '%s' was not found, render aborted!\n", this->modelPath);
        aborted = true;
        return;
    }
#endif
    //Bind to pipeline
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.Pipeline());
    auto layout = pipeline.Layout().get();
    //cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, { m_descriptorSets }, {});//We might require this if we start doing model transforms in the shader with bones
    VkDeviceSize offsets[] = { 0 };
    cb.bindVertexBuffers(0, 1, m_vertexBuffer->getPtr(), offsets);
    cb.bindIndexBuffer(m_indexBuffer->get(), 0, vk::IndexType::eUint32);

    //Prepare all shaders to update dynamics
    //for (unsigned int i = 0; i < data->materialsSize; ++i)
 //       if (auto s = data->materials[i]->getShaders())
 //           s->prepare();
 //   Material::clearActive();
    //Recursive render
    glm::mat4 modelMat = glm::mat4(1);

    //Apply world transforms
    //Check we actually have a rotation (providing no axis == error)
    if ((this->rotation.x != 0 || this->rotation.y != 0 || this->rotation.z != 0) && this->rotation.w != 0)
        modelMat = glm::rotate(modelMat, glm::radians(this->rotation.w), glm::vec3(this->rotation));
    modelMat = glm::translate(modelMat, this->location);

    //Trigger recursive render
    root->render(cb, layout, modelMat);

    //for (unsigned int i = 0; i < data->materialsSize; ++i)
 //       if (auto s = data->materials[i]->getShaders())
 //           s->clearProgram();
}
vk::VertexInputBindingDescription Model::BindingDesc() const
{
    size_t size = 0;
    size += sizeof(*data->vertices);
    if(data->normals)
        size += sizeof(*data->normals);
    if (data->colors)
        size += sizeof(*data->colors);
    if (data->texcoords)
        size += sizeof(*data->texcoords);
    vk::VertexInputBindingDescription rtn;
    {
        rtn.binding = 0;
        rtn.stride = (unsigned int)size;
        rtn.inputRate = vk::VertexInputRate::eVertex; //1 item per vertex (alt is per instance)
    }
    return rtn;
}
std::vector<vk::VertexInputAttributeDescription> Model::AttributeDesc() const
{
    std::vector<vk::VertexInputAttributeDescription> rtn;
    unsigned int components = 0;
    unsigned int offset = 0;
    vk::VertexInputAttributeDescription viaDesc;
    {//Vertex
        viaDesc.binding = 0;
        viaDesc.location = components++;
        viaDesc.format = vk::Format::eR32G32B32Sfloat;    //RGB 32bit (signed) floating point
        viaDesc.offset = offset;        //Position of pos within Vertex
        offset += sizeof(*data->vertices);
        rtn.push_back(viaDesc);
    }
    if (data->normals)
    {
        viaDesc.binding = 0;
        viaDesc.location = components++;
        viaDesc.format = vk::Format::eR32G32B32Sfloat;    //RGB 32bit (signed) floating point
        viaDesc.offset = offset;        //Position of pos within Vertex
        offset += sizeof(*data->normals);
        rtn.push_back(viaDesc);
    }
    if (data->colors)
    {
        viaDesc.binding = 0;
        viaDesc.location = components++;
        viaDesc.format = vk::Format::eR32G32B32A32Sfloat;    //RGB 32bit (signed) floating point
        viaDesc.offset = offset;        //Position of pos within Vertex
        offset += sizeof(*data->colors);
        rtn.push_back(viaDesc);
    }
    if (data->texcoords)
    {
        viaDesc.binding = 0;
        viaDesc.location = components++;
        viaDesc.format = vk::Format::eR32G32Sfloat;    //RGB 32bit (signed) floating point
        viaDesc.offset = offset;        //Position of pos within Vertex
        offset += sizeof(*data->texcoords);
        rtn.push_back(viaDesc);
    }
    return rtn;
}
