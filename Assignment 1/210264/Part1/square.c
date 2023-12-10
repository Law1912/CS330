#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		perror("Unable to execute");
		exit(1);
	}
	char *ptr;
	unsigned long result = strtoul(argv[argc - 1], &ptr, 10);
	result *= result;
	if(argc == 2)
	{
		printf("%lu\n", result);
		return 0;
	}
	char *args[argc];
	for (int i = 0; i < argc - 2; i++)
	{
		args[i] = (char *) malloc ((strlen(argv[i + 1]) + 1) * sizeof(char));
		strcpy(args[i], argv[i + 1]);
	}
	args[argc - 2] = (char *) malloc (sizeof(unsigned long));
	sprintf(args[argc - 2], "%lu", result);
	args[argc - 1] = NULL;
	char next[10] = "./";
	strcat(next, argv[1]);
	execvp(next, args);
	perror("Unable to execute");
	exit(1);
	return 0;
}
