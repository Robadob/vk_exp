#ifndef __Bone_h__
#define __Bone_h__
#include <glm/mat4x2.hpp>
#include <vector>

//TODO
class Bone
{
public:
    Bone(glm::mat4 om)
        :offsetMatrix(om)
    { }
private:
    glm::mat4 offsetMatrix;
    //struct VWPair
    //{
    //    unsigned int vertex;
    //    float weight;
    //};
    //std::vector<VWPair> vertexWeights;
};
#endif //__Bone_h__