#include "server_messages.h"
#include "protocol.h"
#include <stddef.h>

static const char *const command_templates[] = {
    [CHAT_MSG]     = "MSG %s %.*s\n",
    [CHAT_OK]      = "OK\n",
    [CHAT_ERR]     = "ERR %s\n",
    [CHAT_JOIN]    = "JOIN %s\n",
    [CHAT_LEAVE]   = "LEAVE %s\n",
    [CHAT_NICK]    = NULL,
    [CHAT_QUIT]    = NULL,
    [CHAT_UNKNOWN] = NULL,
};

static const char *const error_reasons[] = {
    [ERR_SERVER_FULL]      = "server full",
    [ERR_ALREADY_NAMED]    = "already named",
    [ERR_INVALID_NAME]     = "invalid name",
    [ERR_NAME_TAKEN]       = "name already taken",
    [ERR_UNKNOWN_COMMAND]  = "unknown command",
    [ERR_MESSAGE_TOO_LONG] = "message too long",
};

static_assert(sizeof(error_reasons) / sizeof(error_reasons[0]) == ERR_COUNT,
              "every chat_error needs a reason in error_reasons");

static const size_t template_count =
    sizeof(command_templates) / sizeof(command_templates[0]);

const char *server_template(chat_command cmd) {
    if ((size_t) cmd >= template_count) return NULL;
    return command_templates[cmd];
}

const char *error_reason(chat_error e) {
    if (e < 0 || e >= ERR_COUNT) return NULL;
    return error_reasons[e];
}
