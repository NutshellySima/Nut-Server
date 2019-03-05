#pragma once
#include "common.h"
#include "compress.h"
#include "stdafx.h"
#include "tls.h"

using namespace std;
namespace fs = std::filesystem;

bool checkSubstring(const string in, const string &needle);

string findfield(string &request, string needle);

bool parseRequest(string request, string &method, string &path, string &input,
                  unordered_map<string, string> &fields);

void parseuri(string &uri, string &filename, string &cgiargs, bool &isCGI);

void getFileContent(string &Content, string FileName, int &status,
                    const string method, const char *ADDR,
                    const string cgiargs, const string input,
                    bool isCGI,
                    unordered_map<string, string> *fields);

tuple<int, string, string, string, string, bool>
produceContent(string request, const char *ADDR,
               unordered_map<string, string> &fields);

string getSuffix(string filename);

string getFileType(string filename);

pair<string, string> produceAck(string request, string &method,
                                               const char *ADDR);