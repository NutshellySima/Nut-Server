#pragma once
#include "stdafx.h"

void init_openssl();

void cleanup_openssl();

SSL_CTX *create_context();

void configure_context(SSL_CTX *ctx);