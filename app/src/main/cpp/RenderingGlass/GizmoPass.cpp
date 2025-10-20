#include "GizmoPass.h"
#include "utils.h"
GLuint BoundingBOX::shaderProgram = 0;

GLuint createShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

BoundingBOX::BoundingBOX(){
    extents.resize(6* sizeof(float));
    if(shaderProgram == 0) {
        // 顶点着色器
        const char *vertexShaderSource = R"(
        attribute vec3 aPosition;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
        )";
        GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);

        // 片段着色器
        const char *fragmentShaderSource = R"(
        precision mediump float;
        void main() {
            gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
        )";
        GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

        // 创建着色器程序
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
    }
}
void BoundingBOX::updateExtents(const std::vector<float> &vertices) {
    if(vertices.size() < 6) return;
    for(size_t i=0;i<6;i++){
        extents[i] = vertices[i];
    }
}
void BoundingBOX::draw(const glm::mat4 &mvp) {
    // 定义包围盒的 8 个顶点
    float minx = extents[0];
    float miny = extents[1];
    float minz = extents[2];
    float maxx = extents[3];
    float maxy = extents[4];
    float maxz = extents[5];

    // 顶点数据
    float vertices[] = {
            minx, miny, minz, maxx, miny, minz, maxx, maxy, minz, minx, maxy, minz,
            minx, miny, maxz, maxx, miny, maxz, maxx, maxy, maxz, minx, maxy, maxz
    };

    // 索引数据
    unsigned int indices[] = {
            0, 1, 1, 2, 2, 3, 3, 0, // 底面
            4, 5, 5, 6, 6, 7, 7, 4, // 顶面
            0, 4, 1, 5, 2, 6, 3, 7  // 连接底面和顶面的边
    };

    // 创建 VBO
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 创建 IBO
    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 使用着色器程序
    glUseProgram(shaderProgram);

    // 获取属性和 uniform 位置
    GLint aPosition = glGetAttribLocation(shaderProgram, "aPosition");
    GLint uMVP = glGetUniformLocation(shaderProgram, "uMVP");

    // 设置 MVP 矩阵
    glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));

    // 启用顶点属性
    glEnableVertexAttribArray(aPosition);
    glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    // 绘制线段
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);

    // 禁用顶点属性
    glDisableVertexAttribArray(aPosition);

    // 清理资源
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
}
GizmoPass::GizmoPass() : TemplatePass(){}
GizmoPass::~GizmoPass(){}
void GizmoPass::updateBoundingBOX(std::vector<float>& vertices){
    if(vertices.size()<6) return;
    if(vertices.size() / 6 > boxes.size()){
        boxes.resize(vertices.size() / 6);
    }
    for(size_t i=0;i<vertices.size() / 6;i++){
        std::vector<float> boxVertices(vertices.begin()+i*6,vertices.begin()+(i+1)*6);
        boxes[i].updateExtents(boxVertices);
    }
}
bool GizmoPass::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m){
    for(auto box:boxes){
        glm::mat4 mvp = p * v * m;
        box.draw(mvp);
    }
    return true;
}
void GizmoPass::render(const glm::mat4& p, const glm::mat4& v){
    render(p, v, glm::mat4(1.0f));
}