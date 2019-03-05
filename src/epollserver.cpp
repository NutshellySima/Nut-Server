#include "epollsocket.h"
#include "common.h"

using namespace std;
namespace fs = std::filesystem;

void setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    int ret=fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

struct Con
{
    string header;
    bool hasheader = false;
    string body;
    size_t leftsize = 0;
    bool hasbody = false;
    string send;
    bool produced = false;
    size_t sent = 0;
};

unordered_map<int, Con> cons;

void receiveEpollHTTPHeader(int Client)
{
    char in;
    int Len;
    while (true)
    {
        Len = recv(Client, &in, 1, 0);
        // Error occurred
        if (Len <= 0)
            return;
        cons[Client].header.push_back(in);
        if (checkFinishHeader(cons[Client].header))
        {
            cons[Client].hasheader = true;
            return;
        }
    }
}

void receiveEpollContent(int Client)
{
    std::array<char, 2048> in;
    while (true)
    {
        if(cons[Client].leftsize==0)
            cons[Client].hasbody = true;
        int Len = recv(Client, in.data(), cons[Client].leftsize, 0);
        if (Len <= 0)
            return;
        for (int i = 0; i < Len; ++i)
            cons[Client].body.push_back(in[i]);
        cons[Client].leftsize -= Len;
    }
    cons[Client].hasbody = true;
}

void produceEpollClientRequest(const int Client)
{
    string MessageStr;
    string method_tmp, path, input; // Here we shadow method
    unordered_map<string, string> fields;
    parseRequest(cons[Client].header, method_tmp, path, input, fields);
    MessageStr = "HTTP/1.1 301 Moved Permanently\r\n";
    MessageStr += "Connection: keep-alive\r\n";
    MessageStr += "Content-length: 0\r\n";
    MessageStr += "Location: https://" + fields["Host"] + path + "\r\n";
    MessageStr += "\r\n";
    cons[Client].send = MessageStr;
}

void handleClose(int Client, int epfd)
{
    cons.erase(Client);
    epoll_ctl(epfd, EPOLL_CTL_DEL, Client, nullptr);
    close(Client);
}

void handleSend(int Client, int epfd)
{
    if (cons[Client].produced == false)
        return;
    while (cons[Client].sent < cons[Client].send.size())
    {
        int val = send(Client, cons[Client].send.data() + cons[Client].sent, cons[Client].send.size() - cons[Client].sent, MSG_DONTWAIT);
        if (val == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        else if (val > 0)
            cons[Client].sent += val;
        else
            handleClose(Client, epfd);
    }
    cons[Client] = Con();
}

void handleRead(int Client, int epfd)
{
    // Assume no pipelining
    if (!cons[Client].hasheader)
        receiveEpollHTTPHeader(Client);
    if (cons[Client].hasheader && cons[Client].header.size() == 0)
    {
        handleClose(Client,epfd);
        return;
    }
    if (cons[Client].hasheader && !cons[Client].hasbody && cons[Client].leftsize == 0)
    {
        string s = cons[Client].header;
        string length = findfield(s, "Content-Length");
        if (length != "")
            cons[Client].leftsize = atoi(length.c_str());
    }
    if (cons[Client].hasheader && !cons[Client].hasbody)
        receiveEpollContent(Client);
    if (cons[Client].produced == false && cons[Client].hasbody)
    {
        cons[Client].produced = true;
        produceEpollClientRequest(Client);
        handleSend(Client, epfd);
    }
}

void handleEpollSocket(int Server)
{
    int epfd = epoll_create1(0);
    struct epoll_event ev, evs[128];
    setnonblocking(Server);
    ev.data.fd = Server;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, Server, &ev);
    while (true)
    {
        int cnt = epoll_wait(epfd, evs, 128, -1);
        for (int i = 0; i < cnt; ++i)
        {
            if (evs[i].data.fd == Server)
            {
                // Client handle
                int Client;
                struct sockaddr_in6 ClientAddr;
                socklen_t ClientAddrSize = sizeof(ClientAddr);
                Client = accept(Server, (struct sockaddr *)&ClientAddr, &ClientAddrSize);
                if (Client == -1)
                    continue;
                setnonblocking(Client);
                ev.data.fd = Client;
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                cons[Client] = Con();
                epoll_ctl(epfd, EPOLL_CTL_ADD, Client, &ev);
            }
            else
            {
                int event = evs[i].events;
                if (event & EPOLLIN)
                    handleRead(evs[i].data.fd, epfd);
                else if (event & (EPOLLOUT | EPOLLRDHUP))
                    handleSend(evs[i].data.fd, epfd);
                else if (event & (EPOLLHUP | EPOLLERR))
                    handleClose(evs[i].data.fd, epfd);
            }
        }
    }
    close(epfd);
}