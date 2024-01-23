/* gcc -I./install/include example.c -o example -libverbs */

#include <mellanox/xlio_extra.h>

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <infiniband/verbs.h>

bool tx_comp_done = false;

void send_comp_cb(uintptr_t sockdata, uintptr_t userdata)
{
    tx_comp_done = true;
    printf("Tx completion: sockdata=%lx userdata=%lx\n", sockdata, userdata);
}

int main(int argc, char **argv)
{
    int rc;

    if (argc != 2) {
        printf("Usage: %s <IP>\n", argv[0]);
        return 1;
    }

    struct xlio_api_t *xlio_api = xlio_get_api();
    if (!xlio_api) {
        printf("Couldn't find XLIO API\n");
        return 1;
    }

    struct xlio_extra_attr extra_attr = { .send_comp_cb = &send_comp_cb, };
    rc = xlio_api->xlio_extra_init(&extra_attr);
    assert(rc == 0);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    rc = inet_aton(argv[1], &addr.sin_addr);
    assert(rc != 0);

    rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    assert(rc == 0);

    int flag = fcntl(fd, F_GETFL);
    assert(flag >= 0);
    flag |= O_NONBLOCK;
    rc = fcntl(fd, F_SETFL, flag);
    assert(rc == 0);

    xlio_socket_t sock = xlio_api->xlio_fd_socket(fd);
    assert(sock != NULL);
    xlio_api->xlio_socket_userdata(sock, 0xdeadc0de);

    struct xlio_pd_attr pd_attr = {};
    socklen_t pd_attr_in_out_len = sizeof(pd_attr);
    rc = getsockopt(fd, SOL_SOCKET, SO_XLIO_PD, &pd_attr, &pd_attr_in_out_len);
    assert(rc == 0);
    assert(sizeof(pd_attr) == pd_attr_in_out_len);
    assert(pd_attr.ib_pd != NULL);

    static char header[] = "hello world\n";
    static char payload[4096];
    /* Memory registration in the XLIO protection domain. */
    struct ibv_pd *pd = (struct ibv_pd *)pd_attr.ib_pd;
    struct ibv_mr *mr_header = ibv_reg_mr(pd, header, sizeof(header),
                                          IBV_ACCESS_LOCAL_WRITE);
    assert(mr_header != NULL);
    struct ibv_mr *mr_payload = ibv_reg_mr(pd, payload, sizeof(payload),
                                           IBV_ACCESS_LOCAL_WRITE);
    assert(mr_payload != NULL);
    uint32_t mkey_header = mr_header->lkey;
    uint32_t mkey_payload = mr_payload->lkey;

    memset(payload, 'a', sizeof(payload));

    struct xlio_io_attr io_attr =  {
        .flags = XLIO_IO_FLAG_MSG_MORE,
        .mkey = mkey_header,
        .key = 0,
        .userdata = 0,
    };
    rc = xlio_api->xlio_io_send(sock, header, sizeof(header), &io_attr);
    assert(rc >= 0);

    io_attr.flags = 0;
    io_attr.mkey = mkey_payload;
    io_attr.userdata = 0xdeadbeef;
    rc = xlio_api->xlio_io_send(sock, payload, 32, &io_attr);
    assert(rc >= 0);

    xlio_api->xlio_io_flush(sock);

    int ringfd;
    rc = xlio_api->get_socket_rings_fds(fd, &ringfd, 1);
    assert(rc == 1);

    struct xlio_socketxtreme_completion_t comps;
    while (!tx_comp_done) {
        (void)xlio_api->socketxtreme_poll(ringfd, &comps, 1, SOCKETXTREME_POLL_TX);
    }

#if 0
    while (!tx_comp_done) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN | POLLOUT, };
        (void)poll(&pfd, 1, -1);
    }
#endif

    close(fd);

    ibv_dereg_mr(mr_header);
    ibv_dereg_mr(mr_payload);

    return 0;
}
