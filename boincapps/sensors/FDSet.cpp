#include <iostream>
#include <vector>
#include <algorithm>
#include <sys/select.h>
#include "FDSet.hpp"

using namespace std;

namespace fdset {

        static vector<Handler*> handlers;

        void add_handler(Handler* handler) {
                cout << "add_handler " << handler << endl;
                remove(handlers.begin(), handlers.end(), handler);
                handlers.push_back(handler);
        }

        static void remove_handler(Handler* handler) {
                cout << "remove_handler " << handler << endl;
                remove(handlers.begin(), handlers.end(), handler);
        }

        void checkpoint() {
                fd_set read_set;
                FD_ZERO(&read_set);

                fd_set write_set;
                FD_ZERO(&write_set);

                fd_set exception_set;
                FD_ZERO(&exception_set);

                int max_fd = -1;

                vector<Handler*> handlers_ = handlers;
                size_t nhandlers = handlers_.size();

                for (size_t i = 0; i < nhandlers; i++) {
                        Handler* handler = handlers_[i];
                        if (!handler->keep()) {
                                remove_handler(handler);
                                handlers_[i] = 0;
                                continue;
                        }

                        if (handler->m_read_fd >= 0) {
                                FD_SET(handler->m_read_fd, &read_set);
                                max_fd = (max_fd < handler->m_read_fd) ? handler->m_read_fd : max_fd;
                        }

                        if (handler->m_write_fd >= 0) {
                                FD_SET(handler->m_write_fd, &write_set);
                                max_fd = (max_fd < handler->m_write_fd) ? handler->m_write_fd : max_fd;
                        }

                        if (handler->m_exception_fd >= 0) {
                                FD_SET(handler->m_exception_fd, &exception_set);
                                max_fd = (max_fd < handler->m_exception_fd) ? handler->m_exception_fd : max_fd;
                        }
                }

                if (max_fd == -1) return;

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 0;

                int res = select(max_fd + 1, &read_set, &write_set, &exception_set, &tv);

                if (res < 0) {
                        // TODO: manage error.
                        return;
                }

                for (size_t i = 0; i < nhandlers; i++) {
                        Handler* handler = handlers_[i];
                        if (handler == 0) continue;

                        if (FD_ISSET(handler->m_read_fd, &read_set)) {
                                handler->read_callback();
                        }

                        if (FD_ISSET(handler->m_write_fd, &write_set)) {
                                handler->write_callback();
                        }

                        if (FD_ISSET(handler->m_exception_fd, &exception_set)) {
                                handler->exception_callback();
                        }
                }
        }

        Handler::Handler() {
                m_read_fd = -1;
                m_write_fd = -1;
                m_exception_fd = -1;
        }

        Handler::~Handler() {
                remove_handler(this);
        }

        bool Handler::keep() {
                return true;
        }

        void Handler::read_callback() {
        }

        void Handler::write_callback() {
        }

        void Handler::exception_callback() {
        }

}

