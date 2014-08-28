#ifndef BOINCAPP_H
#define BOINCAPP_H

#include "App.h"
#include <string>

struct BoincApp : public App {

        BoincApp();
        virtual ~BoincApp();

        virtual bool initialize();
        virtual void loop();
        virtual void finalize();

 private:

        std::string _out_s;
        bool _is_standalone;
        long _milestones;
        double _seconds;

        double _t0;
        double _t1;
        long _start_milestone;

}; // end class BoincApp

#endif // BOINCAPP_H
