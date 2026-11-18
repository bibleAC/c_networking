#include "protocol.h"
#include <ctype.h>
#include <string.h>

typedef struct {
    const char* name;
    chat_command cmd;
} command_entry;

static const command_entry entry_list[] = {
    {.cmd = CHAT_NICK,  .name = "NICK"},
    {.cmd = CHAT_MSG,   .name = "MSG"},
    {.cmd = CHAT_QUIT,  .name = "QUIT"},
    {.cmd = CHAT_OK,    .name = "OK"},
    {.cmd = CHAT_ERR,   .name = "ERR"},
    {.cmd = CHAT_JOIN,  .name = "JOIN"},
    {.cmd = CHAT_LEAVE, .name = "LEAVE"},
};

static const size_t entry_count = sizeof(entry_list) / sizeof(command_entry);

line_result parse_line(const char* line, size_t len){
    line_result res = {
        .command = CHAT_UNKNOWN,
        .argument = NULL,
        .argument_len = 0
    };

    // Trim terminators
    while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) len --;
    if (len == 0) return res;

    // Find arguments
    size_t cmd_len;
    size_t arg_len;
    const char* space = memchr(line, ' ', len);
    if (space == NULL){
        // No argument
        cmd_len = len;
    } else {
        cmd_len = space - line;
        arg_len = (line + len) - (space + 1);
        if (arg_len > 0) {
            res.argument = space + 1;
            res.argument_len = arg_len;
        }
    }

    // Check command
    for(size_t i = 0; i < entry_count; i++){
        if(((size_t) strlen(entry_list[i].name) == cmd_len) &&
                memcmp(line, entry_list[i].name, cmd_len) == 0){
            res.command = entry_list[i].cmd;
            break;
        }
    }
    return res;
}

bool is_name_valid(const char* name, size_t len){
    if(len > MAX_NAME_LEN || len == 0) return false;
    for(const char *c = name; c < (name + len); c++){
        if(!isalpha((unsigned char)*c) && *c != '_' && *c != '-') return false;
    }
    return true;
}
