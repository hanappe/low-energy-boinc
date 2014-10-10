#ifndef _WINSOCKET_H_
#define _WINSOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

int winsocket_init(const char * address);

int winsocket_send(const char * data);

int winsocket_end();

#ifdef __cplusplus
}
#endif

#endif // _WINSOCKET_H_
