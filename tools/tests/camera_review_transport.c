#include "camera_review_common.h"
#include "app_camera.h"
#include "app_camera_config.h"
#include "camera_geometry.h"
#define JSON_LIMIT 1024
typedef void esp_tls_t;
typedef struct { int unused; } mbedtls_md5_context;
/* @READER@ */
static camera_reader_t *s_verifying;
static unsigned connects, closes, setups, requests, decoded;
static bool setup_failure, request_failure, decode_failure;
#define ESP_TLS_ERR_SSL_WANT_READ -1
#define ESP_TLS_ERR_SSL_WANT_WRITE -2
#define MBEDTLS_ERR_SSL_TIMEOUT -3
#define MBEDTLS_ERR_SSL_ALLOC_FAILED -4
static camera_reader_t *io_reader;
static bool changed_identity;
static bool session_current = true, cancel_during_read;
static int read_limit;
static unsigned read_calls, next_byte;
static bool current(uint32_t token) { return session_current; }
static void vTaskDelay(int ticks) {}
static int esp_tls_conn_read(esp_tls_t *tls, void *buffer, int length) {
    read_calls++;
    io_reader->identity_failed = changed_identity;
    if (cancel_during_read) session_current = false;
    int count = read_limit && length > read_limit ? read_limit : length;
    for (int i = 0; i < count; i++) ((uint8_t *)buffer)[i] = (uint8_t)next_byte++;
    return count;
}
static int esp_tls_conn_write(esp_tls_t *tls, const void *buffer, int length) {
    io_reader->identity_failed=changed_identity; return length;
}
/* @READ@ */
/* @WRITE@ */
static int esp_tls_conn_destroy(esp_tls_t *tls) { closes++; return 0; }
/* @CLOSE@ */
static bool connect_camera(camera_reader_t *r) {
    assert(!r->tls && !r->identity_failed && !r->certificate_seen);
    connects++; r->tls=(void *)1; s_verifying=r;
    r->certificate_seen=true; return true;
}
static bool start_snapshot_session(camera_reader_t *r) { setups++; return !setup_failure; }
static bool request_snapshot(camera_reader_t *r) { requests++; return !request_failure; }
static bool begin_jpeg_stream(camera_reader_t *r) { return true; }
static bool decode(camera_reader_t *r) {
    decoded++; r->received=80000;
    snprintf(r->image_md5,sizeof(r->image_md5),"%032u",decoded);
    return !decode_failure;
}
/* @FETCH@ */
int main(int argc, char **argv) {
    assert(argc==2); camera_reader_t r={0};
    io_reader = &r;
    if (!strcmp(argv[1],"read-ahead")) {
        read_limit = 12;
        for (unsigned i = 0; i < 12; i++) {
            uint8_t value;
            assert(read_bytes_inner(&r, &value, 1) && value == i);
        }
        assert(read_calls == 1);
        uint8_t rest[25];
        assert(read_bytes_inner(&r, rest, sizeof(rest)));
        for (unsigned i = 0; i < sizeof(rest); i++) assert(rest[i] == i + 12);
        assert(read_calls == 4);
    } else if (!strcmp(argv[1],"read-ahead-close")) {
        read_limit = 8;
        r.tls = (void *)1;
        uint8_t value = 255;
        assert(read_bytes_inner(&r, &value, 1) && value == 0);
        close_camera(&r);
        r.tls = (void *)1;
        next_byte = 130;
        assert(read_bytes_inner(&r, &value, 1) && value == 130);
    } else if (!strcmp(argv[1],"read-ahead-cancel")) {
        read_limit = 8;
        cancel_during_read = true;
        uint8_t value = 255;
        assert(!read_bytes_inner(&r, &value, 1) && value == 255);
    } else if (!strcmp(argv[1],"identity-io")) {
        uint8_t buffer[3]={0};
        read_limit = sizeof(buffer);
        assert(read_bytes_inner(&r,buffer,3) && write_bytes(&r,buffer,3));
        changed_identity=true;
        assert(!read_bytes_inner(&r,buffer,3) && r.error==CAMERA_IDENTITY_ERROR);
        r.identity_failed=false;
        assert(!write_bytes(&r,buffer,3) && r.error==CAMERA_IDENTITY_ERROR);
    } else if (!strcmp(argv[1],"reuse")) {
        assert(fetch(&r,false)); assert(fetch(&r,false));
        assert(connects==1 && setups==1 && requests==2 && closes==0 && decoded==2);
        assert(r.tls && s_verifying==&r);
    } else if (!strcmp(argv[1],"failure")) {
        assert(fetch(&r,false)); request_failure=true;
        assert(!fetch(&r,false) && !r.tls && !s_verifying && closes==1);
        request_failure=false; assert(fetch(&r,false));
        assert(connects==2 && setups==2);
        decode_failure=true; assert(!fetch(&r,false) && !r.tls && closes==2);
        decode_failure=false; assert(fetch(&r,false));
        assert(connects==3 && setups==3);
    } else if (!strcmp(argv[1],"setup-failure")) {
        setup_failure=true;
        assert(!fetch(&r,false) && !r.tls && requests==0 && closes==1);
        setup_failure=false; assert(fetch(&r,false));
        assert(connects==2 && setups==2);
    } else {
        bool stream=!strcmp(argv[1],"stream");
        assert(fetch(&r,stream) && !r.tls); assert(fetch(&r,stream) && !r.tls);
        assert(connects==2 && closes==2);
    }
    close_camera(&r);
    unsigned count=closes; close_camera(&r); assert(closes==count && !s_verifying);
    return 0;
}
