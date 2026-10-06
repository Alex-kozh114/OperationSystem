#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

void print_error(const char *msg) {
    if (!msg) return;
    int len = 0;
    while (msg[len] != '\0') {
        len++;
    }
    write(STDERR_FILENO, msg, len);
}

int main(int argc, char **argv) {
    char filename[256];
    int filename_len = 0;

    if (argc >= 2) {
        while (argv[1][filename_len] != '\0' && filename_len < 255) {
            filename[filename_len] = argv[1][filename_len];
            filename_len++;
        }
        filename[filename_len] = '\0';
    } else {
        const char msg[] = "Enter filename: ";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);

        char ch;
        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == '\n' || ch == '\r') break;
            if (filename_len < 255) {
                filename[filename_len++] = ch;
            }
        }
        filename[filename_len] = '\0';
    }

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        print_error("Error: cannot open file\n");
        return 1;
    }

    int pipe_fd[2]; 
    if (pipe(pipe_fd) == -1) {
        print_error("Error: cannot create pipe\n");
        close(file_fd);
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        print_error("Error: fork failed\n");
        close(file_fd);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 1;
    }

    if (pid == 0) {
        
        if (dup2(file_fd, STDIN_FILENO) == -1 || 
            dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            print_error("Error: dup2 failed in child\n");
            _exit(1);
        }

        close(file_fd);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        char *const args[] = {"child", NULL};
        execv("./child", args);

        print_error("Error: failed to exec child executable\n");
        _exit(1);
    } else {
        
        close(file_fd);
        close(pipe_fd[1]);  

        char buf[BUFFER_SIZE];
        ssize_t bytes_read;

        while ((bytes_read = read(pipe_fd[0], buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, bytes_read);
        }
        if (bytes_read == -1) {
            print_error("Error: failed to read from pipe\n");
        }

        close(pipe_fd[0]);

        wait(NULL);
    }

    return 0;
}