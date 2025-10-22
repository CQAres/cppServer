#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include <vector>

EventLoop::EventLoop() : ep(nullptr), quit(false){
    ep = new Epoll();
}

EventLoop::~EventLoop(){
    delete ep;
}


void EventLoop::loop(){
    while(!quit){
    std::vector<Channel*> chs;
        // 获取活跃事件通道
        // 每个 Channel 对象代表一个事件源（如一个套接字）
        chs = ep->poll();
        for(auto it = chs.begin(); it != chs.end(); ++it){
            // 逐个处理事件
            (*it)->handleEvent();
        }
    }
}

void EventLoop::updateChannel(Channel *ch){
    ep->updateChannel(ch);
}

