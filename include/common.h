#pragma once
#include "stdafx.h"
#include "tls.h"
using namespace std;

int createSocket(int port);


void closeSocket(int Server);

std::optional<std::vector<unsigned char>> receiveContent(int Client, SSL *ssl);
