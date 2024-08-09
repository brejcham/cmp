
#pragma once

#include <boost/asio.hpp>
#include <string>
#include <thread>
#include <vector>


void log(const char *fmt, ...);
void set_signal_handler();
void main_loop();

class workers_t {
  private:
  boost::asio::io_service io;
  boost::asio::io_service::work work;
  std::vector<std::thread> threads;

  public:
  workers_t() : io(), work(io) {}
  ~workers_t() {}
  boost::asio::io_service& get_io_service() {
    return io;
  }
  
  void run(int i) {
    for (int j = 0; j < i; ++j) {
      threads.emplace_back([this]() {
        io.run();
      });
    }
  }

  void stop() {
    io.stop();
    for (auto& t : threads) {
      t.join();
    }
  }
};

class tmr_t {
  private:
    boost::asio::deadline_timer tm;
  public:
    tmr_t(boost::asio::io_service& io_service) : tm(io_service) {}
    virtual ~tmr_t() {}

    void start(int timeout, std::function<void(const boost::system::error_code& error)> cb) {
      tm.expires_from_now(boost::posix_time::seconds(timeout));
      tm.async_wait(cb);
    }
    void stop() {
      tm.cancel();
    }
};
