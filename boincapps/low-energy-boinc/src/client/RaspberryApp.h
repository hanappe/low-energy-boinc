#ifndef RASPBERRYAPP_H
#define RASPBERRYAPP_H

#include "App.h"

struct RaspberryApp : public App {

        RaspberryApp();
        virtual ~RaspberryApp();

        virtual bool initialize();
        virtual void loop();
        virtual void finalize();

 private:

        bool _is_standalone;

}; // end class RaspberryApp

#endif // RASPBERRYAPP_H
