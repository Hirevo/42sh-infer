/*
** is2.c for minishell2 in /home/nicolaspolomack/shell2
** 
** Made by Nicolas Polomack
** Login   <nicolas.polomack@epitech.eu>
** 
** Started on  Sat Mar 25 20:21:25 2017 Nicolas Polomack
** Last update Sun Mar 26 18:43:13 2017 Nicolas Polomack
*/

int	is_separator(char c)
{
  return (c == '>' || c == '<' || c == '|' || c == ';');
}

int	is_space(char c)
{
  return (c == ' ' || c == '\t');
}

int	is_delimiter(char c)
{
  return (c == ';' || c == '|');
}
