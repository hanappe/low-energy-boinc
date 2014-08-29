#ifndef __FDSet_hpp__
#define __FDSet_hpp__

namespace fdset {

        struct Handler {
                int m_read_fd;
                int m_write_fd;
                int m_exception_fd;

                Handler();
                virtual ~Handler();
                virtual bool keep();
                virtual void read_callback();
                virtual void write_callback();
                virtual void exception_callback();
        };

        extern void add_handler(Handler* handler);
        extern void checkpoint();

}

#endif
