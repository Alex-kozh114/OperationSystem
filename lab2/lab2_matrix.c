#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define REPEATS 5

typedef struct {
	double re;
	double im;
} Complex;

typedef struct {
	Complex *result;
	size_t row_begin;
	size_t row_end;
} ThreadArgs;

static size_t n;
static Complex *a, *b;

static void *work(void *_args)
{
	ThreadArgs *args = (ThreadArgs *)_args;

	for (size_t i = args->row_begin; i < args->row_end; ++i) {
		Complex *row = args->result + i * n;

		memset(row, 0, n * sizeof(Complex));

		for (size_t k = 0; k < n; ++k) {
			Complex x = a[i * n + k];
			const Complex *b_row = b + k * n;

			for (size_t j = 0; j < n; ++j) {
				row[j].re += x.re * b_row[j].re - x.im * b_row[j].im;
				row[j].im += x.re * b_row[j].im + x.im * b_row[j].re;
			}
		}
	}

	return NULL;
}

static void multiply_sequential(Complex *result)
{
	ThreadArgs args = {.result = result, .row_begin = 0, .row_end = n};

	work(&args);
}

static void multiply_parallel(Complex *result, size_t n_threads)
{
	pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
	ThreadArgs *thread_args = malloc(n_threads * sizeof(ThreadArgs));

	if (!threads || !thread_args) {
		fprintf(stderr, "Ошибка памяти малютки\n");
		exit(1);
	}

	for (size_t i = 0; i < n_threads; ++i) {
		thread_args[i] = (ThreadArgs){
			.result = result,
			.row_begin = n * i / n_threads,
			.row_end = n * (i + 1) / n_threads,
		};

		if (pthread_create(&threads[i], NULL, work, &thread_args[i]) != 0) {
			fprintf(stderr, "Ошибка: не удалось создать поток №%zu\n", i + 1);
			exit(1);
		}
	}

	for (size_t i = 0; i < n_threads; ++i) {
		pthread_join(threads[i], NULL);
	}

	free(thread_args);
	free(threads);
}

static double now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static void fill_random(Complex *m)
{
	for (size_t i = 0; i < n * n; ++i) {
		m[i].re = rand() / (double)RAND_MAX - 0.5;
		m[i].im = rand() / (double)RAND_MAX - 0.5;
	}
}

int main(int argc, char **argv)
{
	if (argc != 3 || atoi(argv[1]) < 1 || atoi(argv[2]) < 1) {
		fprintf(stderr, "Использование: %s РАЗМЕР_МАТРИЦЫ ЧИСЛО_ПОТОКОВ\n", argv[0]);
		return 1;
	}

	n = (size_t)atoi(argv[1]);
	size_t n_threads = (size_t)atoi(argv[2]);

	a = malloc(n * n * sizeof(Complex));
	b = malloc(n * n * sizeof(Complex));
	Complex *seq_result = malloc(n * n * sizeof(Complex));
	Complex *par_result = malloc(n * n * sizeof(Complex));

	if (!a || !b || !seq_result || !par_result) {
		fprintf(stderr, "Ошибка: не хватает памяти\n");
		return 1;
	}

	srand(42);
	fill_random(a);
	fill_random(b);

	printf("PID процесса: %d\n", (int)getpid());
	fflush(stdout);

	double seq_ms = 0, par_ms = 0;

	for (int r = 0; r < REPEATS; ++r) {
		double start = now_ms();
		multiply_sequential(seq_result);
		double elapsed = now_ms() - start;

		if (r == 0 || elapsed < seq_ms) {
			seq_ms = elapsed;
		}
	}

	for (int r = 0; r < REPEATS; ++r) {
		double start = now_ms();
		multiply_parallel(par_result, n_threads);
		double elapsed = now_ms() - start;

		if (r == 0 || elapsed < par_ms) {
			par_ms = elapsed;
		}
	}

	double speedup = seq_ms / par_ms;
	int same = memcmp(seq_result, par_result, n * n * sizeof(Complex)) == 0;

	printf("Размер матрицы: %zu\n", n);
	printf("Число потоков: %zu\n", n_threads);
	printf("Последовательная версия: %.3f мс\n", seq_ms);
	printf("Параллельная версия: %.3f мс\n", par_ms);
	printf("Ускорение: %.3f\n", speedup);
	printf("Эффективность: %.3f\n", speedup / n_threads);
	printf("Проверка результатов: %s\n", same ? "совпадают" : "НЕ совпадают");

	free(a);
	free(b);
	free(seq_result);
	free(par_result);
	return same ? 0 : 1;
}
