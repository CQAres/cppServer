#pragma once
#include <functional>

class Epoll;
class Channel;
class EventLoop
{
private:
    // 用于管理文件描述符的事件监听
    Epoll *ep;
    // 控制事件循环的运行状态
    bool quit;
public:
    EventLoop();
    ~EventLoop();

    void loop();
    void updateChannel(Channel*);

    void addThread(std::function<void()>);
};

