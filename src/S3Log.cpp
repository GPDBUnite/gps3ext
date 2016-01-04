#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdio>
#include <cstdarg>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <netinet/in.h>
// #include <thread>

#include <vector>
#include <memory>
using std::vector;
using std::shared_ptr;
using std::make_shared;
#include "utils.h"

#include "S3Log.h"
#include "gps3conf.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

uint8_t loglevel() { return s3conf_loglevel; }

// fake implement
void _LogMessage(const char* fmt, va_list args) { vfprintf(stderr, fmt, args); }

void _send_to_local(const char* fmt, va_list args) {
    char buf[1024];
    int len = vsnprintf(buf, 1024, fmt, args);
    buf[len - 1] = 0;
    sendto(s3conf_logsock_local, buf, len, 0,
           (struct sockaddr*)&s3conf_logserverpath, sizeof(struct sockaddr_un));
}

void _send_to_remote(const char* fmt, va_list args) {
    char buf[1024];
    int len = vsnprintf(buf, 1024, fmt, args);
    buf[len - 1] = 0;
    sendto(s3conf_logsock_udp, buf, len, 0,
           (struct sockaddr*)&s3conf_logserveraddr, sizeof(struct sockaddr_in));
}

void LogMessage(LOGLEVEL loglevel, const char* fmt, ...) {
    if (loglevel > s3conf_loglevel) return;
    va_list args;
    va_start(args, fmt);
    switch (s3conf_logtype) {
        case INTERNAL_LOG:
            _LogMessage(fmt, args);
            break;
        case STDERR_LOG:
            vfprintf(stderr, fmt, args);
            break;
        case REMOTE_LOG:
            _send_to_remote(fmt, args);
            break;
        case LOCAL_LOG:
            _send_to_local(fmt, args);
            break;
        default:
            break;
    }
    va_end(args);
}

#ifdef DEBUGS3
static bool loginited = false;
void InitLog() {
    if (!loginited) {
        s3conf_logsock_local = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (s3conf_logsock_local < 0) {
            perror("create socket fail");
        }

        /* start with a clean address structure */
        memset(&s3conf_logserverpath, 0, sizeof(struct sockaddr_un));
        s3conf_logserverpath.sun_family = AF_UNIX;
        snprintf(s3conf_logserverpath.sun_path, UNIX_PATH_MAX,
                 s3conf_logpath.c_str());

        s3conf_logsock_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s3conf_logsock_udp < 0) {
            perror("create socket fail");
        }

        memset(&s3conf_logserveraddr, 0, sizeof(struct sockaddr_in));
        s3conf_logserveraddr.sin_family = AF_INET;
        s3conf_logserveraddr.sin_port = htons(s3conf_logserverport);
        inet_aton(s3conf_logserverhost.c_str(), &s3conf_logserveraddr.sin_addr);

        loginited = true;
    }
}
#endif

#if 0
int main() {

    S3DEBUG("hello");
	return 0;
}

        si_logserver.sin_family = AF_INET;
        si_logserver.sin_port = htons(1111);
        if (inet_aton("127.0.0.1", &si_logserver.sin_addr) == 0) {  // error
            // log error here
            return;
        }
        logsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

#endif
