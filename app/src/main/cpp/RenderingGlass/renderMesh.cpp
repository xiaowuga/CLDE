#include"renderMesh.h"
#include <stddef.h>
#include "common/gfxwrapper_opengl.h"
#include <android/log.h>

#define LOG_TAG "renderMesh"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

renderMesh::renderMesh(){

}

renderMesh::renderMesh(std::vector<renderVertex> vertices, std::vector<unsigned int> indices, std::vector<renderTexture> textures)
    : mVertices(vertices), mIndices(indices), mTextures(textures) {
    setupMesh();
}

renderMesh::renderMesh(std::vector<renderVertex> vertices, std::vector<unsigned int> indices, std::vector<renderTexture> textures,pbrMaterial pbrMaterial)
        : mVertices(vertices), mIndices(indices), mTextures(textures) ,mPbrMaterial(pbrMaterial) {
    setupMesh();
}

renderMesh::renderMesh(std::vector<renderVertex> vertices, std::vector<unsigned int> indices, std::vector<renderTexture> textures,pbrMaterial pbrMaterial,
           int transformNum,
           std::vector<float> transformVector)
        : mVertices(vertices), mIndices(indices), mTextures(textures), mPbrMaterial(pbrMaterial), mTransformNum(transformNum), mTransformVector(transformVector) {
    setupMesh();
}

renderMesh::~renderMesh() {
    if (mVAO != 0) glDeleteVertexArrays(1, &mVAO);
    if (mVBO != 0) glDeleteBuffers(1, &mVBO);
    if (mEBO != 0) glDeleteBuffers(1, &mEBO);
    if (mVBO_transform != 0) glDeleteBuffers(1, &mVBO_transform);
}

renderMesh::renderMesh(renderMesh&& other) noexcept
    : mVertices(std::move(other.mVertices)),
      mIndices(std::move(other.mIndices)),
      mTextures(std::move(other.mTextures)),
      mPbrMaterial(std::move(other.mPbrMaterial)),
      mTransformNum(other.mTransformNum),
      mTransformVector(std::move(other.mTransformVector)),
      mFramebuffer(other.mFramebuffer),
      mVAO(other.mVAO),
      mVBO(other.mVBO),
      mEBO(other.mEBO),
      mVBO_transform(other.mVBO_transform)
{
    other.mFramebuffer = 0;
    other.mVAO = 0;
    other.mVBO = 0;
    other.mEBO = 0;
    other.mVBO_transform = 0;
}

renderMesh& renderMesh::operator=(renderMesh&& other) noexcept {
    if (this != &other) {
        if (mVAO != 0) glDeleteVertexArrays(1, &mVAO);
        if (mVBO != 0) glDeleteBuffers(1, &mVBO);
        if (mEBO != 0) glDeleteBuffers(1, &mEBO);
        if (mVBO_transform != 0) glDeleteBuffers(1, &mVBO_transform);

        mVertices = std::move(other.mVertices);
        mIndices = std::move(other.mIndices);
        mTextures = std::move(other.mTextures);
        
        // Use copy assignment for pbrMaterial to ensure data is transferred correctly
        mPbrMaterial = other.mPbrMaterial;
        
        mTransformNum = other.mTransformNum;
        mTransformVector = std::move(other.mTransformVector);
        
        mFramebuffer = other.mFramebuffer;
        mVAO = other.mVAO;
        mVBO = other.mVBO;
        mEBO = other.mEBO;
        mVBO_transform = other.mVBO_transform;

        other.mFramebuffer = 0;
        other.mVAO = 0;
        other.mVBO = 0;
        other.mEBO = 0;
        other.mVBO_transform = 0;
        
        // Debug log
        // LOGI("renderMesh updated. VAO: %d, Albedo: %.2f %.2f %.2f", mVAO, mPbrMaterial.albedoValue.x, mPbrMaterial.albedoValue.y, mPbrMaterial.albedoValue.z);
    }
    return *this;
}

void renderMesh::setupMesh() {
    // Cleanup existing buffers if they exist (re-initialization case)
    if (mVAO != 0) { glDeleteVertexArrays(1, &mVAO); mVAO = 0; }
    if (mVBO != 0) { glDeleteBuffers(1, &mVBO); mVBO = 0; }
    if (mEBO != 0) { glDeleteBuffers(1, &mEBO); mEBO = 0; }
    if (mVBO_transform != 0) { glDeleteBuffers(1, &mVBO_transform); mVBO_transform = 0; }

    // create buffers/arrays
    //glGenFramebuffers(1, &mFramebuffer);
    //glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);
    // load data into vertex buffers
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    // A great thing about structs is that their memory layout is sequential for all its items.
    // The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
    // again translates to 3/2 floats which translates to a byte array.
    glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(renderVertex), &mVertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(unsigned int), &mIndices[0], GL_STATIC_DRAW);

    // set the vertex attribute pointers
    // vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(renderVertex), (void*)0);
    // vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(renderVertex), (void*)offsetof(renderVertex, Normal));
    // vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(renderVertex), (void*)offsetof(renderVertex, TexCoords));
    // vertex tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(renderVertex), (void*)offsetof(renderVertex, Tangent));
    // vertex bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(renderVertex), (void*)offsetof(renderVertex, Bitangent));
    // ids
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(renderVertex), (void*)offsetof(renderVertex, BoneIDs));
    // weights
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(renderVertex), (void*)offsetof(renderVertex, Weights));

    //transform
    glGenBuffers(1, &mVBO_transform);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO_transform);
    glBufferData(GL_ARRAY_BUFFER, mTransformNum * 16 * sizeof(float), mTransformVector.data(), GL_STATIC_DRAW);
//    glEnableVertexAttribArray(7);
//    glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
//    glVertexAttribDivisor(7, 1);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(7 + i);
        glVertexAttribPointer(7 + i, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float),
                              (void*)(i * 4 * sizeof(float))); // 每列偏移i*16字节
        glVertexAttribDivisor(7 + i, 1);
    }

    glBindVertexArray(0);
}

bool renderMesh::activeTexture(const std::string& textureName) {
    for (auto& it : mTextures) {
        if (textureName == it.path) {
            it.active = true;
        } else {
            it.active = false;
        }
    }
    return true;
}

void renderMesh::draw(renderShader& shader) {
    // bind appropriate textures
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;
    for (unsigned int i = 0; i < mTextures.size(); i++) {
        if (mTextures[i].active == false) {
            continue;
        }

        glActiveTexture(GL_TEXTURE0 + i); // active proper texture unit before binding
        // retrieve texture number (the N in diffuse_textureN)
        std::string number;
        std::string name = mTextures[i].type;
        if (name == "texture_diffuse") {
            number = std::to_string(diffuseNr++);
        }
        else if (name == "texture_specular") {
            number = std::to_string(specularNr++); // transfer unsigned int to string
        }
        else if (name == "texture_normal") {
            number = std::to_string(normalNr++); // transfer unsigned int to string
        }
        else if (name == "texture_height") {
            number = std::to_string(heightNr++); // transfer unsigned int to string
        }

        // now set the sampler to the correct texture unit
        glUniform1i(glGetUniformLocation(shader.id(), (name + number).c_str()), i);
        // and finally bind the texture
        glBindTexture(GL_TEXTURE_2D, mTextures[i].id);
    }

    // draw mesh
    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(mIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // always good practice to set everything back to defaults once configured.
    glActiveTexture(GL_TEXTURE0);
}
void renderMesh::drawShadowMap(renderShader& shader) const{

    // 绘制网格
    glBindVertexArray(mVAO);
    //TODO: LiZiRui: 后面可以用实例化绘制优化
//    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(mIndices.size()), GL_UNSIGNED_INT, 0);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(mIndices.size()), GL_UNSIGNED_INT, 0, mTransformNum);
    glBindVertexArray(0);
}
void renderMesh::drawPBR(renderShader& shader) {

    // 直接使用pbrMaterial中的纹理ID和材质参数
    // albedo处理
    if (mPbrMaterial.useAlbedoMap && mPbrMaterial.albedoMapId != 0) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, mPbrMaterial.albedoMapId);
        shader.setUniformInt("albedoMap", mPbrMaterial.albedoMapId);
        shader.setUniformBool("useAlbedoMap", true);
    } else {
        shader.setUniformBool("useAlbedoMap", false);
        shader.setUniformVec3("albedoValue", mPbrMaterial.albedoValue);
    }

    // normal处理
    if (mPbrMaterial.useNormalMap && mPbrMaterial.normalMapId != 0) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, mPbrMaterial.normalMapId);
        shader.setUniformInt("normalMap", mPbrMaterial.normalMapId);
        shader.setUniformBool("useNormalMap", true);
    } else {
        shader.setUniformBool("useNormalMap", false);
        shader.setUniformVec3("normalValue", mPbrMaterial.normalValue);
    }

    // metallic处理
    if (mPbrMaterial.useMetallicMap && mPbrMaterial.metallicMapId != 0) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, mPbrMaterial.metallicMapId);
        shader.setUniformInt("metallicMap", mPbrMaterial.metallicMapId);
        shader.setUniformBool("useMetallicMap", true);
    } else {
        shader.setUniformBool("useMetallicMap", false);
        shader.setUniformFloat("metallicValue", mPbrMaterial.metallicValue);
    }

    // roughness处理
    if (mPbrMaterial.useRoughnessMap && mPbrMaterial.roughnessMapId != 0) {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, mPbrMaterial.roughnessMapId);
        shader.setUniformInt("roughnessMap", mPbrMaterial.roughnessMapId);
        shader.setUniformBool("useRoughnessMap", true);
    } else {
        shader.setUniformBool("useRoughnessMap", false);
        shader.setUniformFloat("roughnessValue", mPbrMaterial.roughnessValue);
    }

    // ao处理
    if (mPbrMaterial.useAoMap && mPbrMaterial.aoMapId != 0) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, mPbrMaterial.aoMapId);
        shader.setUniformInt("aoMap", mPbrMaterial.aoMapId);
        shader.setUniformBool("useAoMap", true);
    } else {
        shader.setUniformBool("useAoMap", false);
        shader.setUniformFloat("aoValue", mPbrMaterial.aoValue);
    }

    // 绘制网格
    glBindVertexArray(mVAO);
    //TODO: LiZiRui: 后面可以用实例化绘制优化
//    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(mIndices.size()), GL_UNSIGNED_INT, 0);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(mIndices.size()), GL_UNSIGNED_INT, 0, mTransformNum);
    glBindVertexArray(0);

//    // 解绑所有纹理
//    for (unsigned int i = 0; i < 8; i++) {
//        glActiveTexture(GL_TEXTURE0 + i);
//        glBindTexture(GL_TEXTURE_2D, 0);
//    }
//
//    // 恢复默认状态
//    glActiveTexture(GL_TEXTURE0);
}