#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "renderMesh.h"
#include "renderShader.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "communication/dataInterface.h"
#include <android/log.h>
#define LOG_TAG "renderModel.cpp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

class renderModel {
public:
    renderModel() = delete;
    renderModel(const std::string& name, bool hasBoneInfo = false);
    ~renderModel();

    std::vector<cadDataManager::pmiInfo> pmi;
    std::map<std::string, std::vector<int>> protoId;
    std::vector<std::string> nameVector;
    std::vector<std::vector<glm::vec3>> verticesVector;
    std::vector<std::vector<glm::vec3>> normalsVector;
    std::vector<std::vector<glm::vec2>> UVVector;
    std::vector<std::vector<uint32_t>> indicesVector;
    std::vector<pbrMaterial> materialVector;
    std::vector<std::string> materialNameVector;
    std::vector<std::vector<float>> transformVector;
    std::vector<int> transformNumVector;
    std::vector<bool> isTextureVector;
    std::unordered_map<std::string, int> meshIndice;
    int indiceSum = 0;

    std::string& name();

    bool loadModel(const std::string& modelFileName);
    bool loadLocalModel(const std::string &modelFileName,const std::string &importer_type="");

    bool loadFbModel(const std::string& file_name, const std::string& file_path);

    bool initialize() { return false; };

    bool bindMeshTexture(const std::string& meshName, const std::string& textureName);
    bool activeMeshTexture(const std::string& meshName, const std::string& textureName);

    bool render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m);

    int getBoneNodeIndexByName(const std::string& name) const;

    void setBoneNodeMatrices(const std::string& bone, const glm::mat4& m);

    const std::shared_ptr<std::map<std::string, renderMesh>> &getMMeshes() const {
        return mMeshes;
    }

    void updateMMesh(std::string& name, std::vector<std::vector<float>>& transform){
        try {
            auto meshID = protoId.at(name);  // 如果 key 不存在，抛出 std::out_of_range
            int k = 0;
            for( int i : meshID ){
                auto mesh = mMeshes->at(std::to_string(i));
//                mMeshes->at(std::to_string(i)).mTransformVector = std::move(transform[k]);
                LOGI("%i",mesh.mVertices.size());
                mesh.mTransformVector = std::move(transform[k]);
                mesh.setupMesh();
                mMeshes->erase(std::to_string(i));
                mMeshes->insert(std::pair<std::string, renderMesh>(
                        std::to_string(i),
                        mesh));
                k++;
            }
        } catch (const std::out_of_range& e) {
            LOGI("键不存在:");
            std::cerr << "键不存在: " << name << std::endl;
        }
    }

    void shaderInit(){
        initShader();
    }

    void pushMeshFromCustomData() {
        for (int i = 0; i < verticesVector.size(); i++){
            std::pair<std::string, std::vector<renderMesh>>;
            mMeshes->insert(std::pair<std::string, renderMesh>(
                    std::to_string(i),
                    createMeshFromCustomData(verticesVector[i], normalsVector[i],
                                             UVVector[i], indicesVector[i], materialVector[i],
                                             materialNameVector[i], isTextureVector[i],
                                             transformNumVector[i], transformVector[i])));
        }
    }

    renderMesh createMeshFromCustomData(const std::vector<glm::vec3> &positions,
                                        const std::vector<glm::vec3> &normals,
                                        const std::vector<glm::vec2> &uvs,
                                        const std::vector<uint32_t> &indices, const pbrMaterial &material,
                                        const std::string &materialName, const bool &isActive, int i,
                                        std::vector<float> vector);

    void countIndiceSum(){
        indiceSum = 0;
        for(auto& i : indicesVector){
            indiceSum += i.size() / 3;
        }
    }

    int getIndiceSum(){
        return indiceSum;
    }


private:
    void initShader();
    std::vector<renderTexture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
    std::vector<renderTexture> loadMaterialTextures_force(aiMaterial* mat, aiTextureType type, std::string typeName, std::string file);
    void processNode(aiNode* node, const aiScene* scene);
    renderMesh processMesh(aiMesh* mesh, const aiScene* scene);
    void processMeshBone(aiMesh* mesh, std::vector<renderVertex>& vertices);
    void initializeBoneNode();
    void draw();

private:
    std::string mName;
    std::shared_ptr<std::map<std::string, renderMesh>> mMeshes = std::make_shared<std::map<std::string, renderMesh>>();
    bool mHasBoneInfo;

    struct boneInfo {
        int id;
        boneInfo(int count) : id(count) {};
    };
    std::map<std::string, std::shared_ptr<boneInfo>> mBoneInfoMap;

    bool mIsGammaCorrection;

    std::vector<renderTexture> mTexturesLoaded;
    std::string mDirectory;

    std::map<std::string, std::vector<std::string>> mMeshTexturesMap;

    static renderShader mShader;



private:

    template<typename T>
    glm::vec3 toNewVec3(std::vector<T> *flat_vector, int begin);

    template<typename T>
    glm::vec2 toNewVec2(std::vector<T> *flat_vector, int begin);
};