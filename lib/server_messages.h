#pragma once
#include "protocol.h"

/* Errors the server can send as "ERR <reason>\n" */
typedef enum {
    ERR_SERVER_FULL,
    ERR_ALREADY_NAMED,
    ERR_INVALID_NAME,
    ERR_NAME_TAKEN,
    ERR_UNKNOWN_COMMAND,
    ERR_MESSAGE_TOO_LONG,

    ERR_COUNT
} chat_error;

/*
 * printf-style template for a line the server sends, or NULL if the
 * server never sends that command (NICK, QUIT, UNKNOWN).
 *
 *   CHAT_MSG   "MSG %s %.*s\n"   name, text_len (int), text
 *   CHAT_OK    "OK\n"            (no arguments)
 *   CHAT_ERR   "ERR %s\n"        reason
 *   CHAT_JOIN  "JOIN %s\n"       name
 *   CHAT_LEAVE "LEAVE %s\n"      name
 */
const char *server_template(chat_command cmd);

/* Reason text for an error, or NULL if e is out of range. */
const char *error_reason(chat_error e);
