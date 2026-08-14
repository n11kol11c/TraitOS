/* Host-side smoke test for the shell tokenizer + environment (tsh.c).
 * No emulator required. Run via `make smoke`.
 */
#include <stdio.h>
#include <string.h>

#include "tsh.h"

static int errors;
static int checked;

static void check(const char *label, const char *got, const char *want)
{
    checked++;
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: got \"%s\", want \"%s\"\n", label, got, want);
        errors++;
    }
}

int main(void)
{
    char *argv[TSH_ARGV_MAX];
    int argc;
    char line[256];

    tsh_env_seed();

    strcpy(line, "cat /etc/hostname");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("plain argc", argc == 2 ? "2" : "?", "2");
    if (argc >= 2) {
        check("plain[0]", argv[0], "cat");
        check("plain[1]", argv[1], "/etc/hostname");
    }

    strcpy(line, "write /tmp/note \"hello world\"");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("quote argc", argc == 3 ? "3" : "?", "3");
    if (argc >= 3)
        check("quote[2]", argv[2], "hello world");

    strcpy(line, "echo \"$USER is here\"");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("var argc", argc == 2 ? "2" : "?", "2");
    if (argc >= 2)
        check("var[1]", argv[1], "root is here");

    strcpy(line, "echo $HOME / $UNSET $OS");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("multi argc", argc == 5 ? "5" : "?", "5");
    if (argc >= 5) {
        check("multi[1]", argv[1], "/");
        check("multi[2]", argv[2], "/");
        check("multi[3]", argv[3], "");
        check("multi[4]", argv[4], "traitos");
    }

    strcpy(line, "echo a\\ b \"c\\\"d\"");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("escape argc", argc == 3 ? "3" : "?", "3");
    if (argc >= 3) {
        check("escape[1]", argv[1], "a b");
        check("escape[2]", argv[2], "c\"d");
    }

    strcpy(line, "echo \\$HOME and $HOSTNAME");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("dollar argc", argc == 4 ? "4" : "?", "4");
    if (argc >= 4) {
        check("dollar[1]", argv[1], "$HOME");
        check("dollar[3]", argv[3], "traitos");
    }

    strcpy(line, "  echo\tspaced   args  ");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("space argc", argc == 3 ? "3" : "?", "3");
    if (argc >= 3)
        check("space[2]", argv[2], "args");

    strcpy(line, "set GREETING=\"hello there\"");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("set argc", argc == 2 ? "2" : "?", "2");
    if (argc >= 2)
        check("set[1]", argv[1], "GREETING=hello there");

    strcpy(line, "   ");
    argc = tsh_tokenize(line, argv, TSH_ARGV_MAX);
    check("blank", argc == 0 ? "0" : "?", "0");

    tsh_env_set("NAME", "traitos");
    check("env_get", tsh_env_get("NAME") ? tsh_env_get("NAME") : "(null)",
          "traitos");
    check("env_get missing", tsh_env_get("NOPE") ? "set" : "(null)", "(null)");

    printf("\n%s (%d/%d passed)\n",
           errors ? "SHELL SMOKE TEST FAILED" : "SHELL SMOKE TEST PASSED",
           checked - errors, checked);
    return errors ? 1 : 0;
}
