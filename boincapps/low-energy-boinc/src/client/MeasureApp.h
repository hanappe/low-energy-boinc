#ifndef MEASUREAPP_H
#define MEASUREAPP_H

#include "App.h"

struct MeasureApp : public App {

        MeasureApp();
        virtual ~MeasureApp();

        virtual bool initialize();
        virtual void loop();
        virtual void finalize();

 private:

        bool _is_standalone;

}; // end class MeasureApp

#endif // MEASUREAPP_H
