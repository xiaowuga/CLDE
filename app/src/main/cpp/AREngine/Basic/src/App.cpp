
#include"App.h"
#include<thread>

//ARApp& ARApp::instance()
//{
//    static ARApp *_ptr = new ARApp;
//
//    return *_ptr;
//}

int ARApp::init(const std::string& name, AppDataPtr appData, SceneDataPtr sceneData, const std::vector<ARModulePtr>& modules)
{
    this->name = name;
	this->appData = appData;
	this->sceneData = sceneData;
	this->modules = modules;

    return STATE_OK;
}


void ARApp::connectServer(const std::string& ip, int port)
{
    auto con = RPCClientConnection::create();
    con->connect(ip, port, this);

    rpcConnection=con;
}


void ARApp::run(bool grabRequired)
{
    struct AutoClean
    {
        ARApp* _app;
        AutoClean(ARApp *app)
            :_app(app)
        {}
        ~AutoClean()
        {
            auto con = _app->rpcConnection;
            if (con)
            {
                SerilizedObjs cmd = {
                    {"#cmd","close"}
                };
                
                //con->writeBlock(cmd.encode());
                _app->postRemoteCall(nullptr, nullptr, cmd);
                con->close();
            }
        }
    };
    AutoClean _autoClean(this);

    {
        FrameDataPtr frameData = std::make_shared<FrameData>();

        for (auto& m : this->modules)
        {
            m->app = this;

            _ARENGINE_LOCK_MODULE(m)
            if (m->Init(*appData, *sceneData, frameData) != STATE_OK)
                return;
        }
    }

    uint frameID = 0;
    while (appData->_continue)
    {
        auto _frameData=std::make_shared<FrameData>();
        _frameData->frameID = ++frameID;

        size_t mi = 0;
        for (; mi < modules.size(); ++mi)
        {
            auto &mptr=modules[mi];
            _ARENGINE_LOCK_MODULE(mptr)
            if (mptr->Update(*appData,  *sceneData, _frameData) != STATE_OK)
                goto shutDown;
            else if (!_frameData->image.empty()) //if input done
                break;
        }

        if (rpcConnection)
        {
            int nBuffered = rpcConnection->nBufferedPostedFrames();
            //printf("#nBufferedPostedFrames==%d\n", nBuffered);

            if (_maxBufferedPostedFrames <= 0 || nBuffered < _maxBufferedPostedFrames)
            {
                auto serilizedFrame = std::make_shared<SerilizedFrame>();
                std::vector<RemoteProcPtr>  procs;
                for (size_t i = mi + 1; i < modules.size(); ++i)
                {
                    _ARENGINE_LOCK_MODULE(modules[i])
                        if (modules[i]->CollectRemoteProcs(*serilizedFrame, procs, _frameData) != STATE_OK)
                            goto shutDown;
                }
                if (!serilizedFrame->empty() || !procs.empty())
                {
                    rpcConnection->post(*serilizedFrame, _frameData, procs);
                }
                _frameData->serilizedFramePtr = serilizedFrame;
            }
        }

        for (size_t i = mi+1; i < modules.size(); ++i)
        {
            _ARENGINE_LOCK_MODULE(modules[i])
            if (modules[i]->Update(*appData, *sceneData, _frameData) != STATE_OK)
                goto shutDown;
        }
        std::shared_lock<std::shared_mutex> _lock(dataMutex);
        frameData= _frameData;

        if(grabRequired)
        {
            std::lock_guard<std::mutex>  _lock(this->_grabMutex);

            GrabbedData  gd;
            for(auto &ft : this->_grabFunctors)
            {
                std::shared_ptr<IGrabFunctor> f(ft->clone());
                f->grab(this, frameData.get());
                gd.datas.push_back(f);
            }

            _grabbedData=gd;
        }
       // break;
    }

shutDown:
    for (auto& m : modules) {
        m->ShutDown(*appData, *sceneData);
    }

   // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

}

void ARApp::start()
{
    _runThread=std::shared_ptr<std::thread>(
            new std::thread([this](){
                this->run(true);}
            ));
}

void ARApp::stop()
{
    if(_runThread)
    {
        appData->_continue=false;
        _runThread->join();
        _runThread=nullptr;
    }
}

int ARApp::addGrabFunctor(std::shared_ptr<IGrabFunctor> fptr)
{
    std::lock_guard<std::mutex>  _lock(this->_grabMutex);

    int id=-1;
    if(fptr)
    {
        id=(int)_grabFunctors.size();
        _grabFunctors.push_back(fptr);
    }
    return id;
}

ARApp::GrabbedData ARApp::getGrabbed()
{
    std::lock_guard<std::mutex>  _lock(this->_grabMutex);
    return _grabbedData;
}

std::shared_ptr<ARApp::IGrabFunctor> ARApp::getGrabbed(int id)
{
    std::lock_guard<std::mutex>  _lock(this->_grabMutex);
    if(uint(id)>=_grabbedData.datas.size())
        return nullptr;

    return _grabbedData.datas[id];
}

int ARApp::call(ARModule *module, int cmd, std::any &data)
{
    if(module)
    {
        _ARENGINE_LOCK_MODULE(module)
        return module->ProCommand(cmd,data);
    }
    return STATE_ERROR;
}

ARModulePtr  ARApp::getModule(const std::string &name)
{
    ARModulePtr  ptr=nullptr;
    for(auto &m : this->modules)
        if(m && m->getModuleName()==name)
        {
            ptr=m;
            break;
        }
    return ptr;
}

