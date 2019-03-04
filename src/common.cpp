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

bool checkFinishHeader(const std::vector<unsigned char>& receivedHeader)
{
	if(receivedHeader.size()<4)
		return false;
	size_t s=receivedHeader.size()-1;
	if(receivedHeader[s-3]=='\r'&&receivedHeader[s-2]=='\n'&&receivedHeader[s-1]=='\r'&&receivedHeader[s]=='\n')
		return true;
	return false;
}

std::vector<unsigned char> receiveHTTPHeader(int Client, SSL *ssl)
{
	std::vector<unsigned char> receivedHeader;
	std::array<unsigned char, 1> in;
	int Len;
	while (true)
	{
		if (ssl)
			Len = SSL_read(ssl, in.data(), 1);
		else
			Len = recv(Client, in.data(), 1, 0);
		cerr<<in[0];
		// Error occurred
		if (Len <= 0)
			return receivedHeader;
		receivedHeader.push_back(in[0]);
		if(checkFinishHeader(receivedHeader))
			return receivedHeader;
	}
	printf("Error!\n");
	return receivedHeader;
}

std::optional<std::vector<unsigned char>> receiveContent(int Client, SSL *ssl)
{
	std::vector<unsigned char> Retval=receiveHTTPHeader(Client, ssl);
	string s=transfertoString(Retval);
	string length = findfield(s, "Content-Length");
	int l=0;
	if(length!="")
		l=atoi(length.c_str());
	std::array<unsigned char, 2048> in;
	int Len=s.size();
	if(l!=0)
	{
		if (ssl)
			Len += SSL_read(ssl, in.data(), l);
		else
			Len += recv(Client, in.data(), l, 0);
	}
	if (Len <= 0)
		return std::nullopt;
	for (int i = 0; i < Len-static_cast<int>(s.size()); ++i)
		Retval.push_back(in[i]);
	return Retval;
}
