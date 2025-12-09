//
// Created by gyp on 2025/7/7.
//
#pragma once

#include "templatePass.h"

class IrradiancePass : public TemplatePass {
public:
    IrradiancePass();
    virtual ~IrradiancePass();


    virtual bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) override;
    virtual void render(const glm::mat4& p, const glm::mat4& v);
    virtual void initShader() override;
    virtual void draw() override;
    unsigned int getIrradianceMap() const {return mIrradianceMap;}
    void setIrradianceMap(int i) {mIrradianceMap = mIrradianceMaps[i];}
protected:
private:
    void renderCube();
    GLuint mIrradianceMaps[2];  // 两个Irradiance贴图
    unsigned int mIrradianceMap;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
};
