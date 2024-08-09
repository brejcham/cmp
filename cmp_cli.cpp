#include <stdio.h>
#include <mutex>

#include "common_tools.h"

#define BUF_SIZE 1024

void usage()
{
  log("Usage: client <host> <port>\n");
}

class cmp_client_t : public std::enable_shared_from_this<cmp_client_t> {
  private:
  boost::asio::io_service& m_io;
  boost::asio::ip::tcp::socket m_socket;
  tmr_t m_connect_timer;
  std::string m_host;
  std::string m_port;
  int m_connect_timeout = 10;
  std::string m_read_buf;
  std::string m_write_buf;
  std::mutex m_wr_mtx;

  public:
  cmp_client_t(boost::asio::io_service& io_service) : m_io(io_service), m_socket(m_io), m_connect_timer(m_io) {}
  virtual~cmp_client_t() {}

  void connect(const std::string& host, const std::string& port) {
    m_host = host;
    m_port = port;

    start_connect();
  }

  void start_connect() {
    boost::asio::ip::tcp::resolver resolver(m_io);
    boost::asio::ip::tcp::resolver::query query(m_host, m_port);
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);

    log("connecting to %s:%s\n", m_host.c_str(), m_port.c_str());

    auto self = shared_from_this();
    m_connect_timer.start(m_connect_timeout, [self](const boost::system::error_code& eco) {
      if (eco) return;
      self->m_socket.close();
      log("timeout\n");
      self->start_connect();
    });
    
    m_socket.async_connect(*iter, std::bind(&cmp_client_t::connect_handler, shared_from_this(), std::placeholders::_1));
  }

  void connect_handler(const boost::system::error_code& ec) {
    if (!ec) {
      log("connected\n");
      m_connect_timer.stop();
      start_read();
    } else {
      log("connect error: %s\n", ec.message().c_str());
    }
  }

  void start_read() {
    m_read_buf.clear();
    m_read_buf.resize(BUF_SIZE);
    m_socket.async_read_some(boost::asio::buffer(m_read_buf),
        std::bind(&cmp_client_t::read_handler, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
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

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    usage();
    return 1;
  }
  set_signal_handler();

  workers_t workers;
  workers.run(1);

  auto client = std::make_shared<cmp_client_t>(workers.get_io_service());
  client->connect(argv[1], argv[2]);

  std::this_thread::sleep_for(std::chrono::seconds(1));

  client->send("hello\n");

  main_loop();

  workers.stop();

  return 0;
}

