#include "src/EventLoop.h"
#include "src/Server.h"
#include "MemoryPool/MemoryAlloc.h"

int main() {
    void* loopMemory = MemoryAlloc(sizeof(EventLoop));
    EventLoop *loop = new (loopMemory) EventLoop();

    void* serverMemory = MemoryAlloc(sizeof(Server));
    Server *server = new (serverMemory) Server(loop);

    loop->loop();

    server->~Server();
    MemoryFree(serverMemory);
    loop->~EventLoop();
    MemoryFree(loopMemory);
    
    return 0;
}
