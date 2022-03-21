#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

using boost::asio::ip::tcp;

void html(int snum, char npserver[5][50], char port[5][20]) {
    std::string text;
    text =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "  <head>\n"
        "    <meta charset=\"UTF-8\" />\n"
        "    <title>NP Project 3 Sample Console</title>\n"
        "    <link\n"
        "      rel=\"stylesheet\"\n"
        "      "
        "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/"
        "bootstrap.min.css\"\n"
        "      "
        "integrity=\"sha384-TX8t27EcRE3e/"
        "ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
        "      crossorigin=\"anonymous\"\n"
        "    />\n"
        "    <link\n"
        "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        "      rel=\"stylesheet\"\n"
        "    />\n"
        "    <link\n"
        "      rel=\"icon\"\n"
        "      type=\"image/png\"\n"
        "      "
        "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/"
        "678068-terminal-512.png\"\n"
        "    />\n"
        "    <style>\n"
        "      * {\n"
        "        font-family: 'Source Code Pro', monospace;\n"
        "        font-size: 1rem !important;\n"
        "      }\n"
        "      body {\n"
        "        background-color: #212529;\n"
        "      }\n"
        "      pre {\n"
        "        color: #cccccc;\n"
        "      }\n"
        "      b {\n"
        "        color: #01b468;\n"
        "      }\n"
        "    </style>\n"
        "  </head>\n"
        "  <body>\n"
        "    <table class=\"table table-dark table-bordered\">\n"
        "      <thead>\n"
        "        <tr>\n";

    for (int i = 0; i < snum; i++) {
        text = text + "          <th scope=\"col\">";
        text = text + npserver[i];
        text = text + ":";
        text = text + port[i];
        text = text + "</th>\n";
    }

    text = text +
           "        </tr>\n"
           "      </thead>\n"
           "      <tbody>\n"
           "        <tr>\n";

    for (int i = 0; i < snum; i++) {
        text = text + "          <td><pre id=\"s";
        text = text + std::to_string(i);
        text = text + "\" class=\"mb-0\"></pre></td>\n";
    }

    text = text +
           "        </tr>\n"
           "      </tbody>\n"
           "    </table>\n"
           "  </body>\n"
           "</html>\n";
    std::cout << text << std::flush;
}

void replaceAll(std::string& str, const std::string& from,
                const std::string& to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

void output_shell(int snum, std::string content) {
    replaceAll(content, "&", "&#x0026;");
    replaceAll(content, "\n", "&NewLine;");
    replaceAll(content, "\t", "&#009;");
    replaceAll(content, "\"", "&#x34;");
    replaceAll(content, "\'", "&#x22;");
    replaceAll(content, "<", "&#x003C;");
    replaceAll(content, ">", "&#x003E;");
    std::cout << "<script>document.getElementById('s" << snum
              << "').innerHTML += '" << content << "';</script>\n"
              << std::flush;
}
void output_command(int snum, std::string content) {
    replaceAll(content, "&", "&#x0026;");
    replaceAll(content, "\n", "&NewLine;");
    replaceAll(content, "\t", "&#009;");
    replaceAll(content, "\"", "&#x34;");
    replaceAll(content, "\'", "&#x22;");
    replaceAll(content, "<", "&#x003C;");
    replaceAll(content, ">", "&#x003E;");
    std::cout << "<script>document.getElementById('s" << snum
              << "').innerHTML += '<b>" << content << "</b>';</script>\n"
              << std::flush;
}

class Client : public std::enable_shared_from_this<Client> {
  public:
    Client(boost::asio::io_context& io_context, int cid, char npserver[50],
           char port[20], std::vector<std::string> liness, char sockserver[50], char sockport[20])
        : resolver_(io_context), socket_(io_context), q{sockserver, sockport} {
        lines = liness;
        my_id = cid;
        strcpy(HOST, npserver);
        PORT = atoi(port);
        // std::cout << PORT << std::endl;
    }

    void start() { do_resolve(q); }

  private:
    void do_resolve(tcp::resolver::query q) {
        auto self(shared_from_this());
        resolver_.async_resolve(
            q, [this, self](const boost::system::error_code& ec,
                            tcp::resolver::iterator it) {
                if (!ec) {
                    do_connect(it);
                }
            });
    }

    void do_connect(tcp::resolver::iterator it) {
        auto self(shared_from_this());
        socket_.async_connect(
            *it,
            [this, self](const boost::system::error_code& ec) {
                if (!ec) {
                    do_request();
                }
                else {
                    // std::cout << "do_connect error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    void do_request() {
        // std::cout << "do_reply" << std::endl;
        auto self(shared_from_this());
        char p1 = PORT / 256;
        char p2 = PORT % 256;

        char request[100] = {0};
        request[0] = 4;
        request[1] = 1;
        request[2] = p1;
        request[3] = p2;
        request[7] = 1;
        strcpy(request + 9, HOST);

        boost::asio::async_write(
            socket_, boost::asio::buffer(request, 10 + strlen(HOST)),
            [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
                if (!ec) {
                    do_reply();
                }
                else {
                    // std::cout << "do_request error" << std::endl;
                    // std::cout << ec.message() << std::endl;
                }
            });
    }

    void do_reply() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(reply_data_),
            [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (!ec) {
                    // std::cout << "do_reply" << std::endl;
                    // for(int i=8; i<bytes_transferred; i++) {
                    //   std::cout << reply_data_[i];
                    // }
                    do_read();
                }
                else {
                    // std::cout << "do_reply error" << std::endl;
                }
            });
    }

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(bytes),
            [this, self](boost::system::error_code ec,
                         std::size_t bytes_transferred) {
                if (!ec) {
                    std::string str(std::begin(bytes),
                                    bytes_transferred);
                    output_shell(my_id, str);

                    for (int j = 0; j < bytes_transferred; j++) {
                        if (bytes.data()[j] == '%') {
                            if (j == bytes_transferred - 1)
                                prompt = 2;
                            else
                                prompt = 1;
                            break;
                        }
                    }
                    if (prompt == 1) {
                        prompt = 0;
                        if (lines[line_count] == "exit") eof = 1;
                        r = lines[line_count] + "\n";
                        do_write();
                        output_command(my_id, r);
                        line_count++;
                    }
                    else if (line_count < lines.size()) {
                        if (prompt == 2) prompt = 1;
                        do_read();
                    }
                    else {
                        socket_.shutdown(tcp::socket::shutdown_send);
                        socket_.close();
                    }
                }
            });
    }

    void do_write() {
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_, boost::asio::buffer(r, r.length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    if (eof) {
                        socket_.shutdown(tcp::socket::shutdown_send);
                        socket_.close();
                    }
                    else {
                        do_read();
                    }
                }
            });
    }

    tcp::resolver resolver_;
    tcp::socket socket_;
    std::array<char, 4096> bytes;
    std::vector<std::string> lines;
    tcp::resolver::query q;
    std::string r;
    int my_id;
    int line_count = 0;
    int prompt = 0;
    int eof = 0;
    char HOST[50];
    int PORT;
    unsigned char reply_data_[8];
};

int main(int argc, char* argv[]) {
    char npserver[5][50];
    char port[5][20];
    char testfile[5][20];
    char sockserver[50];
    char sockport[20];
    std::vector<std::string> lines[5];

    // if (getenv("QUERY_STRING") == NULL) return 0;
    char* query = getenv("QUERY_STRING");
    // char query[15000] =
    //     "h0=127.0.0.1&p0=12094&f0=t1.txt&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&sh=127.0.0.1&sp=12095";
    char tmp[50];

    for (int k = 0; k < 5; k++) {
        sscanf(query, "%[^&]&%s", tmp, query);
        sscanf(tmp, "%*[^=]=%s", tmp);
        strcpy(npserver[k], tmp);

        sscanf(query, "%[^&]&%s", tmp, query);
        sscanf(tmp, "%*[^=]=%s", tmp);
        strcpy(port[k], tmp);

        sscanf(query, "%[^&]&%s", tmp, query);
        sscanf(tmp, "%*[^=]=%s", tmp);
        strcpy(testfile[k], tmp);
    }

    sscanf(query, "%[^&]&%s", tmp, query);
    sscanf(tmp, "%*[^=]=%s", tmp);
    strcpy(sockserver, tmp);

    sscanf(query, "%[^&]&%s", tmp, query);
    sscanf(tmp, "%*[^=]=%s", tmp);
    strcpy(sockport, tmp);

    int Snum = 5;
    for (int k = 0; k < 5; k++) {
        if (npserver[k][strlen(npserver[k]) - 1] == '=') {
            Snum = k;
            break;
        }
        // else {
        //     strcpy(npserver[k], "127.0.0.1");  //////////////////////////////
        //     strcpy(sockserver, "127.0.0.1");   //////////////////////////////
        // }
    }
    int isSock = 1;
    if (sockserver[strlen(sockserver) - 1] == '=') isSock = 0;

    for (int j = 0; j < Snum; j++) {
        char testName[40];
        strcpy(testName, "./test_case/");
        strcat(testName, testfile[j]);
        std::string filename(testName);
        std::string line;
        std::ifstream input_file(filename);
        if (!input_file.is_open()) {
            std::cerr << "Could not open the file - '" << filename << "'"
                      << std::endl;
            return 0;
        }

        while (getline(input_file, line)) {
            if (line[line.length() - 1] == '\r') {
                line[line.length() - 1] = '\n';
                line.pop_back();
            }
            lines[j].push_back(line);
        }
        input_file.close();
    }

    boost::asio::io_context io_context;

    for (int i = 0; i < Snum; i++) {
        std::make_shared<Client>(io_context, i, npserver[i], port[i], lines[i], sockserver, sockport)
            ->start();
    }
    // Client c0(io_context, 0, npserver[0], port[0], lines[0]);

    std::cout << "Content-type: text/html\n\r\n\r\n";
    html(Snum, npserver, port);

    io_context.run();
}