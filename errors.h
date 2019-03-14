#pragma once

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void abort_errno(const char* s)
{
  fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
  exit(EXIT_FAILURE);
}

static void abort_msg(const char* s)
{
  fprintf(stderr, "%s\n", s);
  exit(EXIT_FAILURE);
}

static void abort_info(const char* msg, const char* info)
{
  fprintf(stderr, "%s %s\n", msg, info);
  exit(EXIT_FAILURE);
}
