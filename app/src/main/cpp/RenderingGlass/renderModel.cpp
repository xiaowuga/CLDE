#include"renderModel.h"
#include"utils.h"
#include"logger.h"
#include "RenderingGlass/renderPassManager.h"
#include "RenderingGlass/pbrPass.h"
#include<iostream>
#include<fstream>
#include<algorithm>
#include<queue>

renderShader renderModel::mShader;
void renderModel::initShader() {
    static bool init = false;
    if (init) {
        return;
    } else {
        const char* vertexShaderCode = R"_(
             #version 320 es
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTexCoords;
            layout(location = 3) in vec3 tangent;
            layout(location = 4) in vec3 bitangent;
            layout(location = 5) in ivec4 boneIds;
            layout(location = 6) in vec4 weights;

            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;

            const int MAX_BONE_NODES = 100;
            const int MAX_BONE_INFLUENCE = 4;
            uniform mat4 finalBoneNodesMatrices[MAX_BONE_NODES];

            out vec2 TexCoords;

            void main(){
                vec4 total_position = vec4(0.0f);
                bool has_bone = false;
                for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                    if (boneIds[i] < 0) {
                        continue;
                    }
                    if (boneIds[i] >= MAX_BONE_NODES) {
                        total_position = vec4(aPos, 1.0f);
                        break;
                    }
                    vec4 local_position = finalBoneNodesMatrices[boneIds[i]] * vec4(aPos, 1.0f);
                    total_position += local_position * weights[i];
                    has_bone = true;
                }
                if (has_bone == false) {
                    total_position = vec4(aPos, 1.0f);
                }
                gl_Position = projection * view * model * total_position;
                TexCoords = aTexCoords;
            }
        )_";

        const char* fragmentShaderCode = R"_(
            #version 320 es
            precision mediump float;
            out vec4 FragColor;
            in vec2 TexCoords;
            uniform sampler2D texture_diffuse1;
            void main()
            {
                FragColor = texture(texture_diffuse1, TexCoords);
            }
        )_";
        mShader.loadShader(vertexShaderCode, fragmentShaderCode);
        init = true;
    }
}

renderModel::renderModel(const std::string& name, bool hasBoneInfo) : mName(name), mHasBoneInfo(hasBoneInfo) {
    mBoneInfoMap.clear();
}

renderModel::~renderModel() {
    mBoneInfoMap.clear();
}

std::string& renderModel::name() {
    return mName;
}

bool renderModel::bindMeshTexture(const std::string& meshName, const std::string& textureName) {
    auto it = mMeshTexturesMap.find(meshName);
    if (it == mMeshTexturesMap.end()) {
        std::vector<std::string> textureNames(1, textureName);
        mMeshTexturesMap[meshName] = textureNames;
    } else {
        it->second.push_back(textureName);
    }
    return true;
}

bool renderModel::activeMeshTexture(const std::string& meshName, const std::string& textureName) {
    auto it = mMeshes->find(meshName);
    if (it != mMeshes->end()) {
        return it->second.activeTexture(textureName);
    }
    return false;
}

std::vector<renderTexture> renderModel::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
    std::vector<renderTexture> textures;
    for (uint32_t i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        bool skip = false;
        for (uint32_t j = 0; j < mTexturesLoaded.size(); j++) {
            if (std::strcmp(mTexturesLoaded[j].path.data(), str.C_Str()) == 0) {
                textures.push_back(mTexturesLoaded[j]);
                skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                break;
            }
        }
        if (!skip) {
            renderTexture texture;
            texture.id = TextureFromFileAssets(str.C_Str(), mDirectory);
            texture.type = typeName;
            texture.path = str.C_Str();
            texture.active = false;
            textures.push_back(texture);
            mTexturesLoaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
        }
    }
    return textures;
}

std::vector<renderTexture> renderModel::loadMaterialTextures_force(aiMaterial* mat, aiTextureType type, std::string typeName, std::string file) {
    std::vector<renderTexture> textures;
    bool skip = false;
    for (uint32_t j = 0; j < mTexturesLoaded.size(); j++) {
        if (std::strcmp(mTexturesLoaded[j].path.data(), file.c_str()) == 0) {
            skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
            break;
        }
    }
    if (!skip) {
        renderTexture texture;
        if((file.rfind("/storage/emulated/0/",0)==0)) texture.id = TextureFromFile(file.c_str(), ""); //start with
        else texture.id = TextureFromFileAssets(file.c_str(), "");
        texture.type = typeName;
        texture.path = file.c_str();
        texture.active = false;
        textures.push_back(texture);
        mTexturesLoaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures.
    }
    return textures;
}

void renderModel::processMeshBone(aiMesh* mesh, std::vector<renderVertex>& vertices) {
    int boneIndex = 0;
//    infof("processMeshBone mesh name:%s, vertices:%d, total bone:%d", mesh->mName.C_Str(), vertices.size(), mesh->mNumBones);
    for (uint32_t i = 0; i < mesh->mNumBones; i++) {
//        infof("i:%02d, bone: %-16s, %02d, total weights:%d", i, mesh->mBones[i]->mName.C_Str(), boneIndex, mesh->mBones[i]->mNumWeights);
        std::string name = mesh->mBones[i]->mName.C_Str();
        std::shared_ptr<boneInfo> boneInformation(new boneInfo(boneIndex));
        auto it = mBoneInfoMap.find(name);
        if (it == mBoneInfoMap.end()) {
            mBoneInfoMap[name] = boneInformation;
        } else {
            errorf("already has boneNode %s", name.c_str());
        }

        for (int weightIndex = 0; weightIndex < mesh->mBones[i]->mNumWeights; weightIndex++) {
            int vertexIndex = mesh->mBones[i]->mWeights[weightIndex].mVertexId;
            float weight =  mesh->mBones[i]->mWeights[weightIndex].mWeight;
            if (weightIndex < vertices.size()) {
                renderVertex &v = vertices[vertexIndex];
                int k = 0;
                for (k = 0; k < MAX_BONE_INFLUENCE; k++) {
                    if (v.BoneIDs[k] < 0) {
                        v.Weights[k] = weight;
                        v.BoneIDs[k] = boneIndex;
                        break;
                    }
                }
                if (k >= 4) {
                    //errorf("k >= 4, weightIndex:%d", weightIndex);
                }
            } else {
                errorf("weightIndex %d > vertices.size() %d", weightIndex, vertices.size());
            }

        }
        boneIndex++;
    }
}

renderMesh renderModel::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<renderVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<renderTexture> textures;

//    infof("mesh vertex count: %d, face count:%d, bone:%d", mesh->mNumVertices, mesh->mNumFaces, mesh->mNumBones);
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        renderVertex vertex{};
        glm::vec3 vector{};
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        if (mesh->HasNormals()) {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }

        if (mesh->mTextureCoords[0]) {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
            // tangent
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.Tangent = vector;
            // bitangent
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.Bitangent = vector;
        } else {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }
        vertices.push_back(vertex);
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    if (mHasBoneInfo) {
        processMeshBone(mesh, vertices);
    }
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

    /*
    // 1. diffuse maps
    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    */

//    infof((std::string("MName: ")+mesh->mName.C_Str()).c_str());
    auto it = mMeshTexturesMap.find(mesh->mName.C_Str());
    if (it != mMeshTexturesMap.end()) {
        for (auto& i : it->second) {
            std::vector<renderTexture> texture = loadMaterialTextures_force(material, aiTextureType_DIFFUSE, "texture_diffuse", i);
            textures.insert(textures.end(), texture.begin(), texture.end());
        }
    }

    return renderMesh(vertices, indices, textures);
}



renderMesh renderModel::createMeshFromCustomData(const std::vector<glm::vec3> &positions,
                                                 const std::vector<glm::vec3> &normals,
                                                 const std::vector<glm::vec2> &uvs,
                                                 const std::vector<uint32_t> &indices,
                                                 const pbrMaterial &material,
                                                 const std::string &materialName, const bool &isActive,
                                                 int transformNum,
                                                 std::vector<float> transformVector) {
    std::vector<renderVertex> vertices;
    std::vector<unsigned int> meshIndices;
    std::vector<renderTexture> textures;

    // 填充顶点数据
    for (size_t i = 0; i < positions.size(); ++i) {
        renderVertex vertex;
        vertex.Position = positions[i];
        vertex.Normal = normals[i];
        vertex.TexCoords = uvs[i];
        // 切线/副切线留空
        vertices.push_back(vertex);
    }

    // 填充索引数据
    meshIndices.assign(indices.begin(), indices.end());

    renderTexture texture;
    texture.path = materialName;
    texture.active = isActive;
    texture.type = "texture_diffuse";
    glGenTextures(1, &texture.id);
//TODO: LiZiRui: 后面可以用实例化绘制优化,小mesh的matrix
//    infof("transformNum %f", transformNum);
    return renderMesh(vertices, indices, textures, material, transformNum, transformVector);
}

void renderModel::processNode(aiNode* node, const aiScene* scene) {
    static std::string indent = "";
//    infof("%snode:%s, children:%d", indent.c_str(), node->mName.C_Str(), node->mNumChildren); ** My Changes : Comment **
//    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
//        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
//        infof("%smesh: %s", indent.c_str(), mesh->mName.C_Str());
//        mMeshes.insert(std::pair<std::string, Mesh>(mesh->mName.C_Str(), processMesh(mesh, scene)));
//    }
    for (int i = 0; i < verticesVector.size(); i++){
        mMeshes->insert(std::pair<std::string, renderMesh>(
                i+"",
                createMeshFromCustomData(verticesVector[i], normalsVector[i],
                                         UVVector[i], indicesVector[i], materialVector[i],
                                         materialNameVector[i], isTextureVector[i],
                                         transformNumVector[i], transformVector[i])));
    }
//    for (uint32_t i = 0; i < node->mNumChildren; i++) {
//        std::string tmp = indent;
//        indent += "  ";
//        processNode(node->mChildren[i], scene);
//        indent = tmp;
//    }
}

bool renderModel::loadModel(const std::string& modelFileName) {
    std::vector<char> fileData = readFileFromAssets(modelFileName.c_str());
    Assimp::Importer importer;
//    const aiScene* scene = importer.ReadFile(modelFileName, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    const aiScene* scene = importer.ReadFileFromMemory(fileData.data(), fileData.size(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
        Log::Write(Log::Level::Error, Fmt("assimp readfile error %s", importer.GetErrorString()));
        return false;
    }
    mDirectory = modelFileName.substr(0, modelFileName.find_last_of('/'));
//    infof("model:%s, scene:%s, mNumMeshes:%d, mNumMaterials:%d, mNumAnimations:%d, mNumTextures:%d", modelFileName.c_str(),scene->mName.C_Str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations, scene->mNumTextures);
    processNode(scene->mRootNode, scene);
    initializeBoneNode();
    return true;
}

template<typename T>
glm::vec3 renderModel::toNewVec3(std::vector<T>* flat_vector, int begin)
{
    float a = flat_vector->at(begin);
    float b = flat_vector->at(begin + 1);
    float c = flat_vector->at(begin + 2);
    return glm::vec3(a, b, c);
}

template<typename T>
glm::vec2 renderModel::toNewVec2(std::vector<T>* flat_vector, int begin)
{
    float a = flat_vector->at(begin);
    float b = flat_vector->at(begin + 1);
    return glm::vec2(a, b);
}

bool renderModel::loadFbModel(const std::string& file_name, const std::string& file_path) {

//std::string fliepath = MakeSdcardPath("/Download/sphere.fb");
//    std::string  filePath = modelFileName;
//    cadDataManager::DataInterface::parseLocalModel(file_name, file_path);
    cadDataManager::DataInterface::setActiveDocumentData(file_name);
//    std::vector<cadDataManager::RenderInfo> renderInfoArray = cadDataManager::DataInterface::getRenderInfo();
//    cadDataManager::RenderInfo renderInfo = renderInfoArray[0];

//    std::string hello = renderInfo.protoId;
//    const char* protoHello = hello.c_str();
//    infof(protoHello);
    auto MapInfo = cadDataManager::DataInterface::getRenderInfoMap();
    pmi = cadDataManager::DataInterface::getPmiInfos();
//    auto instances = cadDataManager::DataInterface::getInstances();
//    auto instanceInfos = cadDataManager::DataInterface::getInstanceInfos();
//    std::string fbModelData = cadDataManager::DataInterface::getModelFlatbuffersData();

    uint8_t* buffer_data;
    int buffer_size;
    int lastProtoSize = verticesVector.size();
    std::string name = "";
    for (auto it = MapInfo.begin(); it != MapInfo.end(); ++it){
        if(verticesVector.size() > lastProtoSize)
        {
            std::vector<int> tempInt;
            for(int k = lastProtoSize; k < verticesVector.size(); k++)
            {
                tempInt.push_back(k);
            }
            protoId.insert(std::pair<std::string, std::vector<int>>(
                    name,
                    tempInt
            ));
            lastProtoSize = verticesVector.size();
        }
        name = it->first;
        auto info = it->second;
        for (int o = 0; o < info.size(); o++) {
            std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices; //存储点信息，相同点只存一份
            std::vector<TinyModelVertex> mVertices{};                     //保存点在数组中位置信息
            std::vector<uint32_t> mIndices{};                             //索引，找点

            cadDataManager::RenderInfo &modelfbs = info[o];
            auto protoId = modelfbs.protoId;
            int num = modelfbs.matrixNum;
            auto matrix = modelfbs.matrix;
            auto type = modelfbs.type;
            auto modelGeo = modelfbs.geo;
            auto modelIndex = modelGeo->getIndex();
            auto position = modelGeo->getPosition();
            auto normal = modelGeo->getNormal();
            auto uv = modelGeo->getUV();
            auto modelPar = modelfbs.params;
            auto metalness = modelPar->mMetalness;
            auto specular = modelPar->mSpecular;
            auto opacity = modelPar->mOpacity;
            auto color = modelPar->mColor;//后续会改成三维rgb
            auto emissive = modelPar->mEmissive;
            auto emissiveIntensity = modelPar->mEmissiveIntensity;
            auto shininess = modelPar->mShininess;
            auto roughness = modelPar->mRoughness;
            auto transmission = modelPar->mTransmission;
            auto material = modelPar->getMaterialName();//这里得到材质的名称(未生效)
            bool mIsTexture = false;

            std::string testcolor = color.substr(1);
            pbrMaterial pbrMaterial;
            if(testcolor == "000000")
            {
                // black specular button
                pbrMaterial.albedoValue = {0.0f, 0.0f, 0.0f};
                pbrMaterial.useAlbedoMap = false;
                pbrMaterial.albedoMapId = 0;

                pbrMaterial.normalValue = {0.0f, 0.0f, 1.0f};
                pbrMaterial.useNormalMap = false;
                pbrMaterial.normalMapId = 0;

                pbrMaterial.metallicValue = 0.8f;
                pbrMaterial.useMetallicMap = false;
                pbrMaterial.metallicMapId = 0;

                pbrMaterial.roughnessValue = 0.0f;
                pbrMaterial.useRoughnessMap = false;
                pbrMaterial.roughnessMapId = 0;

                pbrMaterial.aoValue = 1.0f;
                pbrMaterial.useAoMap = false;
                pbrMaterial.aoMapId = 0;


            }
            else if(testcolor == "BBBBBB" || testcolor == "333333")
            {
                // diffuse gray board
                pbrMaterial.albedoValue = {0.2f, 0.2f, 0.2f};
                pbrMaterial.useAlbedoMap = false;
                pbrMaterial.albedoMapId = 0;

                pbrMaterial.normalValue = {0.0f, 0.0f, 1.0f};
                pbrMaterial.useNormalMap = false;
                pbrMaterial.normalMapId = 0;

                pbrMaterial.metallicValue = 0.2f;
                pbrMaterial.useMetallicMap = false;
                pbrMaterial.metallicMapId = 0;

                pbrMaterial.roughnessValue = 0.8f;
                pbrMaterial.useRoughnessMap = false;
                pbrMaterial.roughnessMapId = 0;

                pbrMaterial.aoValue = 1.0f;
                pbrMaterial.useAoMap = false;
                pbrMaterial.aoMapId = 0;


            }
            else if(testcolor == "FF0000")
            {
                //plastic red board
                pbrMaterial.albedoValue = {1.0f, 0.0f, 0.0f};
                pbrMaterial.useAlbedoMap = false;
                pbrMaterial.albedoMapId = 0;

                pbrMaterial.normalValue = {0.0f, 0.0f, 1.0f};
                pbrMaterial.useNormalMap = false;
                pbrMaterial.normalMapId = 0;

                pbrMaterial.metallicValue = 0.8f;
                pbrMaterial.useMetallicMap = false;
                pbrMaterial.metallicMapId = 0;

                pbrMaterial.roughnessValue = 0.7f;
                pbrMaterial.useRoughnessMap = false;
                pbrMaterial.roughnessMapId = 0;

                pbrMaterial.aoValue = 1.0f;
                pbrMaterial.useAoMap = false;
                pbrMaterial.aoMapId = 0;


            }
            else
            {
                // yellow words and lines
                pbrMaterial.albedoValue = {1.0f, 1.0f, 1.0f};
                pbrMaterial.useAlbedoMap = false;
                pbrMaterial.albedoMapId = 0;

                pbrMaterial.normalValue = {0.0f, 0.0f, 1.0f};
                pbrMaterial.useNormalMap = false;
                pbrMaterial.normalMapId = 0;

                pbrMaterial.metallicValue = 0.8f;
                pbrMaterial.useMetallicMap = false;
                pbrMaterial.metallicMapId = 0;

                pbrMaterial.roughnessValue = 0.2f;
                pbrMaterial.useRoughnessMap = false;
                pbrMaterial.roughnessMapId = 0;

                pbrMaterial.aoValue = 1.0f;
                pbrMaterial.useAoMap = false;
                pbrMaterial.aoMapId = 0;


            }

            if (type == "mesh")
            {
                for (int i = 0; i < modelIndex.size(); i += 1)
                {
                    TinyModelVertex vertex;
                    int index = modelIndex.at(i);
                    vertex.pos = toNewVec3(&position, index * 3);
                    vertex.normal = toNewVec3(&normal, index * 3);
                    if(uv.size() > index * 2){
                        vertex.uv = toNewVec2(&uv, index * 2);
                    }

                    if (uniqueVertices.count(vertex) == 0) //if unique 唯一
                    {                                      //push进数组。记录位置
                        uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                        mVertices.push_back(vertex);
                    }
                    mIndices.push_back(uniqueVertices[vertex]); //根据新proto的数组，索引位置改变
                }

                int Nodenumber = mVertices.size();   //顶点、法向、UV个数
                int Indicesnumber = mIndices.size(); //索引个数

                std::vector<glm::vec3> mVerticesPos{};                        //保存点在数组中位置信息
                std::vector<glm::vec3> mVerticesNor{};                        //保存点在数组中位置信息
                std::vector<glm::vec2> mVerticesUV{};

                //读取顶点，保存成数组形式
                for (int i = 0; i < Nodenumber; i++)
                {
                    mVerticesPos.push_back(mVertices[i].pos);
                    mVerticesNor.push_back(mVertices[i].normal);
                    mVerticesUV.push_back(mVertices[i].uv);
                }

                nameVector.push_back(protoId);
                verticesVector.push_back(mVerticesPos);
                normalsVector.push_back(mVerticesNor);
                UVVector.push_back(mVerticesUV);
                indicesVector.push_back(mIndices);
                materialVector.push_back(pbrMaterial);
                materialNameVector.push_back(material);//未生效
                transformVector.push_back(matrix);
                transformNumVector.push_back(num);
                isTextureVector.push_back(mIsTexture);

//                infof("verticesVector size : %i", verticesVector.size());
            }
        }
    }

    return true;
}

void renderModel::draw() {
    GL_CALL(glFrontFace(GL_CCW));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_CULL_FACE));
    GL_CALL(glEnable(GL_DEPTH_TEST));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    for (auto &it : *mMeshes) {
        it.second.draw(mShader);
    }
}

bool renderModel::render(const glm::mat4& p, const glm::mat4& v, const glm::mat4& m) {
//    mShader.use();
//    mShader.setUniformMat4("projection", p);
//    mShader.setUniformMat4("view", v);
//    mShader.setUniformMat4("model", m);
//    draw();
//    glUseProgram(0);
    auto& passManager = RenderPassManager::getInstance();
    passManager.executeAllPasses(p,v,m);
    //获取pbrpass
//    auto pbrPass = passManager.getPassAs<PbrPass>("pbr");
//    pbrPass->render(p,v,m);
    return true;
}

void renderModel::initializeBoneNode() {
    mShader.use();
    glm::mat4 m = glm::mat4(1.0f);
    for (auto it : mBoneInfoMap) {
        std::string name = "finalBoneNodesMatrices[" + std::to_string(it.second->id) +  "]";
        mShader.setUniformMat4(name, m);
    }
}

int renderModel::getBoneNodeIndexByName(const std::string& name) const {
    auto it = mBoneInfoMap.find(name);
    if (it != mBoneInfoMap.end()) {
        return it->second->id;
    }
    errorf("not found bone %s", name.c_str());
    return -1;
}

void renderModel::setBoneNodeMatrices(const std::string& bone, const glm::mat4& m) {
    int index = getBoneNodeIndexByName(bone);
    if (index < 0) {
        return;
    }
    std::string name = "finalBoneNodesMatrices[" + std::to_string(index) +  "]";
    mShader.use();
    mShader.setUniformMat4(name, m);
}

//bool renderModel::loadLocalModel(const std::string &modelFileName, const std::string &importer_type) {
//    initShader();
////    std::vector<char> fileData;
////    std::ifstream file;
////    file.open(modelFileName, std::ios::in | std::ios::binary | std::ios::ate);
////    if (!file) {
////        errorf("Failed to open file: %s",modelFileName.c_str());
////        return false;
////    }
////    std::streamsize fileSize = file.tellg();
////    file.seekg(0, std::ios::beg);
////    fileData.resize(fileSize);
////    if (!file.read(fileData.data(), fileSize)) {
////        std::cerr << "Failed to read file: " << modelFileName << std::endl;
////    }
//    Assimp::Importer importer;
//    const aiScene* scene = importer.ReadFile(modelFileName, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
////    const aiScene* scene = importer.ReadFileFromMemory(fileData.data(), fileData.size(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
//    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
//        Log::Write(Log::Level::Error, Fmt("assimp readfile error %s", importer.GetErrorString()));
//        return false;
//    }
//    mDirectory = modelFileName.substr(0, modelFileName.find_last_of('/'));
//    infof("model:%s, scene:%s, mNumMeshes:%d, mNumMaterials:%d, mNumAnimations:%d, mNumTextures:%d", modelFileName.c_str(),
//          scene->mName.C_Str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations, scene->mNumTextures);
//    processNode(scene->mRootNode, scene);
//    initializeBoneNode();
//    return true;
//}

bool renderModel::loadLocalModel(const std::string &modelFileName, const std::string &importer_type) {
    initShader();
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(modelFileName, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
//    const aiScene* scene = importer.ReadFileFromMemory(fileData.data(), fileData.size(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
        Log::Write(Log::Level::Error, Fmt("assimp read file error %s", importer.GetErrorString()));
        return false;
    }
    // ---------------------- Get Texture Filename ---------------------------
    std::vector<std::string> texture_filename_list;
    for (unsigned int m = 0; m<scene->mNumMaterials; m++){
        aiMaterial *mtl = scene->mMaterials[m];
        int nTex;
        if ((nTex = mtl->GetTextureCount(aiTextureType_DIFFUSE)) > 0){
            aiString path;	// filename
            for (int i = 0; i < nTex; ++i){
                if (mtl->GetTexture(aiTextureType_DIFFUSE, i, &path) == AI_SUCCESS){
//                    infof(("AIString: "+std::string(path.C_Str())).c_str());
                    texture_filename_list.emplace_back(path.C_Str());
                }
            }
        }
    }
    // --------------------- Get Mesh Name -----------------------------
    std::vector<std::string> mesh_name_list;
    std::queue<aiNode*> q; //遍历所有Node用
    q.push(scene->mRootNode);
    while(!q.empty()){
        auto node=q.front(); q.pop();
        for (uint32_t i = 0; i < node->mNumMeshes; i++) {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            mesh_name_list.emplace_back(mesh->mName.C_Str());
//            infof((std::string("MName: ")+mesh->mName.C_Str()).c_str());
        }
        for(uint32_t i=0;i<node->mNumChildren;i++) q.push(node->mChildren[i]);
    }

    // 需要注意，如果mesh和texture的数量大于1.它们之间的对应关系暂时还无法确定，可能会有匹配错误的情况出现
    if(mesh_name_list.size()>1||texture_filename_list.size()>1){
        warnf("Mesh Num: %d and Texture Num: %d Upper than 1.",(int)mesh_name_list.size(),(int)texture_filename_list.size());
    }
    mDirectory = modelFileName.substr(0, modelFileName.find_last_of('/'));
    for(int i=0;i<(int)mesh_name_list.size();++i){
        bindMeshTexture(mesh_name_list[i],mDirectory+'/'+texture_filename_list[i]);
    }
    infof("model:%s, scene:%s, mNumMeshes:%d, mNumMaterials:%d, mNumAnimations:%d, mNumTextures:%d", modelFileName.c_str(),scene->mName.C_Str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations, scene->mNumTextures);
    processNode(scene->mRootNode, scene);
    //=========================== 把模型标准化，防止过大或过小 ================================
    double sum_x=0,sum_y=0,sum_z=0; int count=0;
    float vmax=0;
    for(auto &mesh:*mMeshes){
        for(auto &v:mesh.second.mVertices){
            auto pos=v.Position;
            sum_x+=pos.x; sum_y+=pos.y; sum_z+=pos.z;
            ++count;
        }
    }
    infof(std::string("Average: "+std::to_string(sum_x/count)+", "+std::to_string(sum_y/count)+", "+std::to_string(sum_z/count)).c_str());
    for(auto &mesh:*mMeshes){
        for(auto &v:mesh.second.mVertices){
            auto &pos=v.Position;
            pos.x-=(sum_x/count); pos.y-=(sum_y/count); pos.z-=(sum_z/count);
            vmax=std::max(abs(pos.x),vmax);
            vmax=std::max(abs(pos.y),vmax);
            vmax=std::max(abs(pos.z),vmax);
        }
    }
    for(auto &mesh:*mMeshes){
        for(auto &v:mesh.second.mVertices){
            auto &pos=v.Position;
            pos/=vmax;
            pos/=20;
        }
        mesh.second.setupMesh();
    }
    //====================================================================
    initializeBoneNode();
    for(int i=0;i<(int)mesh_name_list.size();++i){
        activeMeshTexture(mesh_name_list[i],mDirectory+'/'+texture_filename_list[i]);
    }
    return true;
}