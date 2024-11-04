#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdbool.h>

#include "chrono.c"

#define DEBUG 1

#define NTIMES 10// Numero de vezes que as buscas serao feitas
#define MAX_THREADS 8 // Numero maximo de threads
#define MAX_TOTAL_ELEMENTS (long long)16e6 // Numero de elementos máximos em Input
#define NQ (long long)20 // Numero de elementos em Q

pthread_t Thread[MAX_THREADS];
int Thread_id[MAX_THREADS];
pthread_barrier_t bsearch_barrier;

int nThreads;       // numero efetivo de threads
int nTotalElements; // numero total de elementos

// Vetor de input global, guarda NTIMES copias do input concatenadas
long long InputG[NTIMES * MAX_TOTAL_ELEMENTS];

// Vetor de busca global, guarda NTIMES copias do vetor de busca concatenadas
long long QG[NTIMES * NQ];

// Vetor de respostas global, guarda NTIMES copias do vetor de resposta concatenadas
int PosG[NTIMES * NQ];

typedef struct
{
    int thread_id;
    long long *x; // elemento buscado
    int *ans_ptr; // onde guardar a resposta
    long long *Input;
} thread_args_t;


int compare(const void *a, const void *b)
{
    const long long int_a = *(const long long *)a;
    const long long int_b = *(const long long *)b;

    if (int_a < int_b)
        return -1;
    else if (int_a > int_b)
        return 1;
    else
        return 0;
}

void *bsearch_lower_bound(void *ptr)
{
    thread_args_t *args = (thread_args_t *)ptr;
    int myIndex = args->thread_id;
    long long *Input = args->Input;

    while (true)
    {
        pthread_barrier_wait(&bsearch_barrier);
        int first = 0;
        int last = nTotalElements - 1;

        int myAnswer = last + 1;

        long long x = *args->x;
        int *ans_ptr = args->ans_ptr;

        while (first <= last)
        {
            int m = first + (last - first) / 2;

            if (Input[m] >= x)
            {
                myAnswer = m;
                last = m - 1;
            }
            else
            {
                first = m + 1;
            }
        }

        *ans_ptr = myAnswer;

        pthread_barrier_wait(&bsearch_barrier);

        if (myIndex == 0)
            return NULL;
    }
}

void parallel_multiple_bsearch(long long Input[], long long Q[], int Pos[])
{
    printf("Entered func\n");
    thread_args_t thread_args[nThreads];
    static int initialized = 0;

    if (!initialized) {
        if (pthread_barrier_init(&bsearch_barrier, NULL, nThreads) != 0)
        {
            perror("pthread_barrier_init");
            exit(EXIT_FAILURE);
        }

        // cria todas as outra threads trabalhadoras
        for (int i = 1; i < nThreads; i++)
        {
            thread_args[i].thread_id = i;
            thread_args[i].x = &Q[i];
            thread_args[i].ans_ptr = &Pos[i];
            thread_args[i].Input = Input;
            if (pthread_create(&Thread[i], NULL, bsearch_lower_bound, &thread_args[i]) != 0)
            {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        }
        thread_args[0].thread_id = 0;
        thread_args[0].x = &Q[0];
        thread_args[0].ans_ptr = &Pos[0];
        thread_args[0].Input = Input;
    }

    printf("initilizaed\n");
    for (int i = 0; i < NQ; i += nThreads)
    {
        for (int j = 0; j < nThreads && (i + j) < NQ; j++)
        {
            thread_args[j].x = &Q[i + j];
            thread_args[j].ans_ptr = &Pos[j + i]; 
            if (DEBUG)
                printf("Thread %d will look for %lld\n", j, *thread_args[j].x);
        }
        printf("Calling caller Func\n");
        // caller thread will be thread 0, and will start working on its chunk
        bsearch_lower_bound(&thread_args[0]);
        printf("All returned\n");

        if (DEBUG) {
            printf("Pos = ");
            for (int j = 0; j < NQ; j++) printf("%d ", Pos[j]);
            printf("\n\n");
        }
    }

    return;
}

int main(int argc, char *argv[])
{
    chronometer_t chrono_time;

    if (argc != 3)
    {
        printf("usage: %s <nTotalElements> <nThreads>\n", argv[0]);
        return 0;
    }
    else
    {
        nThreads = atoi(argv[2]);
        if (nThreads == 0)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n", argv[0]);
            printf("<nThreads> can't be 0\n");
            return 0;
        }
        if (nThreads > MAX_THREADS)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n", argv[0]);
            printf("<nThreads> must be less than %d\n", MAX_THREADS);
            return 0;
        }
        nTotalElements = atoi(argv[1]);
        if (nTotalElements > MAX_TOTAL_ELEMENTS)
        {
            printf("usage: %s <nTotalElements> <nThreads>\n", argv[0]);
            printf("<nTotalElements> must be up to %lld\n", MAX_TOTAL_ELEMENTS);
            return 0;
        }
    }

    printf("will use %d threads to run bsearch on %d elements\n\n", nThreads, nTotalElements);

    srand((unsigned int)1);

    // Popular InputVector com nTotalElements elementos aleatorios
    for (int i = 0; i < nTotalElements; i++)
    {
        for (int j = 0; j < NTIMES; j++) {
            InputG[i] = (long long)rand();
        }
    }

    // Ordenar
    qsort(InputG, nTotalElements, sizeof(long long), compare);

    // Criar copias concatenadas para evitar efeitos de cache
    for (int i = 1; i < NTIMES; i++) {
        for (int j = 0; j < nTotalElements; j++) {
            InputG[(i*nTotalElements) + j] = InputG[j];
        }
    }

    // Criar vetor de busca
    for (int i = 0; i < NQ; i++)
    {
        long long num = (long long)rand();
        for (int j = 0; j < NTIMES; j++) {
            QG[i + (j*NQ)] = num;
        }
    }



    if (DEBUG) {
        printf("Input = ");
        for (int i = 0; i < nTotalElements; i++) printf("%lld ", InputG[i]);

        printf("\n\n");


        printf("Search = ");
        for (int i = 0; i < NQ;i++) printf("%lld ", QG[i]);
        printf("\n\n");
    }


    chrono_reset(&chrono_time);
    chrono_start(&chrono_time);

    long long *Input, *Q; 
    int* Pos;
    long long start_position_InVec = 0;
    long long start_position_SearchVec = 0;
    long long start_position_Pos = 0;
    for (int i = 0; i < NTIMES; i++) {
        printf("Time = %d\n",i);

        // Minimizar efeitos da cache
        Input = &InputG[start_position_InVec];
        Q = &QG[start_position_SearchVec];
        Pos = &PosG[start_position_Pos];

        printf("calling");
        parallel_multiple_bsearch(Input, Q, Pos);
        printf("out");

        start_position_InVec += nTotalElements;
        start_position_SearchVec += NQ;
        start_position_Pos += NQ;
        printf("Ended this time\n\n");
    }

    // Measuring time after parallel_lowerBoundBinarySearch finished...
    chrono_stop(&chrono_time);

    // calcular e imprimir a VAZAO (numero de operacoes/s)
    double total_time_in_seconds = (double)chrono_gettotal(&chrono_time) /
                                   ((double)1000 * 1000 * 1000);
    printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);

    return 0;
}