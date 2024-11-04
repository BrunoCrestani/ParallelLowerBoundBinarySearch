#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chrono.c"

#define DEBUG 0
#define MAX_THREADS 64
#define LOOP_COUNT 1
#define MAX_TOTAL_ELEMENTS (long long)250e6
#define TOTAL_SEARCHABLE_ELEMENTS 4

pthread_t Thread[MAX_THREADS];
int thread_id[MAX_THREADS];

int nThreads;       // numero efetivo de threads
int nTotalElements; // numero total de elementos
long long answers[MAX_THREADS];

long long InputVector[MAX_TOTAL_ELEMENTS];
long long *InVec = InputVector;

long long SearchVector[TOTAL_SEARCHABLE_ELEMENTS] = {12, 200, 7, -1};
long long *SearchVec = SearchVector;

// Qual elemento de searchVec está sendo buscado
int searched_index;

pthread_barrier_t bsearch_barrier;

void *bsearch_lower_bound(void *ptr)
{
    int myIndex = *(int*)ptr;

    while (true)
    {
        // cada vez que sair dessa barreira vai estar procurando um numero novo
        pthread_barrier_wait(&bsearch_barrier);

        int first = myIndex * nTotalElements;
        int last = (myIndex+1) * nTotalElements - 1;

        printf("thread %d here! first=%d last=%d\n",
               myIndex, first, last);

        int myAnswer = last + 1;
        long long x = searchVec[searched_index];

        while (first <= last)
        {
            int m = first + (last - first) / 2;

            if (InVec[m] >= x)
            {
                myAnswer = m;
                last = m - 1;
            }
            else
            {
                first = m + 1;
            }
        }

        answers[myIndex] = myAnswer;

        pthread_barrier_wait(&bsearch_barrier);
        if (myIndex == 0)
            return NULL;
    }

    // NEVER HERE!
    if (myIndex != 0)
        pthread_exit(NULL);

    return NULL;
}

void parallel_bsearch(long long x, int nThreads)
{
    static int initialized = 0;
    thread_args_t thread_args[nThreads];

    if (!initialized)
    {
        pthread_barrier_init(&bsearch_barrier, NULL, nThreads);
        // thread 0 will be the caller thread

        // cria todas as outra threds trabalhadoras
        for (int i = 1; i < nThreads; i++)
        {
            thread_args[i].thread_id = i;
            thread_args[i].searchVec = searchVec;
            pthread_create(&Thread[i], NULL,
                           bsearch_lower_bound, &x);
        }

        initialized = 1;
    }

    thread_id[0] = 0;

    // caller thread will be thread 0, and will start working on its chunk
    // A hora que isso é chamado, comeca a busca do prox elemento do vetor de busca
    bsearch_lower_bound(&thread_id[0]);

    // DAR UM JEITO DE JUNTAR OS RESULTADOS

    return;
}

int main(int argc, char *argv[])
{
    int i;
    chronometer_t time;

    if (argc != 3)
    {
        printf("usage: %s <nTotalElements> <nThreads>\n",
               argv[0]);
        return 0;
    }
    else
    {
        nThreads = atoi(argv[2]);
        if (nThreads == 0)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n",
                   argv[0]);
            printf("<nThreads> can't be 0\n");
            return 0;
        }
        if (nThreads > MAX_THREADS)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n",
                   argv[0]);
            printf("<nThreads> must be less than %d\n", MAX_THREADS);
            return 0;
        }
        nTotalElements = atoi(argv[1]);
        if (nTotalElements > MAX_TOTAL_ELEMENTS)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n",
                   argv[0]);
            printf("<nTotalElements> must be up to %lld\n", MAX_TOTAL_ELEMENTS);
            return 0;
        }
    }

    printf("will use %d threads to run bsearch on %d elements\n\n", nThreads, nTotalElements);

    for (int i = 0; i < MAX_TOTAL_ELEMENTS; i++)
        InputVector[i] = (long long)i;

    chrono_reset(&time);
    chrono_start(&time);

    // FAZER ISSO VARIAS VEZES, CRIAR COPIAS A CADA VEZ
    for (int i = 0; i < tamSerched; i++)
        parallel_bsearch(serach) 
    
    // Measuring time after finished...
    chrono_stop(&time);

    chrono_reportTime(&time, "time");

    // calcular e imprimir a VAZAO (numero de operacoes/s)
    double total_time_in_seconds = (double)chrono_gettotal(&time) /
                                   ((double)1000 * 1000 * 1000);
    printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);

    double OPS = ((double)nTotalElements * NTIMES) / total_time_in_seconds;
    printf("Throughput: %lf OP/s\n", OPS);

    return 0;
}
