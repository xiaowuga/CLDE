//
// Created by gyp on 2025/9/8.
//
#pragma once
#include "templatePass.h"



class SSAOGeometryPass : public TemplatePass{
public:
    SSAOGeometryPass();
    virtual ~SSAOGeometryPass();
    virtual bool render(const glm::mat4 &p, const glm::mat4 &v, const glm::mat4 &m) override;

    virtual void render(const glm::mat4 &p, const glm::mat4 &v) override;

    unsigned int getGPosition() const {return gPosition;}
    unsigned int getGNormal() const {return gNormal;}
    unsigned int getGAlbedo() const {return gAlbedo;}
protected:


    virtual void initShader() override;

protected:
    virtual void draw() override;
private:
    renderShader mPositionShader,mDepthShader;
    unsigned int gBufferNormal;
    unsigned int gBufferPosition;
    unsigned int gPosition;
    unsigned int gNormal,depthMap;
    unsigned int gBufferDepth;
    unsigned int depthTextureNormal,depthTexturePosition;
    unsigned int gAlbedo;
    unsigned int rboDepth;
    int mCurViewport[4] = {0, 0, 0, 0}; // x, y, width, height
    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    //debug
    void renderSphere();
    void drawPosition();
    void drawNormal();
public:
    unsigned int getDepthMap() const {
        return depthMap;
    }

private:
    unsigned int sphereVAO = 0;
    unsigned int indexCount;
    bool IsRenderbufferFormatSupported(GLenum internalFormat);
};
