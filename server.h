//
// Created by charleshan on 2022/5/6.
//
#include <iostream>
#include <vector>
#include <set>
#include <functional>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#ifndef CHARLES_NET_SERVER_H
#define CHARLES_NET_SERVER_H

class charles_epoll {
private:
    int m_fd;
    int m_size;
    std::vector<epoll_event> m_events;
public:
    charles_epoll(int size){
        create(size);
    };
    ~charles_epoll(){
        close(m_fd);
    };
    void create(int size){
        m_fd = epoll_create(size);
        m_events = std::vector<epoll_event>(size, epoll_event{});
    }
    // epoll fd添加监听对象
    int add(epoll_event event){
        int ret = epoll_ctl(m_fd, EPOLL_CTL_ADD, event.data.fd, &event);
        if (ret == -1) {
            std::cout<<"epoll_ctl add error"<<std::endl;
        }
        return ret;
    }

    int free(epoll_event event) {
        int ret = epoll_ctl(m_fd, EPOLL_CTL_DEL, event.data.fd, &event);
        if (ret == -1) {
            std::cout<<"epoll_ctl free error"<<std::endl;
        }
        return ret;
    }

    std::pair<int, std::vector<epoll_event>&> wait() {
        int count = epoll_wait(m_fd, m_events.data(), m_events.size(), -1);
        return std::make_pair(count, std::ref(m_events));
    }
    int get(){
        return m_fd;
    }
};

class charles_socket{
private:
    int m_fd = 0;
public:
    charles_socket() = delete;
    charles_socket(const charles_socket& obj) = delete;
    charles_socket& operator=(const charles_socket &rhs) = delete;
    charles_socket(int sock):m_fd(sock){
    }
    // 不支持拷贝，只支持移动
    charles_socket& operator=(charles_socket &&rhs) noexcept {
        if(this == &rhs) {
            return *this;
        }
        this->m_fd = rhs.m_fd;
        rhs.cleanup();
        return *this;
    }
    charles_socket(charles_socket &&rhs) noexcept{
        this->m_fd = rhs.m_fd;
        rhs.cleanup();
    }
    ~charles_socket(){
        if(m_fd) {
            close(m_fd);
        }
    }
    int get(){
        return m_fd;
    }
    void cleanup() {
        m_fd = 0;
    }
    bool operator < (const charles_socket &obj) const {
        if (m_fd < obj.m_fd) {
            return true;
        }
        return false;
    }
};

// 封装tcp socket
class server {
private:
    // 服务端socket
    charles_socket serv_sock = -1;
    // 客户端连接socket
    std::set<charles_socket> clnt_socks;
    // 服务器端地址
    sockaddr_in serv_adr;
    charles_epoll epoll_handle;
    std::vector<char> buf;
public:
    server(unsigned port): epoll_handle(5), buf(20){
        serv_sock = socket(PF_INET, SOCK_STREAM, 0);
        serv_adr.sin_family = AF_INET;
        serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_adr.sin_port = htons(port);
        if (bind(serv_sock.get(), (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1){
            std::cout<<"socket bind error"<<std::endl;
            return;
        }
        std::cout<<"bind success"<<std::endl;
        if (listen(serv_sock.get(), 5) == -1) {
            std::cout<<"socket listen error"<<std::endl;
            return;
        }
        std::cout<<"listen success"<<std::endl;
        epoll_event server_event;
        server_event.events = EPOLLIN;
        server_event.data.fd = serv_sock.get();
        epoll_handle.add(server_event);

        while(true) {
            auto result = epoll_handle.wait();
            if(result.first == -1) {
                std::cout<<"epoll wait error"<<std::endl;
                return;
            }
            for (int i = 0;i<result.first;i++) {
                // 如果是服务器端接收请求的时候
                if (result.second[i].data.fd == serv_sock.get()){
                    sockaddr_in clnt_addr;
                    socklen_t adr_sz;
                    auto clnt_sock = accept(serv_sock.get(), (struct sockaddr *)&clnt_addr, &adr_sz);
                    epoll_event clnt_event;
                    clnt_event.events = EPOLLIN;
                    clnt_event.data.fd = clnt_sock;
                    // 然后开始监听客户端的连接
                    charles_socket charles_clnt_socket(clnt_sock);
                    clnt_socks.emplace(std::move(charles_clnt_socket));
                    epoll_handle.add(clnt_event);
                    std::cout<<"accept client socket: "<<clnt_sock<<std::endl;
                }else{
                    auto client_fd = result.second[i].data.fd;
                    // 处理客户端的请求，先暂时简单处理，直接打印(就是这里，待会放到线程池中进行处理，现在暂且先单线程进行处理)
                    // 果然是这样，客户端发送了len个长度的消息之后，即使你自己设置的buf不足以接纳这些内容，你依然是可以根据epoll回调来读取接下来的消息的
                    int str_len = ::read(client_fd, buf.data(), buf.size());
                    if (str_len == 0) {
                        epoll_ctl(epoll_handle.get(), EPOLL_CTL_DEL, client_fd, NULL);// 删除套接字
                        ::close(client_fd);
                        clnt_socks.erase(client_fd);
                        std::cout<<"close client: "<<client_fd<<std::endl;
                    }else{
                        // 向客户端原路返回
                        std::cout<<"write to client: ";
                        for(int i = 0;i<str_len;i++) {
                            std::cout<<buf[i];
                        }
                        std::cout<<std::endl;
                        ::write(client_fd, buf.data(), str_len);
                    }
                }
            }
        }
    };
    ~server(){
    };
    // 释放某个客户端的socket
    void close(){
    }
    // 接受某个客户端发送的消息
    void accecpt(){
    }
    // 向某个客户端发送消息
    void write(){
    }
};


#endif //CHARLES_NET_SERVER_H
