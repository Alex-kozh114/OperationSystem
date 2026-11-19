#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include <unistd.h>

#define SHM_SIZE 4096
#define NAME_SIZE 64
#define END_OF_DATA UINT32_MAX

typedef struct
{
    uint32_t length;
    char text[SHM_SIZE - sizeof(uint32_t)];
} shared_data_t;

static void print_error(const char *msg)
{
    if (!msg) return;
    int len = 0;
    while (msg[len] != '\0') len++;
    write(STDERR_FILENO, msg, len);
}

static int long_to_string(long value, char *buffer)
{
    if (buffer == NULL)
    {
        print_error("Error: Buffer is NULL\n");
        return 0;
    }
    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }
    unsigned long temp = (value < 0) ? 0UL - (unsigned long)value : (unsigned long)value;
    int len = 0;

    if (value < 0)
    {
        buffer[0] = '-';
        len++;
    }
    unsigned long t = temp;
    while (t > 0)
    {
        t /= 10;
        len++;
    }

    int start_index = (value < 0) ? 1 : 0;
    for (int i = len - 1; i >= start_index; i--)
    {
        buffer[i] = (char)(temp % 10) + '0';
        temp /= 10;
    }

    buffer[len] = '\0';
    return len;
}

#endif