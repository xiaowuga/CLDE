//
// Created by gyp on 2025/7/9.
//

#include "RenderingGlass/shadowMappingDepthPass.h"
#include "utils.h"
#include "RenderingGlass/renderPassManager.h"
#include "RenderingGlass/pbrPass.h"

ShadowMappingDepthPass::ShadowMappingDepthPass(): TemplatePass() {}

ShadowMappingDepthPass::~ShadowMappingDepthPass() {}





void ShadowMappingDepthPass::initShader() {
    std::vector<char> vertexShaderCode = readFileFromAssets("shaders/shadowMappingDepth.vert");
    vertexShaderCode.push_back('\0');
    std::vector<char> fragmentShaderCode = readFileFromAssets("shaders/shadowMappingDepth.frag");
    fragmentShaderCode.push_back('\0');
    mShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());

    //测试用平面VBO、VAO
    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float planeVertices[] = {
            // positions            // normals         // texcoords
            25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
            -25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
            -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

            25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
            -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
            25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
    };
    // plane VAO
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    lightPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::mat4 lightmodel = glm::mat4(1.0f);
#if ENABLE_SPHERE_SCENE
    //球的光源位置
    lightmodel = glm::translate(lightmodel, glm::vec3(0.0f, 5.0f, 5.0f));
#else
    //仪表盘光源位置
//    lightmodel = glm::translate(lightmodel, glm::vec3(0.0f, 50.0f, 1.0f));
    lightmodel = glm::translate(lightmodel, glm::vec3(0.0f, 80.0f, -40.0f));
#endif
//    lightPos = glm::vec3(0.0f, 0.0f, 0.0f);
    lightPos = glm::vec3(lightmodel * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    // configure depth map FBO
    // 生成帧缓冲和纹理
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // 使用GL_CLAMP_TO_EDGE代替GL_CLAMP_TO_BORDER
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 将深度纹理附加为FBO的深度缓冲区
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);

    // 检查帧缓冲区是否完整
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // 处理错误
        printf("error");
    }
    // 解除绑定
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

//    //可视化shadowmap shader
    std::vector<char> debugVertexShaderCode = readFileFromAssets("shaders/debugShadowMap.vert");
    debugVertexShaderCode.push_back('\0');
    std::vector<char> debugFragmentShaderCode = readFileFromAssets("shaders/debugShadowMap.frag");
    debugFragmentShaderCode.push_back('\0');
    debugShadowMapShader.loadShader(debugVertexShaderCode.data(), debugFragmentShaderCode.data());
    debugShadowMapShader.use();
    debugShadowMapShader.setUniformInt("depthMap", 0);
    glUseProgram(0);




}
bool ShadowMappingDepthPass::render(const glm::mat4 &p, const glm::mat4 &v, const glm::mat4 &m) {
#if 0
    // render
    // ------
//    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. render depth of scene to texture (from light's perspective)
    // --------------------------------------------------------------



    //lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
#if ENABLE_SPHERE_SCENE
    lightProjection = glm::ortho(-40.0f, 40.0f, -20.0f, 60.0f, near_plane, far_plane);
    lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;
#else

    lightProjection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 87.0f, 91.0f);
//    lightPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::mat4 lightmodel = glm::mat4(1.0f);
    lightmodel = glm::translate(lightmodel, glm::vec3(0.0f, -10.0f, 10.0f));
//    lightPos = glm::vec3(0.0f,0.0f,0.0f);
//    lightView = glm::lookAt(glm::vec3(lightmodel*m*glm::vec4(lightPos,1.0f)), glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;
#endif

//    lightSpaceMatrix = lightProjection * v;
    glEnable(GL_DEPTH_TEST);

//    glFrontFace(GL_CW);
//    glCullFace(GL_BACK);
    // render scene from light's point of view
    mShader.use();
    mShader.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
#if ENABLE_SPHERE_SCENE
    renderScene(mShader);
#else
    // floor
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::mat4(1.0f);
    mShader.setUniformMat4("model", model);
    mShader.setUniformBool("isNotInstanced",true);
    glBindVertexArray(planeVAO);
//    glDrawArrays(GL_TRIANGLES, 0, 6);
    mShader.setUniformBool("isNotInstanced",false);
    model = glm::scale(m,glm::vec3 (0.001f));
    mShader.setUniformMat4("model", model);
    draw();
#endif
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO); // 恢复


#endif
    return true;
}
void ShadowMappingDepthPass::render(const glm::mat4 &p, const glm::mat4 &v) {
#if 1
    // render
    // ------
//    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. render depth of scene to texture (from light's perspective)
    // --------------------------------------------------------------



    //lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
#if ENABLE_SPHERE_SCENE
    lightProjection = glm::ortho(-40.0f, 40.0f, -20.0f, 60.0f, near_plane, far_plane);
#else
    lightProjection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, near_plane, far_plane);
#endif

    lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;
//    lightSpaceMatrix = lightProjection * v;
    glEnable(GL_DEPTH_TEST);

//    glFrontFace(GL_CW);
//    glCullFace(GL_BACK);
    // render scene from light's point of view
    mShader.use();
    mShader.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
#if ENABLE_SPHERE_SCENE
    renderScene(mShader);
#else
    // floor
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::mat4(1.0f);
    mShader.setUniformMat4("model", model);
    mShader.setUniformBool("isNotInstanced",true);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

#endif
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO); // 恢复


#endif
}
void ShadowMappingDepthPass::draw() {
    for (const auto &it : *mMeshes) {
        it.second.drawShadowMap(mShader);
    }
}
// renders the 3D scene
// --------------------
void ShadowMappingDepthPass::renderScene(const renderShader &shader)
{

    glm::mat4 model = glm::mat4(1.0f);
//    auto& passManager = RenderPassManager::getInstance();
//    auto pbrPass = passManager.getPassAs<PbrPass>("pbr");

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-5.0, 2.0, -15.0));
    shader.setUniformMat4("model", model);
    shader.setUniformBool("isNotInstanced",true);
    renderSphere();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-3.0, 2.0, -15.0));
    shader.setUniformMat4("model", model);
    shader.setUniformBool("isNotInstanced",true);
    renderSphere();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0, 2.0, -15.0));
    shader.setUniformMat4("model", model);
    shader.setUniformBool("isNotInstanced",true);
    renderSphere();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(1.0, 2.0, -15.0));
    shader.setUniformMat4("model", model);
    shader.setUniformBool("isNotInstanced",true);
    renderSphere();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(3.0, 2.0, -15.0));
    shader.setUniformMat4("model", model);
    shader.setUniformBool("isNotInstanced",true);
    renderSphere();
    // floor
    model = glm::mat4(1.0f);
    shader.setUniformMat4("model", model);
    shader.setUniformBool("isNotInstanced",true);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

}

void ShadowMappingDepthPass::debugShadowMapRender(){
    // render Depth map to quad for visual debugging
    // ---------------------------------------------
    debugShadowMapShader.use();
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    debugShadowMapShader.setUniformFloat("near_plane", near_plane);
    debugShadowMapShader.setUniformFloat("far_plane", far_plane);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    renderQuad();
    glUseProgram(0);
}
// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
void ShadowMappingDepthPass::renderQuad()
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

// renders (and builds at first invocation) a sphere
// -------------------------------------------------

void ShadowMappingDepthPass::renderSphere()
{
    if (sphereVAO == 0)
    {
        glGenVertexArrays(1, &sphereVAO);

        unsigned int vbo, ebo;
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359f;
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
        {
            for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
            {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                positions.push_back(glm::vec3(xPos, yPos, zPos));
                uv.push_back(glm::vec2(xSegment, ySegment));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        bool oddRow = false;
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            if (!oddRow) // even rows: y == 0, y == 2; and so on
            {
                for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                {
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            }
            else
            {
                for (int x = X_SEGMENTS; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
        indexCount = static_cast<GLsizei>(indices.size());

        std::vector<float> data;
        for (unsigned int i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            if (normals.size() > 0)
            {
                data.push_back(normals[i].x);
                data.push_back(normals[i].y);
                data.push_back(normals[i].z);
            }
            if (uv.size() > 0)
            {
                data.push_back(uv[i].x);
                data.push_back(uv[i].y);
            }
        }
        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
        unsigned int stride = (3 + 2 + 3) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
}
