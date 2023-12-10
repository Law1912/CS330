#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

unsigned long tot_size = 0;
unsigned long num = 0;

void cal_dir_size(char *path, int fd[])
{
	struct dirent *entry;
	struct stat info;
	DIR *dir = opendir(path);
	if(dir == NULL)
	{
		perror("Unable to execute\n");
		exit(1);
	}
	if(lstat(path, &info) == -1)
	{
		perror("Unable to execute\n");
		exit(1);
	}
	tot_size += info.st_size;
	while(entry = readdir(dir))
	{
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;
		char newpath[4096] = {};
		sprintf(newpath, "%s/%s", path, entry->d_name);
		if(lstat(newpath, &info) == -1)
		{
			perror("Unable to execute\n");
			exit(1);
		}
		if(S_ISREG(info.st_mode))
			tot_size += info.st_size;
		else if(S_ISDIR(info.st_mode))
		{
			num++;
			int pid = fork();
			if(pid == -1)
			{
				perror("Unable to execute\n");
				exit(1);
			}
			else if(pid == 0)
			{
				close(1);
				dup(fd[1]);
				close(fd[1]);
				closedir(dir);
				execl("./myDU", "./myDU", newpath, (char *) NULL);
				perror("Unable to execute\n");
				exit(1);
			}
		}
		else
		{
			char new_path[4096] = {};
			unsigned long siz = readlink(newpath, new_path, 4096);
			if(siz == -1)
			{
				perror("Unable to execute\n");
				exit(1);
			}
			char link_path[4096] = {};
			sprintf(link_path, "%s/%s", path, new_path);
			cal_dir_size(link_path, fd);
		}
	}
	closedir(dir);
}

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		perror("Unable to execute\n");
		exit(1);
	}
	char *cur_path = argv[1];
	int fd[2];
	if(pipe(fd) == -1)
	{
		perror("Unable to execute\n");
		exit(1);
	}
	close(0);
	dup(fd[0]);
	close(fd[0]);
	cal_dir_size(cur_path, fd);
	while(wait(NULL) > 0);
	while(num-- > 0)
	{
		unsigned long child_size = 0, total_len = 0, len = 0;
		char buffer[20] = {};
//		scanf("%lu\n", child_size);
		while(buffer[total_len - 1] != '\n' && (len = read(0, buffer + total_len, 1)) > 0)
		{
			total_len += len;
		}
		if(len == -1)
		{
			perror("Unable to execute\n");
			exit(1);
		}
		buffer[total_len - 1] = '\0';
		child_size = strtoul(buffer, NULL, 10);
		tot_size += child_size;
		}
	printf("%lu\n", tot_size);
	// char buffer[20] = {};
	// int len = snprintf(buffer, sizeof(buffer), "%lu", tot_size);
	// write(1, buffer, sizeof(buffer));
	exit(0);
}
