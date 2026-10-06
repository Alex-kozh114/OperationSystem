#include <unistd.h>

#define BUFFER_SIZE 2048

void print_error(const char *msg)
{
    if (!msg) return;
    int len = 0;
    while (msg[len] != '\0') len++;
    write(STDERR_FILENO, msg, len);
}

int long_to_string(long value, char *buffer)
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

    unsigned long temp = (value < 0) ? -value : value;
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
        buffer[i] = (temp % 10) + '0';
        temp /= 10;
    }

    buffer[len] = '\0';
    return len;
}

int main(void)
{
    char line[BUFFER_SIZE];
    int line_len = 0;
    char ch;

    while (read(STDIN_FILENO, &ch, 1) > 0)
    {
        if (ch == '\n')
        {
            line[line_len] = '\0';
            long sum = 0;
            long num = 0;
            int in_number = 0;
            int negative = 0;

            for (int i = 0; i <= line_len; i++)
            {
                // Виртуальный пробел на i == line_len сбрасывает последнее число
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
                else
                {
                    if (in_number)
                    {
                        sum += negative ? -num : num;
                        num = 0;
                        negative = 0;
                        in_number = 0;
                    }
                }
            }

            char output[64];
            int out_len = long_to_string(sum, output);
            output[out_len++] = '\n';
            write(STDOUT_FILENO, output, out_len);
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
    return 0;
}