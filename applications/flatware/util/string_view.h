#ifndef STRING_VIEW_H
#define STRING_VIEW_H
#include <stddef.h>

struct string_view
{
  const char* ptr;
  size_t len;
};

#endif // STRING_VIEW_H
