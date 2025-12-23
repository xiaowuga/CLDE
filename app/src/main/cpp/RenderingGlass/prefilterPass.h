//
// Created by gyp on 2025/7/7.
//
#pragma once

#include "templatePass.h"

class PrefilterPass : public TemplatePass {
public:
    PrefilterPass();
    virtual ~PrefilterPass();


    virtual bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) override;
    virtual void render(const glm::mat4& p, const glm::mat4& v);
    virtual void initShader() override;
    virtual void draw() override;

    unsigned int getPrefilterMap() const {return mPrefilterMap;}
    void setPrefilterMap(int i) {mPrefilterMap = mPrefilterMaps[i];}

protected:
private:
    void renderCube();
    unsigned int mPrefilterMaps[2];
    unsigned int mPrefilterMap;
    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
};
