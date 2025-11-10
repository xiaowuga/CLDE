#pragma once
#include "templatePass.h"
#include "App.h"
class BoundingBOX{
public:
    BoundingBOX();
    void draw(const glm::mat4& mvp);
    void updateExtents(const std::vector<float>& vertices);
    void updateBox(const std::string& name, const std::vector<float>& vertices);
    std::string instanceName;
    std::vector<float> extents;
private:
    static GLuint shaderProgram;
};


class GizmoPass : public TemplatePass{
public:
    GizmoPass();
    virtual ~GizmoPass();

    virtual bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) override;
    //virtual void initialize(const std::shared_ptr<std::map<std::string, renderMesh>>& Meshes) override = 0;
    virtual void render(const glm::mat4& p, const glm::mat4& v) override;
    void updateBoundingBOX(std::vector<float>& vertices);
    void initBoundingBoxMap(std::unordered_map<std::string, std::vector<float>> &vertices);
    void updateBoundingBoxMap(std::string instanceName, std::vector<float> &point);
    std::vector<float> getBoxArray(std::string instanceName);

protected:

//    virtual void initShader() override = 0;
//    virtual void draw() override = 0;
//    virtual void initMeshes(const std::shared_ptr<std::map<std::string, renderMesh>>& Meshes) override = 0;
private:
    std::vector<BoundingBOX> boxes;

};