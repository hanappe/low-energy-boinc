#include "App.h"

App::App() :
        mIsInitialized(false),
        mDebug(false)
{}

App::~App() {
}

bool App::isInitialized() const {
        return mIsInitialized;
}

void App::setDebug(bool b) {
        mDebug = b;
}

bool App::debug() const {
        return mDebug;
}

/*

App* App::Create(Type t) {

        App * app = 0;
    
        switch(t) {
                
        case Boinc: {
                app = new BoincApp(); 
                break;
        }
                
        case Standalone: {
                app = new StandaloneApp(); 
                break;
        }
        case Raspberry {
                app = new RaspberryApp(); 
                break;
        }
                
        default: {
                std::cerr << "App::Create, error: Unkown App type" << t << std::endl;
                break;
        }
        }

        assert(app);
        return app;
}

*/
