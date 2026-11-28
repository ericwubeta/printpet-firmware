#pragma once
#define MBEDTLS_SSL_MAX_FRAG_LEN_4096 4
typedef struct mbedtls_ssl_config mbedtls_ssl_config;
int mbedtls_ssl_conf_max_frag_len(mbedtls_ssl_config *configuration, unsigned char length);
