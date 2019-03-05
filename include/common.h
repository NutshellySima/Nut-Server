#pragma once
#include "stdafx.h"
#include "tls.h"
using namespace std;

int createSocket(int port);


void closeSocket(int Server);

string receiveContent(int Client, SSL *ssl);
string findfield(string &request, string needle);
bool checkFinishHeader(const string& receivedHeader);