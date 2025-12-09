//
// Created by gyp on 2025/7/7.
//

#include "RenderingGlass/brdfPass.h"
#include "utils.h"
#include "RenderingGlass/equirectangularToCubemapPass.h"
#include "RenderingGlass/renderPassManager.h"

BrdfPass::BrdfPass() : TemplatePass() {}

BrdfPass::~BrdfPass() {}

bool BrdfPass::render(const glm::mat4 &p, const glm::mat4 &v, const glm::mat4 &m) {
//    return TemplatePass::render(p, v, m);
    return true;
}

void BrdfPass::render(const glm::mat4 &p, const glm::mat4 &v) {
}

void BrdfPass::initShader() {
    std::vector<char> vertexShaderCode = readFileFromAssets("shaders/brdf.vert");
    vertexShaderCode.push_back('\0');
    std::vector<char> fragmentShaderCode = readFileFromAssets("shaders/brdf.frag");
    fragmentShaderCode.push_back('\0');
    mShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());


    auto& passManager = RenderPassManager::getInstance();
    auto equiPass = passManager.getPassAs<EquirectangularToCubemapPass>("equirectangularToCubemap");
    GLuint* captureFBO = equiPass->getCaptureFBO();
    GLuint* captureRBO = equiPass->getCaptureRbo();
    GLuint* envCubemap = equiPass->getEnvCubemap();
    glm::mat4 captureProjection = equiPass->getCaptureProjection();
    const glm::mat4* captureViews = equiPass->getCaptureViews();
    // pbr: generate a 2D LUT from the BRDF equations used.
    // ----------------------------------------------------
    glGenTextures(1, &mBrdfLUTTexture);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, mBrdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, *captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, *captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBrdfLUTTexture, 0);

    glViewport(0, 0, 512, 512);
    mShader.use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

}

void BrdfPass::draw() {
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------

void BrdfPass::renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}