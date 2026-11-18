#include <assert.h>
#include <string.h>

#include "protocol.h"

/* ---------- parse_line ---------- */

static void test_parse_nick(void) {
    const char *line = "NICK alice\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_NICK);
    assert(r.argument_len == 5);
    assert(memcmp(r.argument, "alice", 5) == 0);
}

static void test_parse_msg_keeps_spaces(void) {
    const char *line = "MSG hello world\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_MSG);
    assert(r.argument_len == 11);
    assert(memcmp(r.argument, "hello world", 11) == 0);
}

static void test_parse_no_space(void) {
    const char *line = "QUIT\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_QUIT);
    assert(r.argument_len == 0);
}

static void test_parse_server_ok(void) {
    const char *line = "OK\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_OK);
    assert(r.argument_len == 0);
}

static void test_parse_server_msg(void) {
    const char *line = "MSG alice hi bob\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_MSG);
    assert(r.argument_len == 12);
    assert(memcmp(r.argument, "alice hi bob", 12) == 0);
}

static void test_parse_space_but_no_argument(void) {
    const char *line = "NICK \n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_NICK);
    assert(r.argument_len == 0);
}

static void test_parse_unknown_longer_command(void) {
    /* "MSGX" must not match "MSG" just because the first 3 bytes agree. */
    const char *line = "MSGX hi\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_UNKNOWN);
}

static void test_parse_unknown_shorter_command(void) {
    const char *line = "MS hi\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_UNKNOWN);
}

static void test_parse_crlf(void) {
    const char *line = "NICK alice\r\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_NICK);
    assert(r.argument_len == 5);
    assert(memcmp(r.argument, "alice", 5) == 0);
}

static void test_parse_empty_line(void) {
    const char *line = "\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_UNKNOWN);
    assert(r.argument_len == 0);
}

static void test_parse_zero_length(void) {
    line_result r = parse_line("", 0);

    assert(r.command == CHAT_UNKNOWN);
    assert(r.argument_len == 0);
}

static void test_parse_lowercase_is_unknown(void) {
    /* Commands are uppercase in the spec. */
    const char *line = "nick alice\n";
    line_result r = parse_line(line, strlen(line));

    assert(r.command == CHAT_UNKNOWN);
}

/* ---------- is_name_valid ---------- */

static void test_name_valid_simple(void) {
    assert(is_name_valid("alice", 5));
}

static void test_name_valid_underscore_and_dash(void) {
    assert(is_name_valid("a_b-c", 5));
}

static void test_name_invalid_empty(void) {
    assert(!is_name_valid("", 0));
}

static void test_name_invalid_digit(void) {
    assert(!is_name_valid("alice1", 6));
}

static void test_name_invalid_space(void) {
    assert(!is_name_valid("al ice", 6));
}

static void test_name_invalid_utf8(void) {
    const char *name = "joão"; /* 5 bytes in UTF-8: the ã is 2 bytes */
    assert(!is_name_valid(name, strlen(name)));
}

static void test_name_length_boundary(void) {
    char name[MAX_NAME_LEN + 1];
    memset(name, 'a', sizeof name);

    assert(is_name_valid(name, MAX_NAME_LEN));      /* exactly 32: OK  */
    assert(!is_name_valid(name, MAX_NAME_LEN + 1)); /* 33: too long    */
}

static void test_name_only_checks_given_length(void) {
    /* "bob1" with length 3 is "bob": the 1 is outside the name. */
    assert(is_name_valid("bob1", 3));
}

int main(void) {
    test_parse_nick();
    test_parse_msg_keeps_spaces();
    test_parse_no_space();
    test_parse_server_ok();
    test_parse_server_msg();
    test_parse_space_but_no_argument();
    test_parse_unknown_longer_command();
    test_parse_unknown_shorter_command();
    test_parse_crlf();
    test_parse_empty_line();
    test_parse_zero_length();
    test_parse_lowercase_is_unknown();

    test_name_valid_simple();
    test_name_valid_underscore_and_dash();
    test_name_invalid_empty();
    test_name_invalid_digit();
    test_name_invalid_space();
    test_name_invalid_utf8();
    test_name_length_boundary();
    test_name_only_checks_given_length();

    return 0;
}
