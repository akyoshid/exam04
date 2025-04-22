/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   microshell.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: akyoshid <akyoshid@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/22 10:05:53 by akyoshid          #+#    #+#             */
/*   Updated: 2025/04/22 10:07:36 by akyoshid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

//
//header
//
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

enum e_sep_type
{
	NOT_SEP = -1,
	SEP_NULL,
	SEP_PIPE,
	SEP_SEMICOLON,
};

typedef struct s_fd_set
{
	int	stdin_dup;
	int	stdout_dup;
	int	pipe_read;
}		t_fd_set;

//
//utils
//
int	ft_strlen(char *str)
{
	int	i;

	if (str == NULL)
		return (-1);
	i = 0;
	while (str[i] != '\0')
		i++;
	return (i);
}

// #include <stdio.h>

void	handle_fatal_error(char *err_code)
{
	(void)err_code;
	// perror("");
	// write(2, err_code, 1);
	write(2, "error: fatal\n", 13);
	exit(EXIT_FAILURE);
}

void	x_write(int fd, void *buf, size_t count)
{
	if (write(fd, buf, count) == -1)
		handle_fatal_error("1");
}

void	w_close(int fd)
{
	if (fd == -1 || fd == STDIN_FILENO
		|| fd == STDOUT_FILENO || fd == STDERR_FILENO)
		return ;
	if (close(fd) == -1)
		handle_fatal_error("2");
}

pid_t	x_fork(void)
{
	int	ret;

	ret = fork();
	if (ret == -1)
		handle_fatal_error("3");
	return (ret);
}

pid_t	x_waitpid(pid_t pid, int *wstatus, int options)
{
	int	ret;

	ret = waitpid(pid, wstatus, options);
	if (ret == -1)
		handle_fatal_error("4");
	return (ret);
}

int	x_dup(int oldfd)
{
	int	ret;

	ret = dup(oldfd);
	if (ret == -1)
		handle_fatal_error("5");
	return (ret);
}

void	w_dup2(int oldfd, int newfd)
{
	w_close(newfd);
	if (oldfd == -1 || newfd == -1)
		return ;
	if (dup2(oldfd, newfd) == -1)
		handle_fatal_error("6");
}

void	x_pipe(int pipefd[2])
{
	if (pipe(pipefd) == -1)
		handle_fatal_error("7");
}

//
//fd
//
void	init_fd_set(t_fd_set *fd_set)
{
	fd_set->stdin_dup = x_dup(0);
	fd_set->stdout_dup = x_dup(1);
	fd_set->pipe_read = -1;
}

void	close_fd_set(t_fd_set *fd_set)
{
	w_close(fd_set->stdin_dup);
	w_close(fd_set->stdout_dup);
	w_close(fd_set->pipe_read);
}

void	restore_stdio(t_fd_set *fd_set)
{
	w_dup2(fd_set->stdin_dup, STDIN_FILENO);
	w_dup2(fd_set->stdout_dup, STDOUT_FILENO);
}

void	setup_io(t_fd_set *fd_set, int sep_type)
{
	int	pipe[2];

	w_dup2(fd_set->pipe_read, STDIN_FILENO);
	w_close(fd_set->pipe_read);
	fd_set->pipe_read = -1;
	if (sep_type == SEP_PIPE)
	{
		x_pipe(pipe);
		fd_set->pipe_read = pipe[0];
		w_dup2(pipe[1], STDOUT_FILENO);
		w_close(pipe[1]);
	}
}

//
//main
//
int	is_sep(char *argv_i, int *sep_type)
{
	if (argv_i == NULL)
	{
		*sep_type = SEP_NULL;
		return (1);
	}
	else if (strcmp(argv_i, "|") == 0)
	{
		*sep_type = SEP_PIPE;
		return (1);
	}
	else if (strcmp(argv_i, ";") == 0)
	{
		*sep_type = SEP_SEMICOLON;
		return (1);
	}
	*sep_type = NOT_SEP;
	return (0);
}

int	count_cmd_args(char **argv, int *sep_type)
{
	int	i;

	i = 0;
	while (1)
	{
		if (is_sep(argv[i], sep_type) == 1)
			break ;
		i++;
	}
	return (i);
}

int	cd_builtin(char **cmd_args)
{
	if (cmd_args[1] == NULL || cmd_args[2] != NULL)
	{
		write(2, "error: cd: bad arguments\n", 25);
		return (EXIT_FAILURE);
	}
	if (chdir(cmd_args[1]) == -1)
	{
		write(2, "error: cd: cannot change directory to ", 38);
		write(2, cmd_args[1], ft_strlen(cmd_args[1]));
		write(2, "\n", 1);
		return (EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);
}

int	exec_cmd(char **cmd_args, char **envp)
{
	pid_t	pid;
	int		status;

	if (cmd_args[0] == NULL)
		return (EXIT_SUCCESS);
	if (strcmp(cmd_args[0], "cd") == 0)
		return (cd_builtin(cmd_args));
	pid = x_fork();
	if (pid == 0)
	{
		if (execve(cmd_args[0], cmd_args, envp) == -1)
		{
			write(2, "error: cannot execute ", 22);
			write(2, cmd_args[0], ft_strlen(cmd_args[0]));
			write(2, "\n", 1);
			exit(EXIT_FAILURE);
		}
	}
	x_waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		return (WTERMSIG(status) + 128);
	else
		return (EXIT_FAILURE);
}

int	exec_args(char **argv, char **envp, t_fd_set *fd_set)
{
	int	cmd_args_count;
	int	sep_type;
	int	ret;

	ret = EXIT_SUCCESS;
	while (*argv != NULL)
	{
		cmd_args_count = count_cmd_args(argv, &sep_type);
		argv[cmd_args_count] = NULL;
		setup_io(fd_set, sep_type);
		ret = exec_cmd(argv, envp);
		if (sep_type == SEP_NULL)
			break ;
		else
		{
			argv += cmd_args_count + 1;
			restore_stdio(fd_set);
		}
	}
	return (ret);
}

int	main(int argc, char **argv, char **envp)
{
	t_fd_set	fd_set;
	int			ret;

	if (argc == 1)
		return (EXIT_SUCCESS);
	init_fd_set(&fd_set);
	argv++;
	ret = exec_args(argv, envp, &fd_set);
	close_fd_set(&fd_set);
	return (ret);
}
