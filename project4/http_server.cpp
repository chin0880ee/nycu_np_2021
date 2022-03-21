#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { do_read(); }

private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                sscanf(data_, "%s %s %s %*s %s", REQUEST_METHOD, REQUEST_URI,
                        SERVER_PROTOCOL, HTTP_HOST);

                if (!ec) {
                    do_write(length);
                }
            }
        );
    }

    void do_write(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_, boost::asio::buffer(status_str, strlen(status_str)),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    strcpy(SERVER_ADDR,
                            socket_.local_endpoint().address().to_string().c_str());
                    sprintf(SERVER_PORT, "%u", socket_.local_endpoint().port());
                    strcpy(REMOTE_ADDR,
                            socket_.remote_endpoint().address().to_string().c_str());
                    sprintf(REMOTE_PORT, "%u", socket_.remote_endpoint().port());

                    char filetmp[100];
                    sscanf(REQUEST_URI, "%[^?]?%s", filetmp, QUERY_STRING);
                    strcat(EXEC_FILE, filetmp);

                    // if (strlen(REQUEST_URI) > strlen(filetmp)+1) sscanf(REQUEST_URI, "%*[^?]?%s", QUERY_STRING);
                    // else QUERY_STRING[0] = '\0';

                    setenv("REQUEST_METHOD", REQUEST_METHOD, 1);
                    setenv("REQUEST_URI", REQUEST_URI, 1);
                    setenv("QUERY_STRING", QUERY_STRING, 1);
                    setenv("SERVER_PROTOCOL", SERVER_PROTOCOL, 1);
                    setenv("HTTP_HOST", HTTP_HOST, 1);
                    setenv("SERVER_ADDR", SERVER_ADDR, 1);
                    setenv("SERVER_PORT", SERVER_PORT, 1);
                    setenv("REMOTE_ADDR", REMOTE_ADDR, 1);
                    setenv("REMOTE_PORT", REMOTE_PORT, 1);
                    setenv("EXEC_FILE", EXEC_FILE, 1);
                    // file path to testpath

                    pid_t kidpid;
                    kidpid = fork();
                    while ( kidpid < 0 ) {
                        kidpid = fork();
                    }
                    if ( kidpid < 0 ) {
                        perror( "Internal error: cannot fork." );
                    }
                    else if ( kidpid == 0 ) {
                        // I am the child.
                        int sock = socket_.native_handle();
                        dup2(sock, STDIN_FILENO);
                        dup2(sock, STDOUT_FILENO);
                        dup2(sock, STDERR_FILENO);
                        socket_.close();

                        if (execlp(EXEC_FILE, EXEC_FILE, NULL) < 0) {
                            std::cout << "Content-type:text/html\r\n\r\n<h1>FAIL</h1>";
                        }
                    }
                    else {
                        // I am the parent.
                        socket_.close();
                    }

                    do_read();
                }
            }
        );
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
    char status_str[200] = "HTTP/1.1 200 OK\n";
    char REQUEST_METHOD[20];
    char REQUEST_URI[1000];
    char QUERY_STRING[1000];
    char SERVER_PROTOCOL[100];
    char HTTP_HOST[100];
    char SERVER_ADDR[100];
    char SERVER_PORT[20];
    char REMOTE_ADDR[100];
    char REMOTE_PORT[20];
    char EXEC_FILE[100] = ".";
};

class Server {
public:
    Server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }

                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        Server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }

    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}