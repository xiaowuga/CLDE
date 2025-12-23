//
// Created by gyp on 2025/7/7.
//
#pragma once

#include "templatePass.h"

class EquirectangularToCubemapPass : public TemplatePass {
public:
    EquirectangularToCubemapPass();
    virtual ~EquirectangularToCubemapPass();


    virtual bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) override;
    virtual void render(const glm::mat4& p, const glm::mat4& v);
    virtual void initShader() override;
    virtual void draw() override;
    unsigned int* getCaptureFBO() { return &mCaptureFBO;}
    unsigned int* getCaptureRbo() {return &mCaptureRBO;}
    unsigned int* getEnvCubemap() {return &mEnvCubemap;}
    const glm::mat4 &getCaptureProjection() const {return mCaptureProjection;}
    const glm::mat4 *getCaptureViews() const {return mCaptureViews;}
    void setTextureIndex(unsigned int i){textureIndex = i;}
    void setEnvCubeMap(int i){mEnvCubemap = mEnvCubemaps[i];}
    // 获取指定索引的Cubemap
    GLuint getEnvCubemapByIndex(int index) const {
        if (index >= 0 && index < 2)
            return mEnvCubemaps[index];
        return 0;
    }

protected:
private:
    void renderCube();
    unsigned int hdrTextures[2];
    GLuint mEnvCubemaps[2];         // 两个Cubemap纹理
    unsigned int textureIndex = 1;
    unsigned int mCaptureFBO = 0;
    unsigned int mCaptureRBO = 0;
    unsigned int mEnvCubemap = 0;

    glm::mat4 mCaptureProjection;
    glm::mat4 mCaptureViews[6];
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;

};
