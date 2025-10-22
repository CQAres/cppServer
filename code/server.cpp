#include "src/EventLoop.h"
#include "src/Server.h"
#include "MemoryPool/MemoryAlloc.h"

int main() {
    EventLoop *loop = new EventLoop();
    Server *server = new Server(loop);
    loop->loop();
    return 0;
}
