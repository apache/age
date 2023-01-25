/* $Header$ */
#include "pool.h"
#include "pool_memory.h"
#include "parsenodes.h"
#include "gramparse.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum
{
	TEST_CONTINUE,
	TEST_QUIT
};

extern int	GetDatabaseEncoding(void);

int
command(const char *cmd)
{
	char		name[1024],
				value[1024];

	if (*cmd == 'q')
		return TEST_QUIT;

	if (sscanf(cmd, "pset %s %s", name, value) == 2)
	{
		if (strcmp(name, "standard_conforming_strings") == 0)
		{
			parser_set_param(name, value);
			fprintf(stdout, "pset %d\n", standard_conforming_strings);
		}
		else if (strcmp(name, "server_version") == 0)
		{
			parser_set_param(name, value);
			fprintf(stdout, "pset %d\n", server_version_num);
		}
		else if (strcmp(name, "server_encoding") == 0)
		{
			parser_set_param(name, value);
			fprintf(stdout, "pset %d\n", GetDatabaseEncoding());
		}
		else
			fprintf(stdout, "ERROR: pset %s\n", name);
	}
	return TEST_CONTINUE;
}

int
main(int argc, char **argv)
{
	List	   *tree;
	ListCell   *l;
	int			state = TEST_CONTINUE;
	int			notty = (!isatty(fileno(stdin)) || !isatty(fileno(stdout)));
	char		line[1024];

	while (state != TEST_QUIT)
	{
		if (!notty)
			fprintf(stdout, "> ");

		if (!fgets(line, 1024, stdin))
			break;

		*(strchr(line, (int) '\n')) = '\0';

		if (line[0] == '#' || line[0] == '\0')
			continue;

		if (line[0] == '\\')
		{
			state = command(line + 1);
			continue;
		}

		tree = raw_parser(line);

		if (tree == NULL)
		{
			printf("syntax error: %s\n", line);
		}
		else
		{
			foreach(l, tree)
			{
				Node	   *node = (Node *) lfirst(l);

				printf("%s\n", nodeToString(node));
			}
		}

		free_parser();
	}

	return 0;
}
