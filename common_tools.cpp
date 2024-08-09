
#include <stdio.h>

#include "common_tools.h"


void log(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

static bool g_initialize_close = false;
void signal_handler(int signum)
{
  log("Caught signal %d\n", signum);
  g_initialize_close = true;
}
void set_signal_handler()
{
  struct sigaction sa = {0};
  sa.sa_handler = signal_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

void main_loop() {
  while (!g_initialize_close) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
