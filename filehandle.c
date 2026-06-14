#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void print_usage(const char *program_name) {
	fprintf(stderr,
			"Usage:\n"
			"  %s info <file>\n"
			"  %s read <file>\n"
			"  %s write <file> <text>\n"
			"  %s append <file> <text>\n"
			"  %s copy <source> <destination>\n"
			"  %s move <source> <destination>\n"
			"  %s delete <file>\n"
			"  %s touch <file>\n",
			program_name, program_name, program_name, program_name,
			program_name, program_name, program_name, program_name);
}

static void print_error(const char *action, const char *path) {
	fprintf(stderr, "%s '%s': %s\n", action, path, strerror(errno));
}

static int read_entire_file(const char *path, char **buffer, size_t *size) {
	FILE *file = fopen(path, "rb");
	long file_size;
	size_t bytes_read;

	if (file == NULL) {
		return -1;
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return -1;
	}

	file_size = ftell(file);
	if (file_size < 0) {
		fclose(file);
		return -1;
	}

	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return -1;
	}

	*buffer = (char *)malloc((size_t)file_size + 1U);
	if (*buffer == NULL) {
		fclose(file);
		errno = ENOMEM;
		return -1;
	}

	bytes_read = fread(*buffer, 1U, (size_t)file_size, file);
	if (bytes_read != (size_t)file_size) {
		free(*buffer);
		*buffer = NULL;
		fclose(file);
		return -1;
	}

	(*buffer)[bytes_read] = '\0';
	*size = bytes_read;
	fclose(file);
	return 0;
}

static int write_text_file(const char *path, const char *text, int append_mode) {
	FILE *file = fopen(path, append_mode ? "ab" : "wb");
	size_t text_length;

	if (file == NULL) {
		return -1;
	}

	text_length = strlen(text);
	if (fwrite(text, 1U, text_length, file) != text_length) {
		fclose(file);
		return -1;
	}

	if (fwrite("\n", 1U, 1U, file) != 1U) {
		fclose(file);
		return -1;
	}

	if (fflush(file) != 0) {
		fclose(file);
		return -1;
	}

	if (fclose(file) != 0) {
		return -1;
	}

	return 0;
}

static int copy_file(const char *source_path, const char *destination_path) {
	FILE *source_file = fopen(source_path, "rb");
	FILE *destination_file;
	unsigned char buffer[4096];
	size_t bytes_read;

	if (source_file == NULL) {
		return -1;
	}

	destination_file = fopen(destination_path, "wb");
	if (destination_file == NULL) {
		fclose(source_file);
		return -1;
	}

	while ((bytes_read = fread(buffer, 1U, sizeof(buffer), source_file)) > 0U) {
		if (fwrite(buffer, 1U, bytes_read, destination_file) != bytes_read) {
			fclose(source_file);
			fclose(destination_file);
			return -1;
		}
	}

	if (ferror(source_file) != 0) {
		fclose(source_file);
		fclose(destination_file);
		return -1;
	}

	if (fclose(source_file) != 0) {
		fclose(destination_file);
		return -1;
	}

	if (fclose(destination_file) != 0) {
		return -1;
	}

	return 0;
}

static int print_file_info(const char *path) {
	struct stat file_info;

	if (stat(path, &file_info) != 0) {
		return -1;
	}

	printf("Path: %s\n", path);
	printf("Size: %lld bytes\n", (long long)file_info.st_size);
	printf("Mode: %o\n", (unsigned int)(file_info.st_mode & 0777U));
	printf("Type: %s\n", S_ISDIR(file_info.st_mode) ? "directory" : "file");
	printf("Last modified: %lld\n", (long long)file_info.st_mtime);

	return 0;
}

static int touch_file(const char *path) {
	FILE *file = fopen(path, "ab");

	if (file == NULL) {
		return -1;
	}

	if (fclose(file) != 0) {
		return -1;
	}

	return 0;
}

static int delete_file(const char *path) {
	if (remove(path) != 0) {
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	const char *command;

	if (argc < 3) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	command = argv[1];

	if (strcmp(command, "info") == 0) {
		if (print_file_info(argv[2]) != 0) {
			print_error("info", argv[2]);
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	if (strcmp(command, "read") == 0) {
		char *contents = NULL;
		size_t file_size = 0;

		if (read_entire_file(argv[2], &contents, &file_size) != 0) {
			print_error("read", argv[2]);
			return EXIT_FAILURE;
		}

		printf("%s", contents);
		if (file_size > 0U && contents[file_size - 1U] != '\n') {
			printf("\n");
		}

		free(contents);
		return EXIT_SUCCESS;
	}

	if ((strcmp(command, "write") == 0) || (strcmp(command, "append") == 0)) {
		int append_mode;

		if (argc < 4) {
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}

		append_mode = (strcmp(command, "append") == 0);
		if (write_text_file(argv[2], argv[3], append_mode) != 0) {
			print_error(command, argv[2]);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	if (strcmp(command, "copy") == 0) {
		if (argc < 4) {
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}

		if (copy_file(argv[2], argv[3]) != 0) {
			print_error("copy", argv[2]);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	if (strcmp(command, "move") == 0) {
		if (argc < 4) {
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}

		if (rename(argv[2], argv[3]) != 0) {
			print_error("move", argv[2]);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	if (strcmp(command, "delete") == 0) {
		if (delete_file(argv[2]) != 0) {
			print_error("delete", argv[2]);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	if (strcmp(command, "touch") == 0) {
		if (touch_file(argv[2]) != 0) {
			print_error("touch", argv[2]);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	print_usage(argv[0]);
	return EXIT_FAILURE;
}
