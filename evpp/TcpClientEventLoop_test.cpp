/*
 * TcpClientEventLoop_test.cpp
 *
 * @build   make evpp
 * @server  bin/TcpServer_test 1234
 * @client  bin/TcpClientEventLoop_test 1234
 *
 */

#include "TcpClient.h"
#include "htime.h"

#define TEST_RECONNECT  1
#define TEST_TLS        0

using namespace hv;

class MyTcpClient : public TcpClientEventLoopTmpl<SocketChannel> {
public:
    MyTcpClient(EventLoopPtr loop = NULL) : TcpClientEventLoopTmpl<SocketChannel>(loop) {
        onConnection = [this](const SocketChannelPtr& channel) {
            std::string peeraddr = channel->peeraddr();
            if (channel->isConnected()) {
                printf("connected to %s! connfd=%d\n", peeraddr.c_str(), channel->fd());
                // send(time) every 3s
                setInterval(3000, [channel](TimerID timerID){
                    if (channel->isConnected()) {
                        if (channel->isWriteComplete()) {
                            char str[DATETIME_FMT_BUFLEN] = {0};
                            datetime_t dt = datetime_now();
                            datetime_fmt(&dt, str);
                            channel->write(str);
                        }
                    } else {
                        killTimer(timerID);
                    }
                });
            } else {
                printf("disconnected to %s! connfd=%d\n", peeraddr.c_str(), channel->fd());
            }
            if (isReconnect()) {
                printf("reconnect cnt=%d, delay=%d\n", reconn_setting->cur_retry_cnt, reconn_setting->cur_delay);
            }
        };

        onMessage = [](const SocketChannelPtr& channel, Buffer* buf) {
            printf("< %.*s\n", (int)buf->size(), (char*)buf->data());
        };
    }

    int connect(int port) {
        int connfd = createsocket(port);
        if (connfd < 0) {
            return connfd;
        }
#if TEST_RECONNECT
        // reconnect: 1,2,4,8,10,10,10...
        reconn_setting_t reconn;
        reconn_setting_init(&reconn);
        reconn.min_delay = 1000;
        reconn.max_delay = 10000;
        reconn.delay_policy = 2;
        setReconnect(&reconn);
#endif

#if TEST_TLS
        withTLS();
#endif
        printf("client connect to port %d, connfd=%d ...\n", port, connfd);
        return startConnect();
    }
};
typedef std::shared_ptr<MyTcpClient> MyTcpClientPtr;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s port\n", argv[0]);
        return -10;
    }
    int port = atoi(argv[1]);

    EventLoopPtr loop(new EventLoop);

    MyTcpClientPtr cli1(new MyTcpClient(loop));
    cli1->connect(port);

    MyTcpClientPtr cli2(new MyTcpClient(loop));
    cli2->connect(port);

    loop->run();

    return 0;
}
