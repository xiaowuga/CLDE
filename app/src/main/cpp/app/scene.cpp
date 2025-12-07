
#include "scene.h"


std::shared_ptr<IScene> createScene(const std::string &name, IApplication *app){

    std::shared_ptr<IScene> _createScene_AppVer2();
    std::shared_ptr<IScene> _createScene_BuildMap();
    std::shared_ptr<IScene> _createScene_MarkerLocation();


    struct DFunc{
        std::string name;
        typedef std::shared_ptr<IScene> (*createFuncT)();
        createFuncT  create;
    };

    DFunc funcs[]= {
            {"AppVer2",_createScene_AppVer2},
            {"BuildMap", _createScene_BuildMap}
    };

    std::shared_ptr<IScene> ptr=nullptr;
    for(auto &f : funcs){
        if(f.name==name) {
            ptr = f.create();
            break;
        }
    }
    if(ptr){
        ptr->m_app=app;
        ptr->m_program=app? app->m_program : nullptr;
    }

    return ptr; //原来是return nullptr,可能是写错了
}


class SceneEmpty
        :public IScene
{
public:
    virtual bool initialize(const XrInstance instance, const XrSession session) {
        return true;
    }
};

std::shared_ptr<IScene> _createScene_empty()
{
    return std::make_shared<SceneEmpty>();
}

