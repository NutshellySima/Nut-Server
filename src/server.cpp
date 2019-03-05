#include "common.h"
#include "compress.h"
#include "stdafx.h"
#include "tls.h"
#include "server.h"

using namespace std;
namespace fs = std::filesystem;

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
    auto Request = receiveContent(Client, ssl);
    std::string MessageStr;
    string Data;
    string method;
    if(Request=="")
      break;
    if (isHTTPS)
    {
      pair<string, string> Result;
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
