/*
** EPITECH PROJECT, 2018
** 42sh
** File description:
** home
*/

#include "shell.h"
#include <stdio.h>
#include <stdlib.h>

static int skip_spaces(shell_t *shell, int *i)
{
    while (is_space(shell->line[*i]))
        *i += 1;
    *i -= 1;
    return 1;
}

static void insert_home(shell_t *shell, int i)
{
    char *str = 0;
    int ret = asprintf(
        &str, "%.*s%s%s", i, shell->line, shell->home, shell->line + i + 1);

    if (ret == -1 || str == NULL)
        handle_error("calloc");
    free(shell->line);
    shell->line = str;
}

void skip_string(char *str, int *i)
{
    char quote = str[(*i)++];

    while (str[*i]) {
        if (str[*i] == '\\' && quote != '\'')
            *i += (!!str[(*i) + 1]);
        else if (str[*i] != quote)
            break;
        *i += 1;
    }
    *i -= (str[*i] == 0);
}

void replace_home(shell_t *shell)
{
    if (shell->home == NULL)
        return;
    for (int i = 0; shell->line[i]; i++) {
        if (shell->line[i] == '\\')
            i += !!(shell->line[i + 1]);
        else if (is_space(shell->line[i]))
            skip_spaces(shell, &i);
        else if (shell->line[i] == '\'' || shell->line[i] == '"')
            skip_string(shell->line, &i);
        else if (shell->line[i] == '~')
            insert_home(shell, i);
    }
}
