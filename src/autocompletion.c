/*
** EPITECH PROJECT, 2018
** 42sh
** File description:
** auto_complete
*/

#include "autocompletion.h"
#include "my.h"
#include "prompt.h"
#include "shell.h"
#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int sort_matches(void *a, size_t idx_a, void *b, size_t idx_b)
{
    (void)(idx_a);
    (void)(idx_b);
    return strcmp(a, b);
}

static bool find_match(void *ctx, void *elem, size_t idx)
{
    (void)(idx);
    return lstr_equals(ctx, elem);
}

static bool filter_duplicate_matches(void *ctx, void *elem, size_t idx)
{
    vec_t *arr = ctx;
    size_t first = (size_t)(lvec_find_index(arr, find_match, elem));
    if (first != idx) {
        free(elem);
        return false;
    }
    return true;
}

// Accounts for trailing slashes
static char *custom_basename(char const *path)
{
    char *found = strrchr(path, '/');
    if (found == 0)
        return strdup(path);
    else
        return strdup(found + 1);
}

// Accounts for trailing slashes
static char *custom_dirname(char const *path)
{
    char *found = strrchr(path, '/');
    if (found == 0)
        return strdup("");
    else if (found == path && found == strchr(path, '/'))
        return strdup("/");
    else
        return strndup(path, found - path);
}

static vec_t *find_path_matches(Shell *shell, char *token)
{
    (void)(shell);
    vec_t *output = lvec_new();

    char *token_name = custom_basename(token);
    if (token_name == 0)
        return output;

    char *token_dir = custom_dirname(token);
    if (token_dir == 0) {
        free(token_name);
        return output;
    }

    DIR *dir = opendir(lstr_equals(token_dir, "") ? "." : token_dir);
    if (dir == 0) {
        free(token_name);
        free(token_dir);
        return output;
    }

    for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
        if (lstr_equals(entry->d_name, "..") ||
            lstr_equals(entry->d_name, ".") ||
            lstr_equals(entry->d_name, ".DS_Store"))
            continue;
        else if (lstr_starts_with(entry->d_name, token_name)) {
            char *pathname = path_join(token_dir, entry->d_name);
            if (pathname != 0) {
                if (entry->d_type == DT_DIR)
                    pathname = lstr_append(pathname, "/");
                lvec_push_back(output, 1, pathname);
            }
        }
    }

    lvec_sort(output, sort_matches);

    closedir(dir);
    free(token_name);
    free(token_dir);
    return output;
}

static vec_t *find_commands_matches(Shell *shell, char *token)
{
    if (lstr_includes(token, "/"))
        return find_path_matches(shell, token);

    vec_t *output = lvec_new();

    // Alias matches
    vec_t *aliases = lhmap_keys(shell->aliases);
    for (size_t idx = 0; idx < lvec_size(aliases); idx++) {
        char *alias = lvec_at(aliases, idx);
        if (lstr_starts_with(alias, token)) {
            char *entry = strdup(alias);
            if (entry != 0)
                lvec_push_back(output, 1, entry);
        }
    }

    // Builtin matches
    vec_t *builtins = lhmap_keys(shell->builtins);
    for (size_t idx = 0; idx < lvec_size(builtins); idx++) {
        char *builtin = lvec_at(builtins, idx);
        if (lstr_starts_with(builtin, token)) {
            char *entry = strdup(builtin);
            if (entry != 0)
                lvec_push_back(output, 1, entry);
        }
    }

    // standard ${PATH} matches
    char *path_var = getenv("PATH");
    if (path_var == 0)
        goto end;

    vec_t *paths = lstr_split(path_var, ":");
    if (paths == 0)
        goto end;

    for (size_t idx = 0; idx < lvec_size(paths); idx++) {
        char *path = lvec_at(paths, idx);
        DIR *dir = opendir(path);
        if (dir == 0)
            continue;

        for (struct dirent *entry = readdir(dir); entry;
             entry = readdir(dir)) {
            if (lstr_equals(entry->d_name, "..") ||
                lstr_equals(entry->d_name, ".") ||
                lstr_equals(entry->d_name, ".DS_Store"))
                continue;
            else if (lstr_starts_with(entry->d_name, token)) {
                char *cmd = strdup(entry->d_name);
                if (cmd != 0)
                    lvec_push_back(output, 1, cmd);
            }
        }

        closedir(dir);
    }

    lvec_clear(paths, true);
    lvec_drop(paths);

end:
    lvec_filter(output, filter_duplicate_matches, output);
    lvec_sort(output, sort_matches);

    return output;
}

static vec_t *find_matches(Shell *shell, Token token)
{
    if (token.is_command) {
        return find_commands_matches(shell, token.token);
    } else {
        return find_path_matches(shell, token.token);
    }
}

static OPTION(CharPtr)
    complete_forward(Shell *shell, char *line, Token token, char *match)
{
    char *sanitized_token = sanitize_single_arg(token.token, false);
    if (sanitized_token == 0)
        return NONE(CharPtr);
    char *sanitized_match = sanitize_single_arg(match, false);
    if (sanitized_match == 0) {
        free(sanitized_token);
        return NONE(CharPtr);
    }

    size_t len = strlen(sanitized_token);
    char *to_add = sanitized_match + len;

    char *ret =
        fmtstr("%.*s%s%s", shell->w.cur, line, to_add, line + shell->w.cur);
    if (ret == 0) {
        free(sanitized_match);
        free(sanitized_token);
        return NONE(CharPtr);
    }

    putstr("%s%s", to_add, line + shell->w.cur);

    size_t backw_len = strlen(line + shell->w.cur);
    for (size_t i = 0; i < backw_len; i++)
        putstr(shell->w.backw);
    fflush(stdout);

    free(sanitized_token);
    free(line);
    shell->w.cur += strlen(to_add);
    free(sanitized_match);

    return SOME(CharPtr, ret);
}

static void *render_match(void *ctx, void *acc, void *elem, size_t idx)
{
    Token *token = ctx;

    if (idx == 0) {
        if (token->is_command) {
            return sanitize_double_quotes(elem, false);
        } else {
            return sanitize_double_quotes(basename(elem), false);
        }
    } else {
        acc = lstr_append(acc, "\\\\n");
        if (token->is_command) {
            return lstr_append(acc, sanitize_double_quotes(elem, false));
        } else {
            return lstr_append(
                acc, sanitize_double_quotes(basename(elem), false));
        }
    }
}

static char *longest_common_prefix(char *s1, char *s2)
{
    size_t len = 0;

    while (s1[len] && s2[len] && s1[len] == s2[len])
        len += 1;
    return strndup(s1, len);
}

static void *prefix_match(void *ctx, void *acc, void *elem, size_t idx)
{
    (void)(ctx);
    if (idx == 0)
        return strdup(elem);
    else {
        char *prefix = longest_common_prefix(acc, elem);
        free(acc);
        return prefix;
    }
}

static OPTION(CharPtr)
    complete_choices(Shell *shell, char *line, Token token, vec_t *matches)
{
    char *prefix = lvec_reduce(matches, prefix_match, 0, 0);
    if (strlen(prefix) > strlen(token.token)) {
        OPTION(CharPtr) opt = complete_forward(shell, line, token, prefix);
        if (IS_NONE(opt)) {
            return NONE(CharPtr);
        } else {
            line = OPT_UNWRAP(opt);
        }
    }
    free(prefix);

    char *rendered_matches = lvec_reduce(matches, render_match, &token, 0);

    static const char *command = "echo \"%s\" | sort | column";
    char *rendered = fmtstr(command, rendered_matches);
    free(rendered_matches);
    if (rendered == 0)
        return NONE(CharPtr);

    size_t len = 0;
    if (line) {
        len = strlen(line + shell->w.cur);
        for (size_t i = 0; i < len; i++)
            putstr(shell->w.forw);
        putstr("\n");
        fflush(stdout);
    }

    quick_exec(shell, rendered);
    print_prompt(shell);
    if (line) {
        putstr(line);

        for (size_t i = 0; i < len; i++)
            putstr(shell->w.backw);
        fflush(stdout);
    }

    return SOME(CharPtr, line);
}

OPTION(Token) extract_token(char *line, size_t cur)
{
    if (line == NULL) {
        Token ret = (Token){
            .token = strdup(""),
            .is_command = true,
        };
        return SOME(Token, ret);
    }

    size_t start = 0;
    bool is_command = true;
    for (size_t i = 0; line[i] && i < cur; i++) {
        if (line[i] == '\\')
            i += !!(line[i + 1]);
        else if (is_space(line[i]) || is_separator(line[i])) {
            if (is_delimiter(line[i])) {
                is_command = true;
                while (is_space(line[++i]))
                    ;
                i -= 1;
            } else {
                is_command = false;
            }
            start = i + 1;
        }
    }

    Token ret = (Token){
        .token = format_arg(lstr_slice(line, start, cur)),
        .is_command = is_command,
    };
    return SOME(Token, ret);
}

void autocomplete(Shell *shell, char **line)
{
    OPTION(Token) opt_token = extract_token(*line, shell->w.cur);
    if (IS_NONE(opt_token))
        return;
    Token token = OPT_UNWRAP(opt_token);

    vec_t *matches = find_matches(shell, token);

    if (lvec_size(matches) == 1) {
        OPTION(CharPtr)
        opt = complete_forward(shell, *line, token, lvec_front(matches));
        if (IS_SOME(opt)) {
            *line = OPT_UNWRAP(opt);
        }
    } else if (lvec_size(matches) > 1) {
        OPTION(CharPtr) opt = complete_choices(shell, *line, token, matches);
        if (IS_SOME(opt)) {
            *line = OPT_UNWRAP(opt);
        }
    }

    lvec_clear(matches, true);
    lvec_drop(matches);
    free(token.token);
}
