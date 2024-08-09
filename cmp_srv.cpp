#include <stdio.h>
#include <mutex>

#include "common_tools.h"

#define BUF_SIZE 1024

void usage()
{
  log("Usage: cmp_srv <port>\n");
}

class cmp_connection_t : public std::enable_shared_from_this<cmp_connection_t> {
  private:
  boost::asio::io_service& m_io;
  boost::asio::ip::tcp::socket m_socket;
  std::string m_host;
  std::string m_port;
  std::string m_read_buf;
  std::string m_write_buf;
  std::mutex m_wr_mtx;

  public:
  cmp_connection_t(boost::asio::io_service& io_service) : m_io(io_service), m_socket(m_io) {}
  virtual~cmp_connection_t() {}

  boost::asio::ip::tcp::socket& get_socket() { return m_socket; }

  void start_read() {
    m_read_buf.clear();
    m_read_buf.resize(BUF_SIZE);
    m_socket.async_read_some(boost::asio::buffer(m_read_buf),
        std::bind(&cmp_connection_t::read_handler, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
  }

  void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    log("read_handler: transferred: %d\n", bytes_transferred);
    if (!ec || ec.value() == boost::system::errc::success) {
      if (bytes_transferred > 0) {  
        log("%s\n", m_read_buf.data());
      }
      start_read();
    } else {
      log("read error: %s %d\n", ec.message().c_str(), ec.value());
    }
  }

  void send(std::string msg) {
    std::lock_guard<std::mutex> lock(m_wr_mtx);
    bool first = m_write_buf.size() == 0;
    m_write_buf.append(msg);
    if (first) start_write();
  }

  void start_write() {
    auto self = shared_from_this();
    m_socket.async_write_some(boost::asio::buffer(m_write_buf),
        [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
          if (!ec) {
            std::lock_guard<std::mutex> lock(self->m_wr_mtx);
            self->m_write_buf.erase(0, bytes_transferred);
            if (self->m_write_buf.size() > 0) {
              self->start_write();
            }
          } else {
            log("write error: %s\n", ec.message().c_str());
          }
        });
  }
};

class cmp_server_t : public std::enable_shared_from_this<cmp_server_t> {
  private:
  boost::asio::io_service& m_io;
  boost::asio::ip::tcp::acceptor m_acceptor;
  std::shared_ptr<cmp_connection_t> m_connection;

  public:
  cmp_server_t(boost::asio::io_service& io_service) : m_io(io_service), m_acceptor(m_io) {}
  virtual~cmp_server_t() {}

  void listen(int port) {
    if (m_acceptor.is_open()) {
      log("already listening\n");
      return;
    }

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
    log("listening on %d\n", port);
    start_accept();
  }

  void start_accept() {
    m_connection = std::make_shared<cmp_connection_t>(m_io);
    m_acceptor.async_accept(m_connection->get_socket(), [this](const boost::system::error_code& ec) {
      if (!ec) {
        log("accepted\n");
        m_connection->start_read();
        start_accept();
      } else {
        log("accept error: %s\n", ec.message().c_str());
      }
    });
  }
};

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    usage();
    return 1;
  }
  set_signal_handler();

  workers_t workers;
  workers.run(1);

  auto server = std::make_shared<cmp_server_t>(workers.get_io_service());
  server->listen(atoi(argv[1]));

  main_loop();

  workers.stop();

  return 0;
}

