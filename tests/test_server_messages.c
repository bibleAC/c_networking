/*
 * Tests for the server's line templates and error reasons.
 *
 * Most checks are round trips: format a line with a template, run it
 * through parse_line, and check that the client would read back the
 * same command and arguments. If a template is wrong (missing '\n',
 * missing %s, or the wrong text in a slot), one of these fails.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"
#include "server_messages.h"

/* Formats with a template from the table. The pragmas silence the
 * "format is not a literal" warning only here, where it is on purpose. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
static int format_line(char *buf, size_t size, chat_command cmd, ...) {
    va_list args;
    va_start(args, cmd);
    int len = vsnprintf(buf, size, server_template(cmd), args);
    va_end(args);
    return len;
}
#pragma clang diagnostic pop

#define FORMAT(buf, ...) format_line((buf), sizeof(buf), __VA_ARGS__)

/* ---------- table shape ---------- */

static void test_sent_commands_have_templates(void) {
    chat_command sent[] = { CHAT_MSG, CHAT_OK, CHAT_ERR, CHAT_JOIN, CHAT_LEAVE };
    for (size_t i = 0; i < sizeof sent / sizeof sent[0]; i++)
        assert(server_template(sent[i]) != NULL);
}

static void test_client_only_commands_have_no_template(void) {
    assert(server_template(CHAT_NICK) == NULL);
    assert(server_template(CHAT_QUIT) == NULL);
    assert(server_template(CHAT_UNKNOWN) == NULL);
}

static void test_every_template_ends_with_newline(void) {
    for (int c = 0; c <= CHAT_UNKNOWN; c++) {
        const char *t = server_template((chat_command) c);
        if (t == NULL) continue;
        size_t n = strlen(t);
        assert(n > 0 && t[n - 1] == '\n');
    }
}

static void test_out_of_range_command_is_null(void) {
    assert(server_template((chat_command) 999) == NULL);
}

/* ---------- round trips through parse_line ---------- */

static void test_msg_round_trip(void) {
    char buf[MAX_LINE_LEN];
    const char *text = "hello there";
    int len = FORMAT(buf, CHAT_MSG, "alice", (int) strlen(text), text);
    assert(len > 0 && len < (int) sizeof buf);
    assert(strcmp(buf, "MSG alice hello there\n") == 0);

    line_result r = parse_line(buf, (size_t) len);
    assert(r.command == CHAT_MSG);
    assert(r.argument_len == 17);
    assert(memcmp(r.argument, "alice hello there", 17) == 0);
}

static void test_msg_uses_only_given_text_length(void) {
    /* The text comes from the receive buffer without '\0', so the
     * template must use %.*s and respect the length. */
    char buf[MAX_LINE_LEN];
    const char *received = "hiXXXXXXXX";
    int len = FORMAT(buf, CHAT_MSG, "bob", 2, received);
    assert(strcmp(buf, "MSG bob hi\n") == 0);
    assert(len == (int) strlen("MSG bob hi\n"));
}

static void test_ok_round_trip(void) {
    char buf[MAX_LINE_LEN];
    int len = FORMAT(buf, CHAT_OK);
    assert(strcmp(buf, "OK\n") == 0);
    assert(len == 3);
    assert((size_t) len == strlen(server_template(CHAT_OK)));

    line_result r = parse_line(buf, (size_t) len);
    assert(r.command == CHAT_OK);
    assert(r.argument_len == 0);
}

static void test_join_round_trip(void) {
    char buf[MAX_LINE_LEN];
    int len = FORMAT(buf, CHAT_JOIN, "carol");
    assert(strcmp(buf, "JOIN carol\n") == 0);

    line_result r = parse_line(buf, (size_t) len);
    assert(r.command == CHAT_JOIN);
    assert(r.argument_len == 5);
    assert(memcmp(r.argument, "carol", 5) == 0);
}

static void test_leave_round_trip(void) {
    char buf[MAX_LINE_LEN];
    int len = FORMAT(buf, CHAT_LEAVE, "dave");
    assert(strcmp(buf, "LEAVE dave\n") == 0);

    line_result r = parse_line(buf, (size_t) len);
    assert(r.command == CHAT_LEAVE);
    assert(r.argument_len == 4);
    assert(memcmp(r.argument, "dave", 4) == 0);
}

static void test_err_round_trip(void) {
    char buf[MAX_LINE_LEN];
    int len = FORMAT(buf, CHAT_ERR, error_reason(ERR_NAME_TAKEN));
    assert(strcmp(buf, "ERR name already taken\n") == 0);

    line_result r = parse_line(buf, (size_t) len);
    assert(r.command == CHAT_ERR);
    assert(r.argument_len == strlen("name already taken"));
}

/* Each template must produce a line that parses back to the command
 * it is stored under. Catches a template filed under the wrong slot. */
static void test_each_template_parses_as_its_own_command(void) {
    char buf[MAX_LINE_LEN];
    int len;

    len = FORMAT(buf, CHAT_MSG, "a", 1, "x");
    assert(parse_line(buf, (size_t) len).command == CHAT_MSG);

    len = FORMAT(buf, CHAT_OK);
    assert(parse_line(buf, (size_t) len).command == CHAT_OK);

    len = FORMAT(buf, CHAT_ERR, "x");
    assert(parse_line(buf, (size_t) len).command == CHAT_ERR);

    len = FORMAT(buf, CHAT_JOIN, "a");
    assert(parse_line(buf, (size_t) len).command == CHAT_JOIN);

    len = FORMAT(buf, CHAT_LEAVE, "a");
    assert(parse_line(buf, (size_t) len).command == CHAT_LEAVE);
}

/* ---------- MSG length limit ---------- */

static void test_longest_msg_is_detected_as_too_long(void) {
    /* Longest possible name and the longest text a client line can carry:
     * a 1024-byte line "MSG <text>\n" has 1024 - 4 - 1 = 1019 bytes of text. */
    char name[MAX_NAME_LEN + 1];
    memset(name, 'n', MAX_NAME_LEN);
    name[MAX_NAME_LEN] = '\0';

    char text[MAX_LINE_LEN - 5];
    memset(text, 't', sizeof text);

    char buf[MAX_LINE_LEN];
    int len = FORMAT(buf, CHAT_MSG, name, (int) sizeof text, text);

    /* The server must treat this as too long instead of broadcasting it. */
    assert(len >= (int) sizeof buf);
}

/* ---------- error reasons ---------- */

static void test_every_error_has_a_reason(void) {
    for (int e = 0; e < ERR_COUNT; e++) {
        const char *reason = error_reason((chat_error) e);
        assert(reason != NULL);
        assert(strlen(reason) > 0);
        /* The template adds "ERR " and '\n'; the reason must not. */
        assert(strchr(reason, '\n') == NULL);
        assert(strncmp(reason, "ERR", 3) != 0);
    }
}

static void test_out_of_range_error_is_null(void) {
    assert(error_reason(ERR_COUNT) == NULL);
    assert(error_reason((chat_error) -1) == NULL);
}

int main(void) {
    test_sent_commands_have_templates();
    test_client_only_commands_have_no_template();
    test_every_template_ends_with_newline();
    test_out_of_range_command_is_null();

    test_msg_round_trip();
    test_msg_uses_only_given_text_length();
    test_ok_round_trip();
    test_join_round_trip();
    test_leave_round_trip();
    test_err_round_trip();
    test_each_template_parses_as_its_own_command();

    test_longest_msg_is_detected_as_too_long();

    test_every_error_has_a_reason();
    test_out_of_range_error_is_null();

    return 0;
}
