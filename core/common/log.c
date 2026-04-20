#include "common/log.h"

#include <stdarg.h>
#include <stdio.h>

static Log_Level g_minimum_level = LOG_LEVEL_DEBUG;

static const char* level_name(Log_Level level) {
  switch (level) {
    case LOG_LEVEL_TRACE: return "TRACE";
    case LOG_LEVEL_DEBUG: return "DEBUG";
    case LOG_LEVEL_INFO:  return "INFO ";
    case LOG_LEVEL_WARN:  return "WARN ";
    case LOG_LEVEL_ERROR: return "ERROR";
  }
  return "?????";
}

void log_set_minimum_level(Log_Level level) {
  g_minimum_level = level;
}

void log_message(Log_Level level, const char* format, ...) {
  if (level < g_minimum_level) return;

  fprintf(stderr, "[%s] ", level_name(level));

  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fputc('\n', stderr);
}
