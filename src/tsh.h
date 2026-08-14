#ifndef TSH_H
#define TSH_H

#define TSH_ARGV_MAX 8
#define TSH_ARG_MAX  128

/* Environment variables (fixed-size table, no allocation). */
int          tsh_env_set(const char *key, const char *val);
const char  *tsh_env_get(const char *key);
void         tsh_env_seed(void);
void         tsh_env_list(void (*cb)(const char *key, const char *val));

/* Split `line` into argv: whitespace-separated, "double quotes" group,
 * backslash escapes the next char, and $NAME expands from the environment.
 * Writes pointers into internal storage (do not call concurrently).
 * Returns argc (0 for an empty line). */
int tsh_tokenize(char *line, char **argv, int max);

#endif
