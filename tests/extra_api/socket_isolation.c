/*
 * Copyright © 2019-2024 NVIDIA CORPORATION & AFFILIATES. ALL RIGHTS RESERVED.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* gcc -pthread tests/extra_api/socket_isolation.c -o socket_isolation_test */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <mellanox/xlio_extra.h>

#define THREADS_NR 5
#define FAKE_PORT 65535

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif /* ARRAY_SIZE */

#define HELLO_MSG "Hello"

static struct xlio_api_t *xlio_api = NULL;
static char *client_ip;
static char *server_ip;
static unsigned short base_port = 8080;

static void server()
{
    char buf[64];
    struct sockaddr_in addr;
    ssize_t len;
    int sock;
    int sock2;
    int sock3;
    int sock_in;
    int sock_in2;
    int val = SO_XLIO_ISOLATE_SAFE;
    int rc;

    /*
     * Socket create
     */

    sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    sock2 = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock2 >= 0);
    sock3 = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock3 >= 0);

    rc = setsockopt(sock, SOL_SOCKET, SO_XLIO_ISOLATE, &val, sizeof(val));
    assert(rc == 0);

    /*
     * Socket bind
     */

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server_ip);
    addr.sin_port = htons(base_port);

    rc = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    assert(rc == 0);

    addr.sin_port = htons(base_port + 1);
    rc = bind(sock2, (struct sockaddr *)&addr, sizeof(addr));
    assert(rc == 0);

    addr.sin_port = htons(base_port + 2);
    rc = bind(sock3, (struct sockaddr *)&addr, sizeof(addr));
    assert(rc == 0);

    rc = setsockopt(sock2, SOL_SOCKET, SO_XLIO_ISOLATE, &val, sizeof(val));
    assert(rc == 0);

    /*
     * Socket listen
     */

    rc = listen(sock, 5);
    assert(rc == 0);

    rc = listen(sock2, 5);
    assert(rc == 0);

    rc = listen(sock3, 5);
    assert(rc == 0);

    rc = setsockopt(sock3, SOL_SOCKET, SO_XLIO_ISOLATE, &val, sizeof(val));
    assert(rc == -1);
    assert(errno == EINVAL);

    /*
     * Check rings
     */

    int xlio_ring_fds[3];
    int xlio_ring_fds2[3];
    int xlio_ring_fds3[3];
    rc = xlio_api->get_socket_rings_fds(sock, xlio_ring_fds, ARRAY_SIZE(xlio_ring_fds));
    assert(rc == 1);
    rc = xlio_api->get_socket_rings_fds(sock2, xlio_ring_fds2, ARRAY_SIZE(xlio_ring_fds2));
    assert(rc == 1);
    rc = xlio_api->get_socket_rings_fds(sock3, xlio_ring_fds3, ARRAY_SIZE(xlio_ring_fds3));
    assert(rc == 1);
    assert(xlio_ring_fds[0] == xlio_ring_fds2[0]);
    assert(xlio_ring_fds[0] != xlio_ring_fds3[0]);

    /*
     * Socket accept
     */

    do {
        sock_in = accept(sock, NULL, NULL);
    } while (sock_in == -1 && errno == EINTR);
    assert(sock_in >= 0);

    do {
        sock_in2 = accept(sock2, NULL, NULL);
    } while (sock_in2 == -1 && errno == EINTR);
    assert(sock_in2 >= 0);

    /*
     * Socket read / write
     */

    len = write(sock_in, HELLO_MSG, sizeof(HELLO_MSG));
    assert(len > 0);

    do {
        len = read(sock_in, buf, sizeof(buf));
    } while (len == -1 && errno == EINTR);
    assert(len > 0);
    assert(len == sizeof(HELLO_MSG));
    assert(strncmp(buf, HELLO_MSG, strlen(HELLO_MSG)) == 0);

    /*
     * Socket close
     */

    sleep(1);

    rc = close(sock_in);
    assert(rc == 0);
    rc = close(sock_in2);
    assert(rc == 0);
    rc = close(sock);
    assert(rc == 0);
    rc = close(sock2);
    assert(rc == 0);
    rc = close(sock3);
    assert(rc == 0);
}

static void client()
{
    char buf[64];
    struct sockaddr_in addr;
    ssize_t len;
    int sock;
    int sock2;
    int val = SO_XLIO_ISOLATE_SAFE;
    int valdef = SO_XLIO_ISOLATE_DEFAULT;
    int rc;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    sock2 = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock2 >= 0);

    rc = setsockopt(sock, SOL_SOCKET, SO_XLIO_ISOLATE, &val, sizeof(val));
    assert(rc == 0);
    rc = setsockopt(sock, SOL_SOCKET, SO_XLIO_ISOLATE, &valdef, sizeof(valdef));
    assert(rc == -1);
    assert(errno == EINVAL);

    if (client_ip) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(client_ip);
        rc = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
        assert(rc == 0);
        rc = bind(sock2, (struct sockaddr *)&addr, sizeof(addr));
        assert(rc == 0);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server_ip);
    addr.sin_port = htons(base_port);

    rc = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    assert(rc == 0);

    addr.sin_port = htons(base_port + 1);
    rc = connect(sock2, (struct sockaddr *)&addr, sizeof(addr));
    assert(rc == 0);

    rc = setsockopt(sock2, SOL_SOCKET, SO_XLIO_ISOLATE, &val, sizeof(val));
    assert(rc == -1);
    assert(errno == EINVAL);

    int xlio_ring_fds[3];
    int xlio_ring_fds2[3];
    rc = xlio_api->get_socket_rings_fds(sock, xlio_ring_fds, ARRAY_SIZE(xlio_ring_fds));
    assert(rc == 1);
    rc = xlio_api->get_socket_rings_fds(sock2, xlio_ring_fds2, ARRAY_SIZE(xlio_ring_fds2));
    assert(rc == 1);
    assert(xlio_ring_fds[0] != xlio_ring_fds2[0]);

    len = write(sock, HELLO_MSG, sizeof(HELLO_MSG));
    assert(len > 0);

    do {
        len = read(sock, buf, sizeof(buf));
    } while (len == -1 && errno == EINTR);
    assert(len > 0);
    assert(len == sizeof(HELLO_MSG));
    assert(strncmp(buf, HELLO_MSG, strlen(HELLO_MSG)) == 0);

    sleep(1);

    rc = close(sock);
    assert(rc == 0);
    rc = close(sock2);
    assert(rc == 0);
}

static int mt_ring_fds[THREADS_NR];

static void *thread_func(void *arg)
{
    uint64_t id = (uint64_t)arg;
    struct sockaddr_in addr;
    int rc;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int val = SO_XLIO_ISOLATE_SAFE;
    rc = setsockopt(sock, SOL_SOCKET, SO_XLIO_ISOLATE, &val, sizeof(val));
    assert(rc == 0);

    val = O_NONBLOCK;
    rc = fcntl(sock, F_SETFL, val);
    assert(rc == 0);

    if (client_ip) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(client_ip);
        rc = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
        assert(rc == 0);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server_ip);
    addr.sin_port = htons(FAKE_PORT);

    /* connect() is non blocking and then expected to fail with ECONNREFUSED or ETIMEDOUT. */
    (void)connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    int xlio_ring_fds[3];
    rc = xlio_api->get_socket_rings_fds(sock, xlio_ring_fds, ARRAY_SIZE(xlio_ring_fds));
    assert(rc == 1);
    mt_ring_fds[id] = xlio_ring_fds[0];

    close(sock);

    return NULL;
}

static void client_mt()
{
    pthread_t tid[THREADS_NR] = {};
    int rc;

    for (uint64_t i = 0; i < THREADS_NR; ++i) {
        rc = pthread_create(&tid[i], NULL, &thread_func, (void *)i);
        assert(rc == 0);
    }

    /* There is no POSIX interface to decrement by >1, so run a loop. */
    for (int i = 0; i < THREADS_NR; ++i) {
        rc = pthread_join(tid[i], NULL);
        assert(rc == 0);
    }
    for (int i = 1; i < THREADS_NR; ++i) {
        assert(mt_ring_fds[i] == mt_ring_fds[0]);
    }
}

static void usage(const char *name)
{
    printf("Usage: %s <-s|-c> [client-ip,]<server-ip>\n", name);
    printf(" -s         Server mode\n");
    printf(" -c         Client mode\n");
    printf(" server-ip  IPv4 address to listen/connect to\n");
    printf(" client-ip  Optional IPv4 address to bind on client side\n");
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        usage(argc > 0 ? argv[0] : "a.out");
    }

    xlio_api = xlio_get_api();
    if (xlio_api == NULL) {
        printf("Extra API not found. Run under XLIO.\n");
        return 1;
    }

    char *ptr = strchr(argv[2], ',');
    if (ptr) {
        server_ip = ptr + 1;
        client_ip = argv[2];
        *ptr = '\0';
    } else {
        server_ip = argv[2];
        client_ip = NULL;
    }

    if (strcmp(argv[1], "-s") == 0) {
        server();
    } else if (strcmp(argv[1], "-c") == 0) {
        client_mt();
        client();
    } else {
        usage(argv[0]);
    }

    printf("Success.\n");
    return 0;
}
