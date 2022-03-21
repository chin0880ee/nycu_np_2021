#include <sys/wait.h>

#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

using boost::asio::ip::tcp;
boost::asio::io_context io_context;

void signalHandler(int signum) {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
    };
}

bool firewall(int CD, std::string IP) {
    std::string filename("./socks.conf");
    std::string line;
    std::ifstream input_file(filename);
    if (!input_file.is_open()) {
        std::cerr << "Could not open the file - '" << filename << "'"
                  << std::endl;
        return false;
    }

    char my_ip[4][4];
    sscanf(IP.c_str(), "%[^.].%[^.].%[^.].%s", my_ip[0], my_ip[1], my_ip[2], my_ip[3]);

    while (getline(input_file, line)) {
        char mode[2];
        char permit_ip[4][4];
        sscanf(line.c_str(), "%*s %s %[^.].%[^.].%[^.].%s", mode, permit_ip[0], permit_ip[1], permit_ip[2], permit_ip[3]);
        if ((CD == 1 && strcmp(mode, "c") == 0) || (CD == 2 && strcmp(mode, "b") == 0)) {
            for (int p = 0; p < 4; p++) {
                if (strcmp(permit_ip[p], "*") == 0) {
                    return true;
                }
                else if (strcmp(permit_ip[p], my_ip[p]) == 0 && p == 3) {
                    return true;
                }
                else if (strcmp(permit_ip[p], my_ip[p]) != 0) {
                    break;
                }
            }
        }
    }
    return false;
}

class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(tcp::socket client_socket, tcp::socket server_socket)
        : client_socket_(std::move(client_socket)),
          server_socket_(std::move(server_socket)) {}

    void start() {
        client_do_read();
        server_do_read();
    }

  private:
    void client_do_read() {
        // std::cout << "client_do_read" << std::endl;
        auto self(shared_from_this());
        client_socket_.async_read_some(
            boost::asio::buffer(client_data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    server_do_write(length);
                }
                else {
                    // std::cout << "client_do_read error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                    client_socket_.close();
                    exit(0);
                }
            });
    }

    void client_do_write(std::size_t length) {
        // std::cout << "client_do_write" << std::endl;
        auto self(shared_from_this());
        boost::asio::async_write(
            client_socket_, boost::asio::buffer(server_data_, length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    server_do_read();
                }
                else {
                    // std::cout << "client_do_write error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                    server_socket_.close();
                }
            });
    }

    void server_do_read() {
        // std::cout << "server_do_read" << std::endl;
        auto self(shared_from_this());
        server_socket_.async_read_some(
            boost::asio::buffer(server_data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    client_do_write(length);
                }
                else {
                    // std::cout << "server_do_read error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                    server_socket_.close();
                    exit(0);
                }
            });
    }

    void server_do_write(std::size_t length) {
        // std::cout << "server_do_write" << std::endl;
        auto self(shared_from_this());
        boost::asio::async_write(
            server_socket_, boost::asio::buffer(client_data_, length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    client_do_read();
                }
                else {
                    // std::cout << "server_do_write error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                    client_socket_.close();
                }
            });
    }

    tcp::socket client_socket_;
    tcp::socket server_socket_;
    enum { max_length = 4096 };
    unsigned char client_data_[max_length];
    unsigned char server_data_[max_length];
};

class Bind : public std::enable_shared_from_this<Bind> {
  public:
    Bind(tcp::socket socket)
        : client_socket_(std::move(socket)),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), 0)) {}  //////////////////////////

    void start() {
        do_accept();
        do_reply();
    }

    void reject() { do_reject(); }

  private:
    void do_accept() {
        auto self(shared_from_this());
        port = acceptor_.local_endpoint().port();
        acceptor_.async_accept(
            [this, self](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    acceptor_.close();
                    do_reply();
                    std::make_shared<Session>(std::move(client_socket_), std::move(socket))->start();
                }
                else {
                    // std::cout << "do_accept error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    void do_reply() {
        // std::cout << "do_reply" << std::endl;
        auto self(shared_from_this());
        unsigned char p1 = port / 256;
        unsigned char p2 = port % 256;
        unsigned char reply[8] = {0, 90, p1, p2, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket_, boost::asio::buffer(reply, 8),
            [self](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    // std::cout << "do_reply error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    void do_reject() {
        // std::cout << "do_reply" << std::endl;
        auto self(shared_from_this());
        unsigned char reply[8] = {0, 90, 0, 0, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket_, boost::asio::buffer(reply, 8),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    acceptor_.close();
                    client_socket_.close();
                    exit(0);
                }
                else {
                    // std::cout << "do_reject error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    tcp::socket client_socket_;
    tcp::acceptor acceptor_;
    unsigned short port;
};

class Connect : public std::enable_shared_from_this<Connect> {
  public:
    Connect(tcp::socket socket, std::string domain, std::string DSTPORT)
        : client_socket_(std::move(socket)),
          server_socket_(io_context),
          resolver_(io_context),
          query{domain, DSTPORT} {
        HOST = domain;
        PORT = DSTPORT;
    }

    void start() { do_resolve(query); }

  private:
    void do_resolve(tcp::resolver::query query) {
        auto self(shared_from_this());
        resolver_.async_resolve(
            query,
            [this, self](const boost::system::error_code& ec, tcp::resolver::iterator it) {
                if (!ec) {
                    std::cout << "<S_IP>: " << client_socket_.remote_endpoint().address().to_string() << std::endl;
                    std::cout << "<S_PORT>: " << client_socket_.remote_endpoint().port() << std::endl;
                    std::cout << "<D_IP>: " << it->endpoint().address().to_string() << std::endl;
                    std::cout << "<D_PORT>: " << PORT << std::endl;
                    std::cout << "<Command>: "
                              << "CONNECT" << std::endl;
                    if (firewall(1, it->endpoint().address().to_string())) {
                        std::cout << "<Reply>: "
                                  << "Accept" << std::endl
                                  << std::endl;
                        do_connect(it);
                    }
                    else {
                        std::cout << "<Reply>: "
                                  << "Reject" << std::endl
                                  << std::endl;
                        do_reject();
                    }
                }
            });
    }

    void do_connect(tcp::resolver::iterator it) {
        // std::cout << "do_connect"  << std::endl;
        auto self(shared_from_this());
        server_socket_.async_connect(
            *it,
            [this, self](const boost::system::error_code& ec) {
                if (!ec) {
                    do_reply();
                    std::make_shared<Session>(std::move(client_socket_), std::move(server_socket_))->start();
                }
                else {
                    // std::cout << "do_connect error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    void do_reply() {
        // std::cout << "do_reply" << std::endl;
        auto self(shared_from_this());
        unsigned char reply[8] = {0, 90, 0, 0, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket_, boost::asio::buffer(reply, 8),
            [self](boost::system::error_code ec, std::size_t /*length*/) {
                if (ec) {
                    // std::cout << "do_reply error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    void do_reject() {
        // std::cout << "do_reject" << std::endl;
        auto self(shared_from_this());
        unsigned char reply[8] = {0, 91, 0, 0, 0, 0, 0, 0};
        boost::asio::async_write(
            client_socket_, boost::asio::buffer(reply, 8),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    client_socket_.close();
                    exit(0);
                }
                else {
                    // std::cout << "do_reject error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    tcp::socket client_socket_;
    tcp::socket server_socket_;
    tcp::resolver resolver_;
    tcp::resolver::query query;
    std::string HOST;
    std::string PORT;
};

class Router : public std::enable_shared_from_this<Router> {
  public:
    Router(tcp::socket socket)
        : client_socket_(std::move(socket)) {}

    void start() { do_router(); }

  private:
    std::string get_domain(size_t length) {
        int USERID = 1;
        for (int i = 8; i < length - 1; i++) {
            if (USERID) {
                if (client_data_[i] != 0x00) {
                    continue;
                }
                else {
                    USERID = 0;
                }
            }
            else {
                domain += client_data_[i];
            }
        }
        return domain;
    }

    std::string get_DSTIP() {
        for (int i = 4; i < 8; i++) {
            DSTIP += std::to_string(client_data_[i]);
            if (i != 7) {
                DSTIP += ".";
            }
        }
        return DSTIP;
    }

    void do_router() {
        auto self(shared_from_this());
        client_socket_.async_read_some(
            boost::asio::buffer(client_data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    VN = std::to_string(client_data_[0]);
                    CD = std::to_string(client_data_[1]);
                    DSTPORT = std::to_string((int)(client_data_[2] << 8) + (int)client_data_[3]);
                    DSTIP = get_DSTIP();
                    domain = get_domain(length);
                    // std::cout << VN << std::endl;
                    // std::cout << CD << std::endl;
                    // std::cout << DSTPORT << std::endl;
                    // std::cout << DSTIP << std::endl;
                    // std::cout << domain << std::endl;
                    if (CD == "1") {
                        // connect mode
                        if (domain == "") domain = DSTIP;
                        std::make_shared<Connect>(std::move(client_socket_), domain, DSTPORT)->start();
                    }
                    else if (CD == "2") {
                        // bind mode
                        std::cout << "<S_IP>: " << client_socket_.remote_endpoint().address().to_string() << std::endl;
                        std::cout << "<S_PORT>: " << client_socket_.remote_endpoint().port() << std::endl;
                        std::cout << "<D_IP>: " << DSTIP << std::endl;
                        std::cout << "<D_PORT>: " << DSTPORT << std::endl;
                        std::cout << "<Command>: "
                                  << "BIND" << std::endl;
                        if (firewall(2, DSTIP)) {
                            std::cout << "<Reply>: "
                                      << "Accept" << std::endl
                                      << std::endl;
                            std::make_shared<Bind>(std::move(client_socket_))->start();  /////////////////////////////////
                        }
                        else {
                            std::cout << "<Reply>: "
                                      << "Reject" << std::endl
                                      << std::endl;
                            std::make_shared<Bind>(std::move(client_socket_))->reject();  ////////////////////////////////
                        }
                    }
                }
            });
    }

    tcp::socket client_socket_;
    enum { max_length = 4096 };
    unsigned char client_data_[max_length];
    std::string VN;
    std::string CD;
    std::string DSTPORT;
    std::string DSTIP;
    std::string domain;
};

class Server {
  public:
    Server(short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

  private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    io_context.notify_fork(boost::asio::io_context::fork_prepare);
                    pid_t kidpid;
                    kidpid = fork();
                    while (kidpid < 0) {
                        kidpid = fork();
                    }
                    if (kidpid < 0) {
                        perror("Internal error: cannot fork.");
                    }
                    else if (kidpid == 0) {
                        // I am the child.
                        io_context.notify_fork(boost::asio::io_context::fork_child);
                        acceptor_.close();
                        std::make_shared<Router>(std::move(socket))->start();
                    }
                    else {
                        // I am the parent.
                        io_context.notify_fork(boost::asio::io_context::fork_parent);
                        socket.close();
                        do_accept();
                    }
                }
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    signal(SIGCHLD, signalHandler);
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        Server s(std::atoi(argv[1]));

        io_context.run();
    }

    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}