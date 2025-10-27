#pragma once
#include "templatePass.h"
#define ENABLE_DEBUG_SHADOWMAP 0
#define ENABLE_SPHERE_SCENE 0
class PbrPass : public TemplatePass {
public:
    PbrPass();
    virtual ~PbrPass();


    virtual bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) override;
    virtual void render(const glm::mat4& p, const glm::mat4& v);
    virtual void render(const glm::mat4 &p, const glm::mat4 &v, const std::vector<glm::mat4> &m);
    void renderSphere();
protected:
    virtual void initShader() override;
    virtual void draw() override;


    unsigned int sphereVAO = 0;
    unsigned int indexCount;
private:
    //test
    unsigned int ironAlbedoMap;
    unsigned int ironNormalMap;
    unsigned int ironMetallicMap;
    unsigned int ironRoughnessMap;
    unsigned int ironAOMap;
//gold
    unsigned int goldAlbedoMap;
    unsigned int goldNormalMap;
    unsigned int goldMetallicMap;
    unsigned int goldRoughnessMap;
    unsigned int goldAOMap;
// gras
    unsigned int grassAlbedoMap;
    unsigned int grassNormalMap;
    unsigned int grassMetallicMap;
    unsigned int grassRoughnessMap;
    unsigned int grassAOMap;
// plastic
    unsigned int plasticAlbedoMap;
    unsigned int plasticNormalMap;
    unsigned int plasticMetallicMap;
    unsigned int plasticRoughnessMap;
    unsigned int plasticAOMap;

    unsigned int wallAlbedoMap;
    unsigned int wallNormalMap;
    unsigned int wallMetallicMap;
    unsigned int wallRoughnessMap;
    unsigned int wallAOMap;

    glm::vec3 lightPositions[4];
    glm::vec3 lightColors[4];

    renderShader sphereShader;
    bool isInitHand = false;

    void renderMeasure(const glm::mat4 &p, const glm::mat4 &v, const glm::mat4 &m);

    void renderMeasure();
};

