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
         char port[20], std::vector<std::string> liness)
      : resolver_(io_context), socket_(io_context), q{npserver, port} {
    lines = liness;
    my_id = cid;
  }

  void start() { do_resolve(q); }

 private:
  void do_resolve(tcp::resolver::query q) {
    auto self(shared_from_this());
    resolver_.async_resolve(q, [this, self](const boost::system::error_code& ec,
                                            tcp::resolver::iterator it) {
      if (!ec) {
        do_connect(it);
      }
    });
  }

  void do_connect(tcp::resolver::iterator it) {
    auto self(shared_from_this());
    socket_.async_connect(*it,
                          [this, self](const boost::system::error_code& ec) {
                            if (!ec) {
                              do_read();
                            }
                          });
  }

  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(bytes),
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

                                } else if (line_count < lines.size()) {
                                  if (prompt == 2) prompt = 1;
                                  do_read();
                                } else {
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
            } else {
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
};

int main(int argc, char* argv[]) {
  if (getenv("QUERY_STRING") == NULL) {
    std::cout << "fghjk" << std::endl;
    return 0;
  }
  char npserver[5][50];
  char port[5][20];
  char testfile[5][20];
  std::vector<std::string> lines[5];

  // if (getenv("QUERY_STRING") == NULL) return 0;
  char* query = getenv("QUERY_STRING");
  //   char query[15000] =
  //       "h0=127.0.0.1&p0=12094&f0=t1.txt&h1=127.0.0.1&p1=12094&f1=t1.txt&h2=127."
  //       "0.0.1&p2=12094&f2=t1.txt&h3=127.0.0.1&p3=12094&f3=t1.txt&h4=&p4=&f4=";
  char* tmp = strtok(query, "=");

  int k;
  for (k = 0; k < 5; k++) {
    tmp = strtok(NULL, "&");
    if (k < 5 && tmp[2] == '=') break;
    strcpy(npserver[k], tmp);

    tmp = strtok(NULL, "=");
    tmp = strtok(NULL, "&");
    strcpy(port[k], tmp);

    tmp = strtok(NULL, "=");
    tmp = strtok(NULL, "&");
    strcpy(testfile[k], tmp);

    tmp = strtok(NULL, "=");
  }

  int Snum;
  Snum = k;

  char *cgi_path = getenv("EXEC_FILE");
  int cgi_tmp;
  for (int i=0; i<strlen(cgi_path); i++) {
    if (cgi_path[i] == '/') cgi_tmp = i;
  }
  cgi_path[cgi_tmp+1] = '\0';
  strcat(cgi_path, "test_case/");

  for (int j = 0; j < Snum; j++) {
    char testName[40];
    strcpy(testName, cgi_path);
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
    std::make_shared<Client>(io_context, i, npserver[i], port[i], lines[i])
        ->start();
  }
  // Client c0(io_context, 0, npserver[0], port[0], lines[0]);

  std::cout << "Content-type: text/html\n\r\n\r\n";
  html(Snum, npserver, port);

  io_context.run();
}