# λ of XYZ

A HTTPS server with an example site.

## Nut-Server

## Feature

## Security
+ HTTPS powered by OpenSSL.
+ Prevent access to folder other than "pages".
+ Redirect HTTP traffic to HTTPS.

## Functionality
+ Support basic HTTP GET/POST/HEAD method.
+ Support to transform "%20" to space.
+ Support several MIME types. (including js/css/html/fonts...)
+ Be able to report server status.
+ CGI support. (including GET/POST, Cookie support; It does not support changing HTTP status code via STATUS field).
+ Can encode text page to gzip using zlib.
+ master: Multithreaded; epollET: epoll(edge-triggered)+non-bocking socket; epollLT: epoll(level-triggered)+non-bocking socket
+ Support HTTP keep-alive semantic and HTTP pipelining
## Prerequisites
+ OpenSSL
+ Zlib
+ A C++ 17 compatible compiler/standard library

## Build & Run
```
$ chmod +x ./make.sh
$ ./make.sh
# Add cert.pem and key.pem under ./certificate
$ ./server.out
```

## Caution
1. You should make sure the CGI is **secured** or you should drop root privileges manually by modifying the server software or inside the CGI.
2. This server does not support uploading files because lack of parsing of `multipart/form-data`.
3. This server does not support long header+content requests in order to prevent malicious requests.

## Example site

Note: This example site is purely static on client side (i.e. without js) and its language is Chinese (Simplified).

## License

All contents under this repo is viewed as code.

Copyright (c) 2018-2019 Chijun Sima

Licensed under the MIT License.