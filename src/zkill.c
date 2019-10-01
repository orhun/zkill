/**!
 * zkill, <TODO: Add description>
 * Copyright (C) 2019 by orhun <https://www.github.com/orhun>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "zkill.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <ftw.h>

static int fd;          	 /* File descriptor to be used in file operations */
static char *strPath, 	 	 /* String part of a path in '/proc' */
	fileContent[BLOCK_SIZE], /* Text content of a file */
	buff;                    /* Char variable that used as buffer in read */

/*!
 * Read the given file and return its content.
 *
 * @param filename
 * @return fileContent
 */
static char* readFile(char *fileName) {
	/**
	 * Open file with following flags:
	 * O_RDONLY: Open for reading only.
	 * S_IRUSR: Read permission bit for the owner of the file.
	 * S_IRGRP: Read permission bit for the group owner of the file.
	 * S_IROTH: Read permission bit for other users.
	 */
	fd = open(fileName, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
	/* Check for file open error. */
	if (fd == -1)
		return NULL;
	/**
	 * Read bytes from file descriptor into the buffer.
	 * Use 'read until the end' method since it's not always possible to
	 * read file knowing its size. ('/proc' has zero-length virtual files)
	 */
	for (int i = 0; read(fd, &buff, sizeof(buff)) != 0; i++) {
		fileContent[i] = buff;
	}
	/* Close the file descriptor and return file content. */
	close(fd);
	return fileContent;
}

/*!
 * Check the given process' status.
 *
 * @param procPath (process path in '/proc')
 * @return PROCESS_status
 */
static int checkProcStatus(const char *procPath) {
	/* Array for storing the stat file name of the process. */
	char pidStatFile[sizeof(procPath)+sizeof(STAT_FILE)];
	/* Fill the array with the given parameter and append '/stat'. */
	strcpy(pidStatFile, procPath);
	strcat(pidStatFile, STAT_FILE);
	/* Read the PID status file. */
	char *content = readFile(pidStatFile);
	/* Check for the file read error. */
	if (content == NULL)
		return PROCESS_READ_ERROR;
	/* Check file content for process status.*/
	if (strstr(content, STATE_ZOMBIE) != NULL)
		return PROCESS_ZOMBIE;
	return PROCESS_DRST;
}

/*!
 * Event for receiving tree entry from '/proc'.
 *
 * @param fpath  (path name of the entry)
 * @param sb     (file status structure for fpath)
 * @param tflag  (type flag of the entry)
 * @param ftwbuf (structure that contains entry base and level)
 * @return EXIT_status
 */
static int procEntryRecv(const char *fpath, const struct stat *sb,
		int tflag, struct FTW *ftwbuf) {
	/**
	 * Check for depth of the fpath (1), type of the entry (directory),
	 * base of the fpath (numeric value) to filter entries except PID.
	 */
    if (ftwbuf->level == 1 && tflag == FTW_D &&
        strtol(fpath + ftwbuf->base, &strPath, 10) &&
        !strcmp(strPath, "")) {
		/* Check the process status. */
		switch (checkProcStatus(fpath)) {
			/* D (uninterruptible sleep), R (running), S (sleeping), T (stopped) */
			case PROCESS_DRST:
				fprintf(stderr, "Process: '%s'\r", fpath + ftwbuf->base);
				break;
			case PROCESS_ZOMBIE: 	 /* Defunct (zombie) process. */
				fprintf(stderr, "Process (Z): '%s'\n", fpath + ftwbuf->base);
				break;
			case PROCESS_READ_ERROR: /* Failed to read process' file. */
				fprintf(stderr, "Failed to open file: '%s'\r", fpath);
				break;
			default:
				fprintf(stderr, "\n");
				break;
		}
    }
    return EXIT_SUCCESS;
}

/*!
 * Parse command line arguments.
 *
 * @param argc (argument count)
 * @param argv (argument vector)
 * @return EXIT_status
 */
static int parseArgs(int argc, char **argv){
    int opt;
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v': /* Show version information. */
                fprintf(stderr, "zkill v%s\n", VERSION);
                return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/*!
 * Entry-point
 */
int main(int argc, char *argv[]) {
    /* Parse command line arguments. */
    if(parseArgs(argc, argv))
        return EXIT_SUCCESS;
    /* Call ftw to get '/proc' contents. */
    if (nftw(PROC_FS, procEntryRecv, USE_FDS, FTW_PHYS) == -1)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}