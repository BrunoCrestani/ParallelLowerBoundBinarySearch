#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

#include "chrono.c"

#define DEBUG 0
#define MAX_THREADS 64
#define LOOP_COUNT 1

#define TYPE LONG_LONG
#define element_TYPE long long

#define MAX_TOTAL_ELEMENTS (250 * 1000 * 1000) // ajustado para 250 milhões de elementos long long

pthread_t Thread[MAX_THREADS];
int my_thread_id[MAX_THREADS];
element_TYPE answersArray[MAX_THREADS];

int nThreads;       // número efetivo de threads
int nTotalElements; // número total de elementos

element_TYPE InputVector[MAX_TOTAL_ELEMENTS]; // vetor de entrada

pthread_barrier_t myBarrier;

int min(int a, int b)
{
   return (a < b) ? a : b;
}

element_TYPE plus(element_TYPE a, element_TYPE b)
{
   return a + b;
}

void *bSearchLowerBound(void *ptr)
{
   int myIndex = *((int *)ptr);
   int nElements = nTotalElements / nThreads;

   // Intervalo para cada thread
   int first = myIndex * nElements;
   int last = min((myIndex + 1) * nElements, nTotalElements) - 1;

#if DEBUG == 1
   printf("thread %d here! first=%d last=%d\n", myIndex, first, last);
#endif

   if (myIndex != 0)
      pthread_barrier_wait(&myBarrier);

   int low = first, high = last;
   int x = 10; // elemento que será procurado

   // Busca binária
   while (low < high)
   {
      int mid = (low + high) / 2;
      if (InputVector[mid] < x)
      {
         low = mid + 1;
      }
      else
      {
         high = mid;
      }
   }

   answersArray[myIndex] = low;
   pthread_barrier_wait(&myBarrier);
   return NULL;
}

int main(int argc, char *argv[])
{
   int i;
   chronometer_t parallelReductionTime;

   if (argc != 3)
   {
      printf("usage: %s <nTotalElements> <nThreads>\n", argv[0]);
      return 0;
   }
   else
   {
      nThreads = atoi(argv[2]);
      if (nThreads == 0 || nThreads > MAX_THREADS)
      {
         printf("Número de threads deve ser entre 1 e %d\n", MAX_THREADS);
         return 0;
      }
      nTotalElements = atoi(argv[1]);
      if (nTotalElements > MAX_TOTAL_ELEMENTS)
      {
         printf("Número máximo de elementos é %d\n", MAX_TOTAL_ELEMENTS);
         return 0;
      }
   }

   printf("Usando %d threads para processar %d elementos long long\n\n", nThreads, nTotalElements);

   // Inicializa o vetor de entrada
   for (int i = 0; i < nTotalElements; i++)
      InputVector[i] = 1;

   chrono_reset(&parallelReductionTime);

   pthread_barrier_init(&myBarrier, NULL, nThreads);

   // Criação das threads
   my_thread_id[0] = 0;
   for (i = 1; i < nThreads; i++)
   {
      my_thread_id[i] = i;
      pthread_create(&Thread[i], NULL, bSearchLowerBound, &my_thread_id[i]);
   }

   pthread_barrier_wait(&myBarrier);
   chrono_start(&parallelReductionTime);

   bSearchLowerBound(&my_thread_id[0]); // A thread 0 (main) faz o papel da thread principal

   // Procura pela posição global do elemento
   element_TYPE globalIndex = 0;
   for (int i = 0; i < nThreads; i++)
   {
      if (answersArray[i])
         globalIndex = answersArray[i];
   }

   // Medindo tempo após execução das threads
   chrono_stop(&parallelReductionTime);

   printf("globalIndex = %lld\n", globalIndex);

   for (i = 1; i < nThreads; i++)
      pthread_join(Thread[i], NULL);

   pthread_barrier_destroy(&myBarrier);

   chrono_reportTime(&parallelReductionTime, "parallelReductionTime");

   double total_time_in_seconds = (double)chrono_gettotal(&parallelReductionTime) / ((double)1000 * 1000 * 1000);
   printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);

   double OPS = (nTotalElements) / total_time_in_seconds;
   printf("Throughput: %lf OP/s\n", OPS);

   return 0;
}
