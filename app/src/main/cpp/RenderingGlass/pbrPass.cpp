#include "pbrPass.h"
#include "utils.h"
#include "renderPassManager.h"
#include "equirectangularToCubemapPass.h"
#include "irradiancePass.h"
#include "prefilterPass.h"
#include "brdfPass.h"
#include "shadowMappingDepthPass.h"
#include "SSAOPass.h"

#define ENABLE_SSAO 1
PbrPass::PbrPass() : TemplatePass() {}

PbrPass::~PbrPass() {}


void PbrPass::initShader() {
    // 加载PBR专用shader
    std::vector<char> vertexShaderCode = readFileFromAssets("shaders/pbr.vert");
    vertexShaderCode.push_back('\0');
    std::vector<char> fragmentShaderCode = readFileFromAssets("shaders/pbr.frag");
    fragmentShaderCode.push_back('\0');
    mShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());

    mShader.use();
    mShader.setUniformInt("irradianceMap", 0);
    mShader.setUniformInt("prefilterMap", 1);
    mShader.setUniformInt("brdfLUT", 2);
    mShader.setUniformInt("albedoMap", 3);
    mShader.setUniformInt("normalMap", 4);
    mShader.setUniformInt("metallicMap", 5);
    mShader.setUniformInt("roughnessMap", 6);
    mShader.setUniformInt("aoMap", 7);

    mShader.setUniformInt("shadowMap", 8);

    // load PBR material textures
    // --------------------------
//    albedo    = TextureFromFileAssets("textures/pbr/rusted_iron/albedo.png","");
//    normal    = TextureFromFileAssets("textures/pbr/rusted_iron/normal.png","");
//    metallic  = TextureFromFileAssets("textures/pbr/rusted_iron/metallic.png","");
//    roughness = TextureFromFileAssets("textures/pbr/rusted_iron/roughness.png","");
//    ao        = TextureFromFileAssets("textures/pbr/rusted_iron/ao.png","");


    // load PBR material textures
    // --------------------------
    // rusted iron
    ironAlbedoMap =    TextureFromFileAssets("textures/pbr/rusted_iron/albedo.png","");
    ironNormalMap =    TextureFromFileAssets("textures/pbr/rusted_iron/normal.png","");
    ironMetallicMap =  TextureFromFileAssets("textures/pbr/rusted_iron/metallic.png","");
    ironRoughnessMap = TextureFromFileAssets("textures/pbr/rusted_iron/roughness.png","");
    ironAOMap =        TextureFromFileAssets("textures/pbr/rusted_iron/ao.png","");
    // gol
    goldAlbedoMap =    TextureFromFileAssets("textures/pbr/gold/albedo.png","");
    goldNormalMap =    TextureFromFileAssets("textures/pbr/gold/normal.png","");
    goldMetallicMap =  TextureFromFileAssets("textures/pbr/gold/metallic.png","");
    goldRoughnessMap = TextureFromFileAssets("textures/pbr/gold/roughness.png","");
    goldAOMap =        TextureFromFileAssets("textures/pbr/gold/ao.png","");
    // gras
    grassAlbedoMap =   TextureFromFileAssets("textures/pbr/grass/albedo.png","");
    grassNormalMap =   TextureFromFileAssets("textures/pbr/grass/normal.png","");
    grassMetallicMap = TextureFromFileAssets("textures/pbr/grass/metallic.png","");
    grassRoughnessMap =TextureFromFileAssets("textures/pbr/grass/roughness.png","");
    grassAOMap =       TextureFromFileAssets("textures/pbr/grass/ao.png","");

    // plastic
    plasticAlbedoMap =     TextureFromFileAssets("textures/pbr/plastic/albedo.png","");
    plasticNormalMap =     TextureFromFileAssets("textures/pbr/plastic/normal.png","");
    plasticMetallicMap =   TextureFromFileAssets("textures/pbr/plastic/metallic.png","");
    plasticRoughnessMap =  TextureFromFileAssets("textures/pbr/plastic/roughness.png","");
    plasticAOMap =         TextureFromFileAssets("textures/pbr/plastic/ao.png","");

    // wall
    wallAlbedoMap =        TextureFromFileAssets("textures/pbr/wall/albedo.png","");
    wallNormalMap =        TextureFromFileAssets("textures/pbr/wall/normal.png","");
    wallMetallicMap =      TextureFromFileAssets("textures/pbr/wall/metallic.png","");
    wallRoughnessMap =     TextureFromFileAssets("textures/pbr/wall/roughness.png","");
    wallAOMap =            TextureFromFileAssets("textures/pbr/wall/ao.png","");

    // lights
    // ------

    lightPositions[0] = glm::vec3(-10.0f,  10.0f, 10.0f);
    lightPositions[1] = glm::vec3( 10.0f,  10.0f, 10.0f);
    lightPositions[2] = glm::vec3(-10.0f, 20.0f, 10.0f);
    lightPositions[3] = glm::vec3( 10.0f, 20.0f, 10.0f);


    lightColors[0] = glm::vec3(300.0f, 300.0f, 300.0f);
    lightColors[1] = glm::vec3(300.0f, 300.0f, 300.0f);
    lightColors[2] = glm::vec3(300.0f, 300.0f, 300.0f);
    lightColors[3] = glm::vec3(300.0f, 300.0f, 300.0f);
    glUseProgram(0);
}

void PbrPass::draw() {
    GL_CALL(glFrontFace(GL_CCW));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_CULL_FACE));
//    GL_CALL(glEnable(GL_DEPTH_TEST));
//    GL_CALL(glEnable(GL_BLEND));
//    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    for (auto &it : *mMeshes) {
        it.second.drawPBR(mShader);
    }
}

bool PbrPass::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) {

#if ENABLE_DEBUG_SHADOWMAP
    auto& passManager = RenderPassManager::getInstance();
    passManager.restoreViewport();
    auto shadowPass = passManager.getPassAs<ShadowMappingDepthPass>("shadowMappingDepth");
    // debug shadow map
    shadowPass->debugShadowMapRender();
#else
    auto& passManager = RenderPassManager::getInstance();
    passManager.restoreViewport();
    // render
    // ------
    // render scene, supplying the convoluted irradiance map to the final shader.
    // ------------------------------------------------------------------------------------------
    glEnable(GL_DEPTH_TEST);
//    glFrontFace(GL_CW);
//    glCullFace(GL_BACK);
    mShader.use();
    mShader.setUniformMat4("projection", p);
//    glm::mat4 model = glm::mat4(1.0f);
//    glm::mat4 view = camera.GetViewMatrix();
    mShader.setUniformMat4("view", v);

//TODO: gyp:相机位置参数没传
//    mShader.setVec3("camPos", camera.Position);

    auto irradiancePass = passManager.getPassAs<IrradiancePass>("irradiance");
    GLuint irradianceMap = irradiancePass->getIrradianceMap();

    auto prefilterPass = passManager.getPassAs<PrefilterPass>("prefilter");
    GLuint prefilterMap = prefilterPass->getPrefilterMap();

    auto brdfPass = passManager.getPassAs<BrdfPass>("brdf");
    GLuint brdfLUTTexture = brdfPass->getBrdfLUTTexture();

    // bind pre-computed IBL data
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

    auto shadowPass = passManager.getPassAs<ShadowMappingDepthPass>("shadowMappingDepth");
    glm::mat4 lightSpaceMatrix = shadowPass->getLightSpaceMatrix();
    GLuint depthMap = shadowPass->getDepthMap();
    glm::vec3 lightPos = shadowPass->getLightPos();
    glm::mat4 lightProjection = shadowPass->getLightProjection();
    glm::mat4 lightView = shadowPass->getLightView();


    mShader.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, depthMap);


    glm::mat4 model = glm::scale(m,glm::vec3 (0.001f));
    mShader.setUniformMat4("model", model);

    mShader.setUniformVec3("lightPos", lightPos);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
//TODO: gyp: 要加上缩放
    // wall
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, wallAlbedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, wallNormalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, wallMetallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, wallRoughnessMap);
#if ENABLE_SSAO
    auto ssaoPass = passManager.getPassAs<SSAOPass>("SSAO");
    GLuint ssao = ssaoPass->getSsaoColorBufferBlur();
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, ssao);
#else
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, wallAOMap);
#endif


    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(3.0, 2.0, -15.0));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    mShader.setUniformBool("isNotInstanced", true);
    mShader.setUniformBool("useAlbedoMap",true);
    mShader.setUniformBool("useNormalMap",true);
    mShader.setUniformBool("useMetallicMap",true);
    mShader.setUniformBool("useRoughnessMap",true);
    mShader.setUniformBool("useAoMap",true);
    mShader.setUniformBool("lightChange", lightChange);
    // floor
//    auto planeVAO = shadowPass->getPlaneVao();
//    model = glm::mat4(1.0f);
//    mShader.setUniformMat4("model", model);
//    glBindVertexArray(planeVAO);
//    glDrawArrays(GL_TRIANGLES, 0, 6);
//    glBindVertexArray(0);
    mShader.setUniformBool("isNotInstanced", false);
    model = glm::scale(m,glm::vec3 (0.001f));
    mShader.setUniformMat4("model", model);
    draw();
    // 渲染结束后，解绑所有使用的纹理
    // 从最高纹理单元开始解绑，倒序解绑所有纹理单元
    for (int i = 8; i >= 0; i--) {
        glActiveTexture(GL_TEXTURE0 + i);
        if (i <= 2) {
            // 前三个纹理单元绑定的是立方体贴图和BRDF LUT
            if (i == 2) {
                glBindTexture(GL_TEXTURE_2D, 0); // BRDF LUT
            } else {
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0); // irradiance和prefilter maps
            }
        } else {
            glBindTexture(GL_TEXTURE_2D, 0); // PBR材质纹理
        }
    }

    // 恢复默认纹理单元
    glActiveTexture(GL_TEXTURE0);

    // 禁用深度测试等可能影响其他通道的状态
//    glDisable(GL_DEPTH_TEST);

    // 恢复默认程序
    glUseProgram(0);
#endif


    return true;
}

void PbrPass::render(const glm::mat4& p, const glm::mat4& v, const std::vector<glm::mat4>& m) {
    if(!isInitHand){
        std::vector<char> vertexShaderCode = readFileFromAssets("shaders/sphere.vert");
        vertexShaderCode.push_back('\0');
        std::vector<char> fragmentShaderCode = readFileFromAssets("shaders/sphere.frag");
        fragmentShaderCode.push_back('\0');
        sphereShader.loadShader(vertexShaderCode.data(), fragmentShaderCode.data());
        isInitHand = true;
    }
    sphereShader.use();
    sphereShader.setUniformMat4("projection", p);
    sphereShader.setUniformMat4("view", v);

//TODO: gyp: 要加上缩放
    for(size_t i = 0; i < m.size(); i++){
        glm::mat4 model = glm::scale(m[0],glm::vec3 (1.0f));
        model = m[i];
        model = glm::scale(m[i],glm::vec3 (0.005f));
        sphereShader.setUniformMat4("model", model);
        renderSphere();
    }
}

void PbrPass::render(const glm::mat4 &p, const glm::mat4 &v) {
#if 1
#if ENABLE_DEBUG_SHADOWMAP
    auto& passManager = RenderPassManager::getInstance();
    passManager.restoreViewport();
    auto shadowPass = passManager.getPassAs<ShadowMappingDepthPass>("shadowMappingDepth");
    // debug shadow map
    shadowPass->debugShadowMapRender();
#else
    auto& passManager = RenderPassManager::getInstance();
    passManager.restoreViewport();
    // render
    // ------
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // render scene, supplying the convoluted irradiance map to the final shader.
    // ------------------------------------------------------------------------------------------
    glEnable(GL_DEPTH_TEST);
//    glFrontFace(GL_CW);
//    glCullFace(GL_BACK);
    mShader.use();
    mShader.setUniformMat4("projection", p);
    glm::mat4 model = glm::mat4(1.0f);
//    glm::mat4 view = camera.GetViewMatrix();
    mShader.setUniformMat4("view", v);
//TODO: gyp:相机位置参数没传
//    mShader.setVec3("camPos", camera.Position);

    auto shadowPass = passManager.getPassAs<ShadowMappingDepthPass>("shadowMappingDepth");
    glm::mat4 lightSpaceMatrix = shadowPass->getLightSpaceMatrix();
    GLuint depthMap = shadowPass->getDepthMap();
    glm::vec3 lightPos = shadowPass->getLightPos();
    glm::mat4 lightProjection = shadowPass->getLightProjection();
    glm::mat4 lightView = shadowPass->getLightView();

    // set light uniforms
//    mShader.setUniformVec3("viewPos", camera.Position);
    mShader.setUniformVec3("lightPos", lightPos);
    mShader.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    auto irradiancePass = passManager.getPassAs<IrradiancePass>("irradiance");
    GLuint irradianceMap = irradiancePass->getIrradianceMap();

    auto prefilterPass = passManager.getPassAs<PrefilterPass>("prefilter");
    GLuint prefilterMap = prefilterPass->getPrefilterMap();

    auto brdfPass = passManager.getPassAs<BrdfPass>("brdf");
    GLuint brdfLUTTexture = brdfPass->getBrdfLUTTexture();

    // bind pre-computed IBL data
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

    // rusted iron
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, ironAlbedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, ironNormalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, ironMetallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, ironRoughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, ironAOMap);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-5.0, 2.0, -15.0));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    mShader.setUniformBool("isNotInstanced", true);
    mShader.setUniformBool("useAlbedoMap",true);
    mShader.setUniformBool("useNormalMap",true);
    mShader.setUniformBool("useMetallicMap",true);
    mShader.setUniformBool("useRoughnessMap",true);
    mShader.setUniformBool("useAoMap",true);
    renderSphere();

    // gold
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, goldAlbedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, goldNormalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, goldMetallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, goldRoughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, goldAOMap);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-3.0, 2.0, -15.0));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    mShader.setUniformBool("isNotInstanced", true);
    mShader.setUniformBool("useAlbedoMap",true);
    mShader.setUniformBool("useNormalMap",true);
    mShader.setUniformBool("useMetallicMap",true);
    mShader.setUniformBool("useRoughnessMap",true);
    mShader.setUniformBool("useAoMap",true);
    renderSphere();

    // grass
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, grassAlbedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, grassNormalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, grassMetallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, grassRoughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, grassAOMap);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0, 2.0, -15.0));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    mShader.setUniformBool("isNotInstanced", true);
    mShader.setUniformBool("useAlbedoMap",true);
    mShader.setUniformBool("useNormalMap",true);
    mShader.setUniformBool("useMetallicMap",true);
    mShader.setUniformBool("useRoughnessMap",true);
    mShader.setUniformBool("useAoMap",true);
    renderSphere();

    // plastic
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, plasticAlbedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, plasticNormalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, plasticMetallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, plasticRoughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, plasticAOMap);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(1.0, 2.0, -15.0));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    mShader.setUniformBool("isNotInstanced", true);
    mShader.setUniformBool("useAlbedoMap",true);
    mShader.setUniformBool("useNormalMap",true);
    mShader.setUniformBool("useMetallicMap",true);
    mShader.setUniformBool("useRoughnessMap",true);
    mShader.setUniformBool("useAoMap",true);
    renderSphere();

    // wall
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, wallAlbedoMap);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, wallNormalMap);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, wallMetallicMap);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, wallRoughnessMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, wallAOMap);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(3.0, 2.0, -15.0));
    mShader.setUniformMat4("model", model);
    mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
    mShader.setUniformBool("isNotInstanced", true);
    mShader.setUniformBool("useAlbedoMap",true);
    mShader.setUniformBool("useNormalMap",true);
    mShader.setUniformBool("useMetallicMap",true);
    mShader.setUniformBool("useRoughnessMap",true);
    mShader.setUniformBool("useAoMap",true);
    renderSphere();



    // floor
    auto planeVAO = shadowPass->getPlaneVao();
    model = glm::mat4(1.0f);
    mShader.setUniformMat4("model", model);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // render light source (simply re-render sphere at light positions)
    // this looks a bit off as we use the same shader, but it'll make their positions obvious and
    // keeps the codeprint small.
    for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i)
    {
        glm::vec3 newPos = lightPositions[i];
        newPos = lightPositions[i];
        mShader.setUniformVec3("lightPositions[" + std::to_string(i) + "]", newPos);
        mShader.setUniformVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);

        model = glm::mat4(1.0f);
        model = glm::translate(model, newPos);
        model = glm::scale(model, glm::vec3(0.5f));
        mShader.setUniformMat4("model", model);
        mShader.setUniformMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
//        renderSphere();
    }
    draw();
    // 渲染结束后，解绑所有使用的纹理
    // 从最高纹理单元开始解绑，倒序解绑所有纹理单元
    for (int i = 8; i >= 0; i--) {
        glActiveTexture(GL_TEXTURE0 + i);
        if (i <= 2) {
            // 前三个纹理单元绑定的是立方体贴图和BRDF LUT
            if (i == 2) {
                glBindTexture(GL_TEXTURE_2D, 0); // BRDF LUT
            } else {
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0); // irradiance和prefilter maps
            }
        } else {
            glBindTexture(GL_TEXTURE_2D, 0); // PBR材质纹理
        }
    }

    // 恢复默认纹理单元
    glActiveTexture(GL_TEXTURE0);

    // 禁用深度测试等可能影响其他通道的状态
//    glDisable(GL_DEPTH_TEST);

    // 恢复默认程序
    glUseProgram(0);
#endif
#endif

    // 可根据PBR需求扩展渲染流程，这里直接复用父类逻辑
    return;
}

// renders (and builds at first invocation) a sphere
// -------------------------------------------------

void PbrPass::renderSphere()
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

        // 生成球面顶点数据
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

        // 生成索引
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
