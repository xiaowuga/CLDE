#pragma once
#include "templatePass.h"
class BoundingBOX{
public:
    BoundingBOX();
    void draw(const glm::mat4& mvp);
    void updateExtents(const std::vector<float>& vertices);
private:
    std::vector<float> extents;
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
protected:

//    virtual void initShader() override = 0;
//    virtual void draw() override = 0;
//    virtual void initMeshes(const std::shared_ptr<std::map<std::string, renderMesh>>& Meshes) override = 0;
private:
    std::vector<BoundingBOX> boxes;
};