#ifndef TSH_H
#define TSH_H

#define TSH_ARGV_MAX 8
#define TSH_ARG_MAX  128

/* Environment variables (fixed-size table, no allocation). */
int          tsh_env_set(const char *key, const char *val);
const char  *tsh_env_get(const char *key);
void         tsh_env_seed(void);
void         tsh_env_list(void (*cb)(const char *key, const char *val));

/* Command history (ring buffer, oldest first, consecutive dupes dropped). */
#define TSH_HIST_MAX 32
#define TSH_HIST_LEN 128
int           tsh_hist_push(const char *line);
int           tsh_hist_count(void);
const char   *tsh_hist_get(int i);
void          tsh_hist_clear(void);

/* Split `line` into argv: whitespace-separated, "double quotes" group,
 * backslash escapes the next char, and $NAME expands from the environment.
 * Writes pointers into internal storage (do not call concurrently).
 * Returns argc (0 for an empty line). */
int tsh_tokenize(char *line, char **argv, int max);

#endif
