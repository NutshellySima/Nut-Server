#pragma once
#include "stdafx.h"
#include "tls.h"
using namespace std;

int createSocket(int port);


void closeSocket(int Server);

std::optional<std::vector<unsigned char>> receiveContent(int Client, SSL *ssl);
string findfield(string &request, string needle);

template <typename T> string transfertoString(T Context) {
  string ret;
  for (auto i : Context)
    ret += i;
  return ret;
}