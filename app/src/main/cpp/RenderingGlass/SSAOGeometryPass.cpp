//
// Created by gyp on 2025/9/8.
//
#include "utils.h"
#include "SSAOGeometryPass.h"
//#include "check.h"

SSAOGeometryPass::SSAOGeometryPass() : TemplatePass() {}

SSAOGeometryPass::~SSAOGeometryPass() {}

void SSAOGeometryPass::initShader() {
    std::vector<char> vertexShaderCode = readFileFromAssets("shaders/ssaoGeometry.vert");
    vertexShaderCode.push_back('\0');
    std::vector<char> fragmentShaderCode = readFileFromAssets("shaders/ssaoGeometry.frag");
    fragmentShaderCode.push_back('\0');
    mShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());

    vertexShaderCode = readFileFromAssets("shaders/ssaoGeometry.vert");
    vertexShaderCode.push_back('\0');
    fragmentShaderCode = readFileFromAssets("shaders/ssaoGeometryPosition.frag");
    fragmentShaderCode.push_back('\0');
    mPositionShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());

#if 0
    // configure g-buffer framebuffer
    // ------------------------------

    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    const unsigned int SCR_WIDTH = 1920;
    const unsigned int SCR_HEIGHT = 1200;
    // position color buffer
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    glDrawBuffers(3, attachments);
#elif 1
    glGenFramebuffers(1, &gBufferNormal);
    glBindFramebuffer(GL_FRAMEBUFFER, gBufferNormal);

    const unsigned int SCR_WIDTH = 1920;
    const unsigned int SCR_HEIGHT = 1200;

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gNormal, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenFramebuffers(1, &gBufferPosition);
    glBindFramebuffer(GL_FRAMEBUFFER, gBufferPosition);
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

//    unsigned int att0 = GL_COLOR_ATTACHMENT0;
//    glDrawBuffers(1, &att0);
#endif

}

void SSAOGeometryPass::render(const glm::mat4 &p, const glm::mat4 &v) {
}

bool SSAOGeometryPass::render(const glm::mat4 &p, const glm::mat4 &v, const glm::mat4 &m) {

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    // 1. geometry pass: render scene's geometry/color data into gbuffer
    // -----------------------------------------------------------------

    glBindFramebuffer(GL_FRAMEBUFFER, gBufferNormal);
    glEnable(GL_DEPTH_TEST);
    mShader.use();
    glm::mat4 model = glm::scale(m,glm::vec3 (0.001f));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat4("view", v);
    mShader.setUniformMat4("projection", p);
    mShader.setUniformBool("invertedNormals", false);
    mShader.setUniformBool("isNotInstanced", false);
    draw();
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, gBufferPosition);
    glEnable(GL_DEPTH_TEST);
    mPositionShader.use();
    mPositionShader.setUniformMat4("model", model);
    mPositionShader.setUniformMat4("view", v);
    mPositionShader.setUniformMat4("projection", p);
    mPositionShader.setUniformBool("invertedNormals", false);
    mPositionShader.setUniformBool("isNotInstanced", false);
    draw();
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

    return true;
}

void SSAOGeometryPass::draw() {
    GL_CALL(glFrontFace(GL_CCW));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_CULL_FACE));
//    GL_CALL(glEnable(GL_DEPTH_TEST));
//    GL_CALL(glEnable(GL_BLEND));
//    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    for (auto &it : *mMeshes) {
        it.second.drawShadowMap(mShader);
    }
}
void SSAOGeometryPass::renderSphere()
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

static inline bool isDepthFormat(GLenum fmt) {
    return fmt == GL_DEPTH_COMPONENT16 ||
           fmt == GL_DEPTH_COMPONENT24 ||
           fmt == GL_DEPTH_COMPONENT32F;
}

static inline bool isStencilFormat(GLenum fmt) {
    return fmt == GL_STENCIL_INDEX8;
}

static inline bool isDepthStencilFormat(GLenum fmt) {
    return fmt == GL_DEPTH24_STENCIL8 ||
           fmt == GL_DEPTH32F_STENCIL8;
}

// 检查给定内部格式的 renderbuffer 是否可用于帧缓冲附件
// 返回 true 表示可用，false 表示不支持或不可用
bool SSAOGeometryPass::IsRenderbufferFormatSupported(GLenum internalFormat) {
    // 备份当前绑定
    GLint prevRbo = 0, prevFbo = 0, prevDrawFbo = 0, prevReadFbo = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRbo);
#if defined(GL_DRAW_FRAMEBUFFER_BINDING) && defined(GL_READ_FRAMEBUFFER_BINDING)
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
#else
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
#endif

    GLuint rbo = 0, fbo = 0;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);

    // 使用一个很小的尺寸即可探测
    const GLsizei W = 8, H = 8;
    glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, W, H);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        // 清理并恢复
        glBindRenderbuffer(GL_RENDERBUFFER, prevRbo);
        glDeleteRenderbuffers(1, &rbo);
#if defined(GL_DRAW_FRAMEBUFFER_BINDING) && defined(GL_READ_FRAMEBUFFER_BINDING)
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFbo);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
#endif
        return false;
    }

    glGenFramebuffers(1, &fbo);
#if defined(GL_DRAW_FRAMEBUFFER)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
#endif

    // 选择正确的附件类型
    if (isDepthStencilFormat(internalFormat)) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    } else if (isDepthFormat(internalFormat)) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    } else if (isStencilFormat(internalFormat)) {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    } else {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
        // 可选：在 GLES3 下设置 draw buffer，避免实现依赖
#if defined(GL_MAX_COLOR_ATTACHMENTS)
        const GLenum buf = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &buf);
#endif
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    bool ok = (status == GL_FRAMEBUFFER_COMPLETE);

    // 清理与恢复绑定
    glDeleteFramebuffers(1, &fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, prevRbo);
    glDeleteRenderbuffers(1, &rbo);
#if defined(GL_DRAW_FRAMEBUFFER_BINDING) && defined(GL_READ_FRAMEBUFFER_BINDING)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFbo);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
#endif

    return ok;
}