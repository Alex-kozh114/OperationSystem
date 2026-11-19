#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shared.h"

static char shm_name[NAME_SIZE];
static char sem_data_name[NAME_SIZE];
static char sem_free_name[NAME_SIZE];

static shared_data_t *data = MAP_FAILED;
static sem_t *sem_data = SEM_FAILED;
static sem_t *sem_free = SEM_FAILED;
static int shm_created = 0;

static void build_name(char *dst, const char *prefix, long id)
{
    int n = 0;
    while (prefix[n] != '\0')
    {
        dst[n] = prefix[n];
        n++;
    }
    long_to_string(id, dst + n);
}

static void cleanup(void)
{
    if (sem_data != SEM_FAILED)
    {
        sem_close(sem_data);
        sem_unlink(sem_data_name);
    }
    if (sem_free != SEM_FAILED)
    {
        sem_close(sem_free);
        sem_unlink(sem_free_name);
    }
    if (data != MAP_FAILED)
    {
        munmap(data, SHM_SIZE);
    }
    if (shm_created)
    {
        shm_unlink(shm_name);
    }
}

int main(int argc, char **argv)
{
    char filename[256];
    int filename_len = 0;
    if (argc >= 2)
    {
        while (argv[1][filename_len] != '\0' && filename_len < 255)
        {
            filename[filename_len] = argv[1][filename_len];
            filename_len++;
        }
        filename[filename_len] = '\0';
    }
    else
    {
        const char msg[] = "Enter filename: ";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);

        char ch;
        while (read(STDIN_FILENO, &ch, 1) > 0)
        {
            if (ch == '\n' || ch == '\r') break;
            if (filename_len < 255)
            {
                filename[filename_len++] = ch;
            }
        }
        filename[filename_len] = '\0';
    }

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1)
    {
        print_error("Error: cannot open file\n");
        return 1;
    }

    long id = (long)getpid();
    build_name(shm_name, "/lab3-shm-", id);
    build_name(sem_data_name, "/lab3-sem-data-", id);
    build_name(sem_free_name, "/lab3-sem-free-", id);

    shm_unlink(shm_name);
    sem_unlink(sem_data_name);
    sem_unlink(sem_free_name);

    int shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (shm_fd == -1)
    {
        print_error("Error: cannot create shared memory\n");
        close(file_fd);
        return 1;
    }
    shm_created = 1;

    if (ftruncate(shm_fd, SHM_SIZE) == -1)
    {
        print_error("Error: cannot resize shared memory\n");
        close(shm_fd);
        close(file_fd);
        cleanup();
        return 1;
    }

    data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (data == MAP_FAILED)
    {
        print_error("Error: cannot map shared memory\n");
        close(file_fd);
        cleanup();
        return 1;
    }

    sem_data = sem_open(sem_data_name, O_CREAT | O_EXCL, 0600, 0);
    if (sem_data == SEM_FAILED)
    {
        print_error("Error: cannot create semaphore\n");
        close(file_fd);
        cleanup();
        return 1;
    }
    sem_free = sem_open(sem_free_name, O_CREAT | O_EXCL, 0600, 1);
    if (sem_free == SEM_FAILED)
    {
        print_error("Error: cannot create semaphore\n");
        close(file_fd);
        cleanup();
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        print_error("Error: fork failed\n");
        close(file_fd);
        cleanup();
        return 1;
    }

    if (pid == 0)
    {
        if (dup2(file_fd, STDIN_FILENO) == -1)
        {
            print_error("Error: dup2 failed in child\n");
        }
        else
        {
            close(file_fd);
            char *const args[] = {"child", shm_name, sem_data_name, sem_free_name, NULL};
            execv("./child", args);
            print_error("Error: failed to exec child executable\n");
        }
        data->length = END_OF_DATA;
        sem_post(sem_data);
        _exit(1);
    }

    close(file_fd);

    int exit_code = 0;
    while (1)
    {
        if (sem_wait(sem_data) == -1)
        {
            print_error("Error: sem_wait failed\n");
            exit_code = 1;
            break;
        }

        uint32_t length = data->length;
        if (length == END_OF_DATA) break;

        if (length > sizeof(data->text))
        {
            print_error("Error: corrupted message in shared memory\n");
            exit_code = 1;
            break;
        }

        write(STDOUT_FILENO, data->text, length);

        if (sem_post(sem_free) == -1)
        {
            print_error("Error: sem_post failed\n");
            exit_code = 1;
            break;
        }
    }

    if (exit_code != 0) kill(pid, SIGTERM);

    int status = 0;
    if (waitpid(pid, &status, 0) == -1)
    {
        print_error("Error: waitpid failed\n");
        exit_code = 1;
    }
    else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        print_error("Error: child finished with an error\n");
        exit_code = 1;
    }

    cleanup();
    return exit_code;
}