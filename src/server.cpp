#include "common.h"
#include "compress.h"
#include "stdafx.h"
#include "tls.h"

using namespace std;
namespace fs = std::filesystem;

template <typename T>
bool checkSubstring(T ClientRequest, const std::string &needle)
{
  bool Enable = true;
  auto in = ClientRequest;
  auto it = std::search(in.begin(), in.end(), needle.begin(), needle.end());
  if (it == in.end())
    Enable = false;
  return Enable;
}

string findfield(string &request, string needle)
{
  string finder = needle + ": .*\r\n";
  std::regex r(finder);
  auto begin = std::sregex_iterator(request.begin(), request.end(), r);
  auto end = std::sregex_iterator();
  if (begin != end)
  {
    string ret = begin->str();
    // strip \r\n
    ret.pop_back();
    ret.pop_back();
    ret = ret.substr(needle.size() + 2);
    return ret;
  }
  return "";
}

bool parseRequest(string request, string &method, string &path, string &input,
                  unordered_map<string, string> &fields)
{
  string needle = "\r\n\r\n";
  auto it =
      std::search(request.begin(), request.end(), needle.begin(), needle.end());
  if (it != request.end())
  {
    advance(it, 4);
    input = request.substr(it - request.begin());
  }
  fields["Cookie"] = findfield(request, "Cookie");
  fields["User-Agent"] = findfield(request, "User-Agent");
  fields["Host"] = findfield(request, "Host");
  fields["Accept"] = findfield(request, "Accept");
  fields["Accept-Encoding"] = findfield(request, "Accept-Encoding");
  fields["Accept-Language"] = findfield(request, "Accept-Language");
  fields["CF-IPCountry"] = findfield(request, "CF-IPCountry");
  fields["CF-Connecting-IP"] = findfield(request, "CF-Connecting-IP");
  fields["X-Forwarded-For"] = findfield(request, "X-Forwarded-For");
  fields["X-Forwarded-Proto"] = findfield(request, "X-Forwarded-Proto");
  fields["CF-RAY"] = findfield(request, "CF-RAY");
  fields["CF-Visitor"] = findfield(request, "CF-Visitor");
  fields["Referer"] = findfield(request, "Referer");
  fields["Origin"] = findfield(request, "Origin");
  fields["Content-Length"] = findfield(request, "Content-Length");
  fields["Content-Type"] = findfield(request, "Content-Type");
  std::stringstream buf(request);
  buf >> method;
  buf >> path;
  if ((buf.rdstate() & std::stringstream::failbit) != 0)
    return false;
  return true;
}

void parseuri(string &uri, string &filename, string &cgiargs, bool &isCGI)
{
  uri = regex_replace(uri, std::regex("%20"), " ");
  filename = "." + uri;
  isCGI = checkSubstring(filename, "/cgi-bin");
  if (!isCGI && filename.back() == '/')
    filename += "index.html";
  if (!isCGI)
    cgiargs = "";
  else
  {
    auto pos = filename.find_first_of('?');
    if (pos != std::string::npos)
    {
      cgiargs = filename.substr(pos + 1);
      filename = filename.substr(0, pos);
    }
  }
}

void getFileContent(string &Content, string FileName, int &status,
                    const string method = "", const char *ADDR = nullptr,
                    const string cgiargs = "", const string input = "",
                    bool isCGI = false,
                    unordered_map<string, string> *fields = nullptr)
{
  // First we clear the Content inside.
  Content.clear();
  // map a place for the child process to store validation status.
  char *FileValidationCode = static_cast<char *>(mmap(
      nullptr, 4, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  pid_t Status = fork();
  if (Status > 0)
  {
    // Parent
    int childstatus = 0;
    waitpid(Status, &childstatus, 0);
    if (childstatus != 0)
      status = 500;
  }
  else if (Status == 0)
  {
    // Child
    auto setStatus = [&](const char *S) {
      FileValidationCode[0] = S[0];
      FileValidationCode[1] = S[1];
      FileValidationCode[2] = S[2];
    };
    // chroot
    if (chroot("./pages") == -1)
    {
      cerr << "chroot\n";
      setStatus("500");
      exit(-1);
    }
    if (chdir("/") == -1)
    {
      cerr << "chdir\n";
      setStatus("500");
      exit(-1);
    }
    struct stat sbuf;
    if (stat(FileName.c_str(), &sbuf) < 0)
    {
      setStatus("404");
      exit(0);
    }
    if (!isCGI)
    { // static
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
      {
        setStatus("403");
        exit(0);
      }
    }
    else
    { // Dynamic
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
      {
        setStatus("403");
        exit(0);
      }
    }
    string CurrentDir = string(fs::current_path());
    string CanonicalFile = string(fs::canonical(fs::path(FileName)));
    if (CanonicalFile.find(CurrentDir) != 0)
    {
      setStatus("403");
      exit(0);
    }
    setStatus("200");
    exit(0);
  }
  else
  {
    puts("fork failed!");
    exit(-1);
  }
  assert(Status > 0 && "Only parent process can reach here");
  int retstatus = atoi(static_cast<char *>(FileValidationCode));
  // unmap the filevalidationcode.
  munmap(FileValidationCode, 4);
  if (retstatus != 200)
  {
    status = retstatus;
    return;
  }
  // Here HTTP status is 200
  int pipes[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) == -1)
  {
    puts("AF_UNIX failed");
    exit(-1);
  }
  ssize_t count = 0;
  char buf[4096];
  Status = fork();
  if (Status > 0)
  {
    close(pipes[0]);
    if (isCGI && method == "POST")
    {
      int wres = write(pipes[1], input.data(), input.size());
      (void)wres;
    }
    shutdown(pipes[1], SHUT_WR);
    while ((count = read(pipes[1], buf, 4096)) != -1)
    {
      if (count == 0)
        break;
      for (ssize_t i = 0; i < count; ++i)
        Content.push_back(buf[i]);
    }
    close(pipes[1]);
    int childstatus = 0;
    waitpid(Status, &childstatus, 0); // child is dead
    if (childstatus != 0)
      status = 500;
  }
  else if (Status == 0)
  {
    close(pipes[1]);
    if (chdir("./pages") == -1)
    {
      cerr << "chdir\n";
      exit(-1);
    }
    if (!isCGI)
    {
      // chroot
      if (chroot(".") == -1)
      {
        cerr << "chroot\n";
        exit(-1);
      }
      int FileData = open(FileName.c_str(), O_RDONLY);
      if (FileData == -1)
      {
        cerr << "open\n";
        exit(-1);
      }
      struct stat sbuf;
      fstat(FileData, &sbuf);
      int res = sendfile(pipes[0], FileData, nullptr, sbuf.st_size);
      (void)res;
      close(FileData);
      close(pipes[0]);
      exit(0);
    }
    else
    {
      setenv("QUERY_STRING", cgiargs.c_str(), 1);
      setenv("SERVER_PORT", "443", 1);
      string scriptname = FileName.substr(1);
      setenv("SCRIPT_NAME", scriptname.c_str(), 1);
      setenv("SERVER_SOFTWARE", "Nut-Server", 1);
      setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
      setenv("REQUEST_METHOD", method.c_str(), 1);
      setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
      setenv("HTTPS", "on", 1);
      if (fields != nullptr)
      {
        auto setter = [&](string Env, string key) {
          if (fields->find(key) != fields->end())
            setenv(Env.c_str(), (*fields)[key].c_str(), 1);
        };
        setter("HTTP_COOKIE", "Cookie");
        setter("HTTP_HOST", "Host");
        setter("HTTP_REFERER", "Referer");
        setter("HTTP_USER_AGENT", "User-Agent");
        setter("HTTP_ACCEPT_LANGUAGE", "Accept-Language");
        setter("HTTP_ACCEPT", "Accept");
        setter("CONTENT_LENGTH", "Content-Length");
        setter("CONTENT_TYPE", "Content-Type");
      }
      assert(ADDR != nullptr && "ADDR == nullptr!");
      setenv("REMOTE_ADDR", ADDR, 1);
      dup2(pipes[0], STDOUT_FILENO);
      dup2(pipes[0], STDIN_FILENO);
      close(pipes[0]);
      char *emptylist[] = {nullptr};
      int execres = execve(FileName.c_str(), emptylist, environ);
      if (execres == -1)
      {
        cerr << "exec failed!" << endl;
        exit(-1);
      }
    }
  }
  else
  {
    puts("fork failed!");
    exit(-1);
  }
  assert(Status > 0 && "Only parent process can reach here");
}

tuple<int, string, string, string, string, bool>
produceContent(string request, const char *ADDR,
               unordered_map<string, string> &fields)
{
  string method, path, filename, content, cgiargs, input;
  bool isCGI = false;
  int status = 200;
  bool valid = parseRequest(request, method, path, input, fields);
  if (valid)
  {
    parseuri(path, filename, cgiargs, isCGI);
    getFileContent(content, filename, status, method, ADDR, cgiargs, input,
                   isCGI, &fields);
  }
  if (!valid)
  {
    getFileContent(content, "400.html", status);
    status = 400;
  }
  else if (method != "GET" && method != "HEAD" && method != "POST")
  {
    getFileContent(content, "501.html", status);
    status = 501;
  }
  else if (status != 200)
  {
    getFileContent(content, to_string(status) + ".html", status);
  }
  return make_tuple(status, content, filename, method, cgiargs, isCGI);
}

string getSuffix(string filename)
{
  auto pos = filename.find_last_of('.');
  string Ret;
  if (pos != std::string::npos)
    Ret = filename.substr(pos);
  return Ret;
}

string getFileType(string filename)
{
  string Suffix = getSuffix(filename);
  static const unordered_map<string, string> MIME = {
      {".aac", "audio/aac"},
      {".abw", "application/x-abiword"},
      {".arc", "application/octet-stream"},
      {".avi", "video/x-msvideo"},
      {".azw", "application/vnd.amazon.ebook"},
      {".bin", "application/octet-stream"},
      {".bmp", "image/bmp"},
      {".bz", "application/x-bzip"},
      {".bz2", "application/x-bzip2"},
      {".csh", "application/x-csh"},
      {".css", "text/css"},
      {".csv", "text/csv"},
      {".doc", "application/msword"},
      {".docx", "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {".eot", "application/vnd.ms-fontobject"},
      {".epub", "application/epub+zip"},
      {".es", "application/ecmascript"},
      {".gif", "image/gif"},
      {".htm", "text/html"},
      {".html", "text/html"},
      {".ico", "image/x-icon"},
      {".ics", "text/calendar"},
      {".jar", "application/java-archive"},
      {".jpeg", "image/jpeg"},
      {".jpg", "image/jpeg"},
      {".js", "application/javascript"},
      {".json", "application/json"},
      {".md", "text/markdown"},
      {".mid", "audio/midi"},
      {".midi", "audio/x-midi"},
      {".mp4", "video/mp4"},
      {".mpeg", "video/mpeg"},
      {".mpkg", "application/vnd.apple.installer+xml"},
      {".odp", "application/vnd.oasis.opendocument.presentation"},
      {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
      {".odt", "application/vnd.oasis.opendocument.text"},
      {".oga", "audio/ogg"},
      {".ogv", "video/ogg"},
      {".ogx", "application/ogg"},
      {".otf", "font/otf"},
      {".png", "image/png"},
      {".pdf", "application/pdf"},
      {".ppt", "application/vnd.ms-powerpoint"},
      {".pptx",
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"},
      {".rar", "application/x-rar-compressed"},
      {".rtf", "application/rtf"},
      {".sh", "application/x-sh"},
      {".svg", "image/svg+xml"},
      {".swf", "application/x-shockwave-flash"},
      {".tar", "application/x-tar"},
      {".tif", "image/tiff"},
      {".tiff", "image/tiff"},
      {".ts", "application/typescript"},
      {".ttf", "font/ttf"},
      {".txt", "text/plain"},
      {".vsd", "application/vnd.visio"},
      {".wav", "audio/wav"},
      {".weba", "audio/webm"},
      {".webm", "video/webm"},
      {".webp", "image/webp"},
      {".woff", "font/woff"},
      {".woff2", "font/woff2"},
      {".xhtml", "application/xhtml+xml"},
      {".xls", "application/vnd.ms-excel"},
      {".xlsx",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {".xml", "application/xml"},
      {".xul", "application/vnd.mozilla.xul+xml"},
      {".yml", "application/x-yaml"},
      {".zip", "application/zip"},
      {".7z", "application/x-7z-compressed"}};
  if (MIME.find(Suffix) != MIME.cend())
    return MIME.at(Suffix);
  return "application/octet-stream";
}

pair<string, vector<unsigned char>> produceAck(string request, string &method,
                                               const char *ADDR)
{
  // [Status, Content, fileType, method, cgiargs]
  unordered_map<string, string> fields;
  auto data = produceContent(request, ADDR, fields);
  vector<unsigned char> Data;
  string MessageStr;
  string fileType = getFileType(get<2>(data));
  string Content = get<1>(data);
  int status = get<0>(data);
  method = get<3>(data);
  string cgiargs = get<4>(data);
  bool isCGI = get<5>(data);
  switch (status)
  {
  case 200:
    MessageStr = "HTTP/1.1 200 OK\r\n";
    break;
  case 400:
    MessageStr = "HTTP/1.1 400 Bad Request\r\n";
    fileType = "text/html";
    break;
  case 403:
    MessageStr = "HTTP/1.1 403 Forbidden\r\n";
    fileType = "text/html";
    break;
  case 404:
    MessageStr = "HTTP/1.1 404 Not Found\r\n";
    fileType = "text/html";
    break;
  case 500:
    MessageStr = "HTTP/1.1 500 Internal Server Error\r\n";
    fileType = "text/html";
    break;
  case 501:
    MessageStr = "HTTP/1.1 501 Not Implemented\r\n";
    fileType = "text/html";
    break;
  default:
    printf("Bad status!\n");
    exit(-1);
    break;
  }
  bool isText = checkSubstring(fileType, "text");
  bool EnableCompress =
      checkSubstring(fields["Accept-Encoding"], "gzip") && isText && !isCGI;
  if (EnableCompress)
  {
    Data = compressContent<string>(Content);
    Content.clear();
  }
  else
  {
    for (auto I : Content)
      Data.push_back(I);
  }
  string charset;
  if (fileType == "text/html")
    charset = "; charset=utf-8";
  MessageStr += "Server: Nut-Server\r\n";
  MessageStr += "Connection: keep-alive\r\n";
  MessageStr +=
      "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";
  if (!isCGI)
  {
    MessageStr += "Content-length: " + std::to_string(Data.size()) + "\r\n";
    if (EnableCompress)
      MessageStr += "Content-Encoding: gzip\r\n";
    MessageStr += "Vary: Accept-Encoding, User-Agent\r\n";
    MessageStr += "Cache-Control: max-age=57600, immutable\r\n";
    MessageStr += "Content-type: " + fileType + charset + "\r\n";
    MessageStr += "\r\n";
  }
  return make_pair(MessageStr, Data);
}

void handleClientRequest(const int Client, const char *ADDR, SSL_CTX *ctx,
                         bool isHTTPS)
{
  SSL *ssl = isHTTPS ? SSL_new(ctx) : nullptr;
  if (ssl)
  {
    SSL_set_fd(ssl, Client);
    if (SSL_accept(ssl) <= 0)
      ERR_print_errors_fp(stderr);
  }
  while (true)
  {
    auto ClientRequest = receiveContent(Client, ssl);
    std::string MessageStr;
    vector<unsigned char> Data;
    string method;
    string Request =
        ClientRequest.has_value() ? transfertoString(*ClientRequest) : string("");
    if(Request=="")
      break;
    if (isHTTPS)
    {
      pair<string, vector<unsigned char>> Result;
      Result = produceAck(Request, method, ADDR);
      MessageStr = Result.first;
      Data = Result.second;
    }
    else
    {
      string method_tmp, path, input; // Here we shadow method
      unordered_map<string, string> fields;
      parseRequest(Request, method_tmp, path, input, fields);
      MessageStr = "HTTP/1.1 301 Moved Permanently\r\n";
      MessageStr += "Connection: keep-alive\r\n";
      MessageStr += "Location: https://" + fields["Host"] + path + "\r\n";
      MessageStr += "\r\n";
    }
    if (ssl)
    {
      SSL_write(ssl, MessageStr.data(), MessageStr.size());
      if (method != "HEAD")
        SSL_write(ssl, Data.data(), Data.size());
    }
    else
    {
      send(Client, MessageStr.data(), MessageStr.size(), 0);
      if (method != "HEAD")
        send(Client, Data.data(), Data.size(), 0);
    }
  }
  if(ssl)
    SSL_free(ssl);
  int ClientCloseRes = close(Client);
  if (ClientCloseRes == -1)
  {
    cerr << "Client Close\n";
    exit(-1);
  }
}

void handleSocket(int Server, SSL_CTX *ctx, bool isHTTPS)
{
  while (true)
  {
    // Client handle
    int Client;
    struct sockaddr_in6 ClientAddr;
    socklen_t ClientAddrSize = sizeof(ClientAddr);
    Client = accept(Server, (struct sockaddr *)&ClientAddr, &ClientAddrSize);
    if (Client == -1)
    {
      if (errno != EAGAIN)
      {
        puts("accept err");
        cerr << "Error number: " << errno << endl;
      }
      continue;
    }
    char ADDRBuffer[INET6_ADDRSTRLEN];
    const char *ADDR = inet_ntop(AF_INET6, &ClientAddr.sin6_addr, ADDRBuffer,
                                 sizeof(ADDRBuffer));
    if (ADDR)
      printf("Connection Request: %s:%d\n", ADDR, ntohs(ClientAddr.sin6_port));
    else
      puts("inet_ntop() err.");
    std::thread(handleClientRequest, Client, ADDR, ctx, isHTTPS).detach();
  }
}

/* we have this global to let the callback get easy access to it */
static pthread_mutex_t *lockarray;

static void init_locks(void)
{
  int i;

  lockarray = (pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                                sizeof(pthread_mutex_t));
  for (i = 0; i < CRYPTO_num_locks(); i++)
  {
    pthread_mutex_init(&(lockarray[i]), NULL);
  }

  CRYPTO_set_id_callback((unsigned long (*)())thread_id);
  CRYPTO_set_locking_callback(
      (void (*)(int, int, const char *, int))lock_callback);
}

static void kill_locks(void)
{
  int i;

  CRYPTO_set_locking_callback(NULL);
  for (i = 0; i < CRYPTO_num_locks(); i++)
    pthread_mutex_destroy(&(lockarray[i]));

  OPENSSL_free(lockarray);
}

int main()
{
  SSL_CTX *ctx;

  init_locks();
  init_openssl();
  ctx = create_context();

  configure_context(ctx);
  signal(SIGPIPE, SIG_IGN);
  std::vector<std::thread> threads;
  int HTTPSServer = createSocket(443);
  int HTTPServer = createSocket(80);
  threads.emplace_back(thread(handleSocket, HTTPSServer, ctx, true));
  threads.emplace_back(thread(handleSocket, HTTPServer, ctx, false));
  for (auto &t : threads)
    t.join();
  closeSocket(HTTPSServer);
  closeSocket(HTTPServer);
  SSL_CTX_free(ctx);
  cleanup_openssl();
  kill_locks();
}
