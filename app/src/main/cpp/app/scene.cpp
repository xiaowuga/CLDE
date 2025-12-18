
#include "scene.h"


std::shared_ptr<IScene> createScene(const std::string &name, IApplication *app){

    std::shared_ptr<IScene> _createScene_AppVer2();
    std::shared_ptr<IScene> _createScene_CLDE();


    struct DFunc{
        std::string name;
        typedef std::shared_ptr<IScene> (*createFuncT)();
        createFuncT  create;
    };

    DFunc funcs[]= {
            {"AppVer2",_createScene_AppVer2},
            {"CLDE", _createScene_CLDE}
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




