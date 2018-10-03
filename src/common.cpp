#include "stdafx.h"
#include "tls.h"
#include "common.h"

int createSocket(int port)
{
	// Server handle
	const int ServerPort = port;
	int Server = socket(PF_INET6, SOCK_STREAM, 0);
	if (Server == -1)
	{
		cerr << "socket() err\n";
		abort();
	}
	int optval = 1;
	int res = setsockopt(Server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1)
		cerr << "setsockopt() - reuse addr error\n";
	int qlen = 128;
	res = setsockopt(Server, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));
	if (res == -1)
		cerr << "setsockopt() - tcpfastopen error\n";
	struct sockaddr_in6 ServerAddr;
	memset(&ServerAddr, 0, sizeof(ServerAddr));
	ServerAddr.sin6_family = AF_INET6;
	ServerAddr.sin6_addr = in6addr_any;
	ServerAddr.sin6_port = htons(ServerPort);
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	res = setsockopt(Server, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
	if (res == -1)
		cerr << "setsockopt() - recvtimeout error\n";
	res = setsockopt(Server, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);
	if (res == -1)
		cerr << "setsockopt() - sendtimeout error\n";
	int BindRes = ::bind(Server, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr));
	if (BindRes == -1)
	{
		cerr << "bind err - no sudo ???\n";
		abort();
	}
	int ListenRes = listen(Server, 128);
	if (ListenRes == -1)
	{
		cerr << "listen err\n";
		abort();
	}
	return Server;
}


void closeSocket(int Server)
{
	int ServerCloseRes = close(Server);
	if (ServerCloseRes == -1)
	{
		cerr << "Server Close\n";
		abort();
	}
}

std::optional<std::vector<unsigned char>> receiveContent(int Client, SSL *ssl)
{
	std::array<unsigned char, 2048> in;
	int Len;
	if (ssl)
		Len = SSL_read(ssl, in.data(), 2047);
	else
		Len = recv(Client, in.data(), 2047, 0);
	if (Len <= 0)
		return std::nullopt;
	std::vector<unsigned char> Retval;
	Retval.reserve(Len);
	for (int i = 0; i < Len; ++i)
		Retval.push_back(in[i]);
	return Retval;
}
