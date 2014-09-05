#ifndef APP_H
#define APP_H


struct App {
        

        enum Type {
                // Setup Boinc sensors and send data to database
                Boinc,
                // Setup all but Boinc sensors, send data to database and store it into local files
                //Standalone,
                // Setup only Wattsup sensor and Temperature sensor, send data to database
                RaspberryPi,
                // Setup all but Boinc sensors and benchmark app
                Measure
        };

        //static App * Create(Type t);        
public:
        App();
        virtual ~App();
        

        virtual bool initialize() = 0;
        virtual void loop() = 0;
        virtual void finalize() = 0;

        bool isInitialized() const;

        void setDebug(bool b);
        bool debug() const;

protected:
        bool mIsInitialized;
        bool mDebug;

}; // end class App

#endif // APP_H
