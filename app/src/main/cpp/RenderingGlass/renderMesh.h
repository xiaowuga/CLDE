#pragma once
#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtc/type_ptr.hpp>
#include "renderShader.h"
#include "RenderingGlass/pbrMaterial.h" // 包含pbrMaterial的定义

#define MAX_BONE_INFLUENCE 4

struct renderVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    int BoneIDs[MAX_BONE_INFLUENCE];
    float Weights[MAX_BONE_INFLUENCE];
    renderVertex() {
        for (int i = 0 ; i < MAX_BONE_INFLUENCE; i++) {
            BoneIDs[i] = -1;
            Weights[i] = 0;
        }
    };
};

struct renderTexture {
    uint32_t id;
    std::string type;
    std::string path;
    bool active;
};

class renderMesh {
public:
    renderMesh();

    renderMesh(std::vector<renderVertex> vertices, std::vector<unsigned int> indices, std::vector<renderTexture> textures);
    renderMesh(std::vector<renderVertex> vertices, std::vector<unsigned int> indices, std::vector<renderTexture> textures,pbrMaterial pbrMaterial);
    renderMesh(std::vector<renderVertex> vertices, std::vector<unsigned int> indices,
         std::vector<renderTexture> textures, pbrMaterial pbrMaterial, int transformNum,
         std::vector<float> transformVector);


//     Destructor to clean up resources
    ~renderMesh();

//     Disable copying to prevent double-free of OpenGL resources
    renderMesh(const renderMesh&) = delete;
    renderMesh& operator=(const renderMesh&) = delete;

    // Enable moving
    renderMesh(renderMesh&& other) noexcept;
    renderMesh& operator=(renderMesh&& other) noexcept;

    void draw(renderShader& shader);
    void drawPBR(renderShader& shader);
    void drawShadowMap(renderShader& shader) const;

    bool activeTexture(const std::string &textureName);
//private:
public:
    void setupMesh();
//private:
public:
    std::vector<renderVertex>       mVertices;
    std::vector<unsigned int> mIndices;
    std::vector<renderTexture>      mTextures;
    pbrMaterial mPbrMaterial;
    int mTransformNum;
    std::vector<float> mTransformVector;

    unsigned int mFramebuffer;
    unsigned int mVAO;
    unsigned int mVBO;
    unsigned int mEBO;
    unsigned int mVBO_transform;
};