#pragma once
#include <stddef.h>

#define MAX_LINE_LEN 1024
#define MAX_NAME_LEN 32

typedef enum{
    CHAT_NICK,
    CHAT_MSG,
    CHAT_QUIT,
    CHAT_OK,
    CHAT_ERR,
    CHAT_JOIN,
    CHAT_LEAVE,
    CHAT_UNKNOWN
} chat_command;

typedef struct {
    chat_command command;
    const char* argument;
    size_t argument_len;
} line_result;

line_result parse_line(const char* line, size_t len);
bool is_name_valid(const char* name, size_t len);
