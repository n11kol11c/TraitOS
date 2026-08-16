#include "tsh.h"

#include "tstring.h"

#define ENV_MAX    16
#define ENV_KEY_LEN 32
#define ENV_VAL_LEN 128

static char env_keys[ENV_MAX][ENV_KEY_LEN];
static char env_vals[ENV_MAX][ENV_VAL_LEN];
static int  env_count = 0;

int tsh_env_set(const char *key, const char *val)
{
    for (int i = 0; i < env_count; i++)
        if (tstrcmp(env_keys[i], key) == 0) {
            tstrncpy(env_vals[i], val, ENV_VAL_LEN);
            return 0;
        }
    if (env_count >= ENV_MAX)
        return -1;
    tstrncpy(env_keys[env_count], key, ENV_KEY_LEN);
    tstrncpy(env_vals[env_count], val, ENV_VAL_LEN);
    env_count++;
    return 0;
}

const char *tsh_env_get(const char *key)
{
    for (int i = 0; i < env_count; i++)
        if (tstrcmp(env_keys[i], key) == 0)
            return env_vals[i];
    return NULL;
}

void tsh_env_seed(void)
{
    tsh_env_set("OS", "trigeros");
    tsh_env_set("HOSTNAME", "trigeros");
    tsh_env_set("USER", "root");
    tsh_env_set("HOME", "/");
}

void tsh_env_list(void (*cb)(const char *key, const char *val))
{
    for (int i = 0; i < env_count; i++)
        cb(env_keys[i], env_vals[i]);
}

/* ---- command history ---------------------------------------------------- */

static char hist_entries[TSH_HIST_MAX][TSH_HIST_LEN];
static int  hist_count = 0;

int tsh_hist_push(const char *line)
{
    if (hist_count > 0 &&
        tstrcmp(hist_entries[hist_count - 1], line) == 0)
        return 0;                      /* drop consecutive duplicates */
    if (hist_count < TSH_HIST_MAX) {
        tstrncpy(hist_entries[hist_count], line, TSH_HIST_LEN);
        hist_count++;
    } else {
        tmemmove(hist_entries[0], hist_entries[1],
                 (TSH_HIST_MAX - 1) * TSH_HIST_LEN);
        tstrncpy(hist_entries[TSH_HIST_MAX - 1], line, TSH_HIST_LEN);
    }
    return 1;
}

int tsh_hist_count(void)
{
    return hist_count;
}

const char *tsh_hist_get(int i)
{
    if (i < 0 || i >= hist_count)
        return NULL;
    return hist_entries[i];
}

void tsh_hist_clear(void)
{
    hist_count = 0;
}

static int parse_num(const char *s, int *out)
{
    int v = 0;
    if (*s == '\0')
        return 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    if (*s != '\0')
        return 0;
    *out = v;
    return 1;
}

int tsh_hist_expand(const char *line, char *out, size_t outsz)
{
    const char *p;
    int idx;

    if (line[0] != '!')
        return 0;
    p = line + 1;
    if (p[0] == '!' && p[1] == '\0') {      /* !! = last command */
        if (hist_count == 0)
            return -1;
        tstrncpy(out, hist_entries[hist_count - 1], outsz);
        return 1;
    }
    if (!parse_num(p, &idx))
        return 0;
    if (idx < 0 || idx >= hist_count)
        return -1;
    tstrncpy(out, hist_entries[idx], outsz);
    return 1;
}

/* ---- tokenizer ---------------------------------------------------------- */

static char arg_buf[TSH_ARGV_MAX][TSH_ARG_MAX];

static size_t arg_append(char *dst, size_t n, char c)
{
    if (n + 1 < TSH_ARG_MAX)
        dst[n] = c;
    return n + 1;
}

/* Expand $NAME from the environment into dst. Returns the name length
 * consumed from `name` (0 if it is not a valid variable name). */
static size_t expand_var(const char *name, char *dst, size_t *n)
{
    const char *start = name;
    if (!((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z') ||
          *name == '_'))
        return 0;
    name++;
    while ((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z') ||
           (*name >= '0' && *name <= '9') || *name == '_')
        name++;

    char key[ENV_KEY_LEN];
    size_t kl = (size_t)(name - start);
    if (kl >= ENV_KEY_LEN)
        kl = ENV_KEY_LEN - 1;
    tmemcpy(key, start, kl);
    key[kl] = '\0';

    const char *val = tsh_env_get(key);
    if (val)
        while (*val)
            *n = arg_append(dst, *n, *val++);
    return (size_t)(name - start);
}

int tsh_tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    const char *p = line;

    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (*p == '#')                   /* comment line */
            break;

        char *dst = arg_buf[argc];
        size_t n = 0;
        int in_quote = 0;

        while (*p && (in_quote || (*p != ' ' && *p != '\t'))) {
            char c = *p++;
            if (c == '"') {
                in_quote = !in_quote;
                continue;
            }
            if (c == '\\' && *p) {
                n = arg_append(dst, n, *p++);   /* \ escapes next char */
                continue;
            }
            if (c == '$') {
                size_t used = expand_var(p, dst, &n);
                if (used > 0)
                    p += used;
                else
                    n = arg_append(dst, n, '$');
                continue;
            }
            n = arg_append(dst, n, c);
        }

        dst[n] = '\0';
        argv[argc++] = dst;
    }
    return argc;
}
