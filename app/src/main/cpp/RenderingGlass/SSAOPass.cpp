//
// Created by gyp on 2025/9/8.
//

#include "SSAOPass.h"
#include "utils.h"
#include "iostream"
#include "renderPassManager.h"
#include "SSAOGeometryPass.h"
#include <random>

SSAOPass::SSAOPass() : TemplatePass() {}
SSAOPass::~SSAOPass() {}
void SSAOPass::initShader() {
    std::vector<char> vertexShaderCode = readFileFromAssets("shaders/ssao.vert");
    vertexShaderCode.push_back('\0');
    std::vector<char> fragmentShaderCode = readFileFromAssets("shaders/ssaoLighting.frag");
    fragmentShaderCode.push_back('\0');
    mSSAOLightingShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());


    vertexShaderCode = readFileFromAssets("shaders/ssao.vert");
    vertexShaderCode.push_back('\0');
    fragmentShaderCode = readFileFromAssets("shaders/ssao.frag");
    fragmentShaderCode.push_back('\0');
    mSSAOShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());

    vertexShaderCode = readFileFromAssets("shaders/ssao.vert");
    vertexShaderCode.push_back('\0');
    fragmentShaderCode = readFileFromAssets("shaders/ssaoBlur.frag");
    fragmentShaderCode.push_back('\0');
    mSSAOBlurShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());

//    glGetIntegerv(GL_VIEWPORT, mCurViewport);
//    const unsigned int SCR_WIDTH = mCurViewport[2];
//    const unsigned int SCR_HEIGHT = mCurViewport[3];
    const unsigned int SCR_WIDTH = 1920;
    const unsigned int SCR_HEIGHT = 1200;
    // also create framebuffer to hold SSAO processing stage
    // -----------------------------------------------------
    glGenFramebuffers(1, &ssaoFBO);  glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    // SSAO color buffer
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    unsigned int att0 = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &att0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Framebuffer not complete!" << std::endl;
    // and blur stage
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    glDrawBuffers(1, &att0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // generate sample kernel
    // ----------------------
    std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 8; ++i)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / 8.0f;

        // scale samples s.t. they're more aligned to center of kernel
        scale = ourLerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    // generate noise texture
    // ----------------------

    for (unsigned int i = 0; i < 16; i++)
    {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);



    // shader configuration
    // --------------------
    mSSAOLightingShader.use();
    mSSAOLightingShader.setUniformInt("gPosition", 0);
    mSSAOLightingShader.setUniformInt("gNormal", 1);
    mSSAOLightingShader.setUniformInt("gAlbedo", 2);
    mSSAOLightingShader.setUniformInt("ssao", 3);
    mSSAOShader.use();
    mSSAOShader.setUniformInt("gPosition", 0);
    mSSAOShader.setUniformInt("gNormal", 1);
    mSSAOShader.setUniformInt("texNoise", 2);
    mSSAOBlurShader.use();
    mSSAOBlurShader.setUniformInt("ssaoInput", 0);

#if 0
    std::vector<char> vertexShaderCode1 = readFileFromAssets("shaders/debug.vert");
    vertexShaderCode1.push_back('\0');
    std::vector<char> fragmentShaderCode1 = readFileFromAssets("shaders/debug.frag");
    fragmentShaderCode1.push_back('\0');
    mDebugShader.loadShader(vertexShaderCode1.data(), fragmentShaderCode1.data());
    unsigned int att0 = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &att0);
    mDebugShader.use();
    mDebugShader.setUniformInt("gNormal", 0);
    mDebugShader.setUniformInt("gPosition", 1);
//    mDebugShader.setUniformInt("depthMap", 1);
    glUseProgram(0);
#endif
}

bool SSAOPass::render(const glm::mat4 &p, const glm::mat4 &v, const glm::mat4 &m) {
#if 0
    GL_CALL(glDisable(GL_DEPTH_TEST));
    debugRender();
#endif
    // 2. generate SSAO texture
    // ------------------------
//    glViewport(0, 0, 1920, 1200);
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        infof("Framebuffer not complete! status: %x", status);
    }
//    glClear(GL_COLOR_BUFFER_BIT);
    GL_CALL(glDisable(GL_DEPTH_TEST));
    mSSAOShader.use();
    // Send kernel + rotation
    for (unsigned int i = 0; i < 8; ++i)
        mSSAOShader.setUniformVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
    mSSAOShader.setUniformMat4("projection", p);

    auto& passManager = RenderPassManager::getInstance();
    auto geometryPass = passManager.getPassAs<SSAOGeometryPass>("SSAOGeometry");
    GLuint gPosition = geometryPass->getGPosition();
    GLuint gNormal   = geometryPass->getGNormal();
    GLuint gAlbedo   = geometryPass->getGAlbedo();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    renderQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

//    // 恢复默认纹理单元
//    glActiveTexture(GL_TEXTURE0);
//    // 恢复默认程序
//    glUseProgram(0);
    // 3. blur SSAO texture to remove noise
    // ------------------------------------
//    glViewport(0, 0, 1920, 1200);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    mSSAOBlurShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    renderQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 恢复默认纹理单元
    glActiveTexture(GL_TEXTURE0);
    // 恢复默认程序
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    // 4. lighting pass: traditional deferred Blinn-Phong lighting with added screen-space ambient occlusion
    // -----------------------------------------------------------------------------------------------------
#if 0
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mSSAOLightingShader.use();
    // send light relevant uniforms
    glm::vec3 lightPosView = glm::vec3(v * glm::vec4(lightPos, 1.0));
    mSSAOLightingShader.setUniformVec3("light.Position", lightPosView);
    mSSAOLightingShader.setUniformVec3("light.Color", lightColor);
    // Update attenuation parameters
    const float linear    = 0.09f;
    const float quadratic = 0.032f;
    mSSAOLightingShader.setUniformFloat("light.Linear", linear);
    mSSAOLightingShader.setUniformFloat("light.Quadratic", quadratic);
    mSSAOLightingShader.setUniformMat4("projection", p);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
//    glActiveTexture(GL_TEXTURE2);
//    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glActiveTexture(GL_TEXTURE3); // add extra SSAO texture to lighting pass
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    renderQuad();
    // 恢复默认纹理单元
    glActiveTexture(GL_TEXTURE0);
    // 恢复默认程序
    glUseProgram(0);
#endif

    return true;
}
// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
void SSAOPass::renderQuad()
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

void SSAOPass::draw() {

}

void SSAOPass::render(const glm::mat4 &p, const glm::mat4 &v) {

}
void SSAOPass::debugRender(){
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    unsigned int att0 = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &att0);
//    glBindFramebuffer(GL_FRAMEBUFFER, 0); // 返回默认
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    auto& passManager = RenderPassManager::getInstance();
    auto geometryPass = passManager.getPassAs<SSAOGeometryPass>("SSAOGeometry");
    GLuint gPosition = geometryPass->getGPosition();
    GLuint gNormal = geometryPass->getGNormal();
//    GLuint depthMap = geometryPass->getDepthMap();
//    GLuint gAlbedo = geometryPass->getGAlbedo();
    mDebugShader.use();
    int mSavedViewport[4] = {0, 0, 0, 0}; // x, y, width, height
    glGetIntegerv(GL_VIEWPORT, mSavedViewport);
//    glViewport(0, 0, 1920, 1200);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gPosition);
//    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_2D, depthMap);
//    GLint prevFBO = 0;
//    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    renderQuad();
    glUseProgram(0);
//    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}

unsigned int SSAOPass::getSsaoColorBuffer() const {
    return ssaoColorBuffer;
}
