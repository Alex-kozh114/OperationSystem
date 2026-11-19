#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "shared.h"

#define BUFFER_SIZE 2048

static shared_data_t *data = MAP_FAILED;
static sem_t *sem_data = SEM_FAILED;
static sem_t *sem_free = SEM_FAILED;

static long sum_line(const char *line, int line_len)
{
    long sum = 0;
    long num = 0;
    int in_number = 0;
    int negative = 0;

    for (int i = 0; i <= line_len; i++)
    {
        char c = (i < line_len) ? line[i] : ' ';

        if (c == '-')
        {
            negative = 1;
            in_number = 1;
        }
        else if (c >= '0' && c <= '9')
        {
            in_number = 1;
            num = num * 10 + (c - '0');
        }
        else if (in_number)
        {
            sum += negative ? -num : num;
            num = 0;
            negative = 0;
            in_number = 0;
        }
    }
    return sum;
}

static int send_message(const char *text, uint32_t length)
{
    if (sem_wait(sem_free) == -1) return -1;

    if (length != END_OF_DATA) memcpy(data->text, text, length);
    data->length = length;

    if (sem_post(sem_data) == -1) return -1;
    return 0;
}

static int send_sum(const char *line, int line_len)
{
    char output[64];
    int out_len = long_to_string(sum_line(line, line_len), output);
    output[out_len++] = '\n';
    return send_message(output, (uint32_t)out_len);
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        print_error("Usage: child <shm name> <sem data name> <sem free name>\n");
        return 1;
    }

    int shm_fd = shm_open(argv[1], O_RDWR, 0);
    if (shm_fd == -1)
    {
        print_error("Error: child cannot open shared memory\n");
        return 1;
    }
    data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if (data == MAP_FAILED)
    {
        print_error("Error: child cannot map shared memory\n");
        return 1;
    }

    sem_data = sem_open(argv[2], 0);
    sem_free = sem_open(argv[3], 0);
    if (sem_data == SEM_FAILED || sem_free == SEM_FAILED)
    {
        print_error("Error: child cannot open semaphores\n");
        return 1;
    }

    int exit_code = 0;
    char line[BUFFER_SIZE];
    int line_len = 0;
    char ch;
    ssize_t bytes;

    while ((bytes = read(STDIN_FILENO, &ch, 1)) > 0)
    {
        if (ch == '\n')
        {
            if (send_sum(line, line_len) == -1)
            {
                exit_code = 1;
                break;
            }
            line_len = 0;
        }
        else if (ch != '\r')
        {
            if (line_len < BUFFER_SIZE - 1)
            {
                line[line_len++] = ch;
            }
        }
    }
    if (bytes == -1)
    {
        print_error("Error: child failed to read input\n");
        exit_code = 1;
    }

    if (exit_code == 0 && line_len > 0 && send_sum(line, line_len) == -1)
    {
        exit_code = 1;
    }

    if (send_message(NULL, END_OF_DATA) == -1)
    {
        print_error("Error: child failed to send end marker\n");
        exit_code = 1;
    }

    sem_close(sem_data);
    sem_close(sem_free);
    munmap(data, SHM_SIZE);
    return exit_code;
}