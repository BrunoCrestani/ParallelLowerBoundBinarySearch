#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

#include "chrono.c"

// Lower bound binary search paralela com Pthreads
// por Bruno Crestani e Rafael Marques

// será feita a busca binaira com lower bound usando nThreads (definida via linha de comando)

// Entrada para a operação bSearchLowerBound será:
//   um vetor de nTotalElements elementos
//   (nTotalElements obtido da linha de comando)
// Para esse teste o vetor NÂO será lido,
//   - o vetor será preenchido sequencialmente pela função main
//  um elemento x que buscara a posicao de insercao no vetor

// Cada thread irá fazer a busca da posicao em que um elemento x se encaixaria no vetor
// A última thread poderá reduzir menos que nTotalElements / nThreads elementos,
//   quando n não for múltiplo de nThreads

// Cada thread irá retornar seu resultado no vetor answersArray
// Todas as threads sincronizam com uma barreira
// Após sincronização com barreira:
//   - a thread 0 (main) irá procurar e retornar pelo posição ideal inferior para o elemento
// solicitado
//   - as threads diferentes de 0 irão simplesmente terminar

// o programa deve calcular e imprimir o tempo da busca binaria lower bound
// // em duas versoes:
// versão A) NAO incluindo o tempo de criação das threads
// versão B) INCLUINDO  o tempo de criação das threads

// rodar o programa 30 vezes obtendo o tempo MÍNIMO e o MÉDIO
//  nas duas versoes

// calcular a aceleração para 2, 3, 4, 5, 6, 7, e 8 threads
// colocando em uma planilha (tabela)

/*
A quantidade de elementos para as experiencias:
Vamos especificar aqui em breve.
   Tipicamente teremos:
      - Vetor Input de
      1Milhao, 2Milhoees, 4Milhoes, 8Milhoes e 16Milhoes de elementos long long

      - Vetor de pesquisa Q, e vetor Pos  (parte B do trabalho)
      podemos pensar em 100mil elementos nesse vetor (em principio)

      - (v1.2) o mesmo vetor de pesquisa DEVE ser usado no loop de medicao da parte A
      tambem, ver observacoes abaixo.
 */

#define DEBUG 0
// #define DEBUG 1
#define MAX_THREADS 64
#define LOOP_COUNT 1

#define FLOAT 1
#define DOUBLE 2

#define TYPE FLOAT // CHOICE OF FLOAT or DOUBLE

#if TYPE == FLOAT
#define element_TYPE float
#elif TYPE == DOUBLE
#define element_TYPE double
#endif

#if TYPE == FLOAT
#define MAX_TOTAL_ELEMENTS (500 * 1000 * 1000) // if each float takes 4 bytes
                                               // will have a maximum 500 million FLOAT elements
                                               // which fits in 2 GB of RAM
#elif TYPE == DOUBLE
#define MAX_TOTAL_ELEMENTS (250 * 1000 * 1000) // if each float takes 4 bytes
// will have a maximum 250 million DOUBLE elements
// which fits in 2 GB of RAM
#endif

pthread_t Thread[MAX_THREADS];
int my_thread_id[MAX_THREADS];
element_TYPE answersArray[MAX_THREADS];

int nThreads;       // numero efetivo de threads
                    // obtido da linha de comando
int nTotalElements; // numero total de elementos
                    // obtido da linha de comando

float InputVector[MAX_TOTAL_ELEMENTS]; // will NOT use malloc
                                       // for simplicity

pthread_barrier_t myBarrier;

int min(int a, int b)
{
   if (a < b)
      return a;
   else
      return b;
}

element_TYPE plus(element_TYPE a, element_TYPE b)
{
   return a + b;
}

void *bSearchLowerBound(void *ptr, int x)
{
   int myIndex = *((int *)ptr);
   int nElements = nTotalElements / nThreads;

   // assume que temos pelo menos 1 elemento por thread
   int first = myIndex * nElements;
   int last = min((myIndex + 1) * nElements, nTotalElements) - 1;

#if DEBUG == 1
   printf("thread %d here! first=%d last=%d\n", myIndex, first, last);
#endif

   if (myIndex != 0)
      pthread_barrier_wait(&myBarrier);

   int low = first, high = last;

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
         printf("<nTotalElements> must be up to %d\n", MAX_TOTAL_ELEMENTS);
         return 0;
      }
   }

#if TYPE == FLOAT
   printf("will use %d threads to reduce %d total FLOAT elements\n\n", nThreads, nTotalElements);
#elif TYPE == DOUBLE
   printf("will use %d threads to reduce %d total DOUBLE elements\n\n", nThreads, nTotalElements);
#endif

   // inicializaçoes
   // initialize InputVector
   for (int i = 0; i < nTotalElements; i++)
      InputVector[i] = (element_TYPE)1;

   chrono_reset(&parallelReductionTime);

   pthread_barrier_init(&myBarrier, NULL, nThreads);

   // thread 0 será main

   my_thread_id[0] = 0;
   for (i = 1; i < nThreads; i++)
   {
      my_thread_id[i] = i;
      pthread_create(&Thread[i], NULL, bSearchLowerBound, &my_thread_id[i]);
   }

   // Medindo tempo SEM criacao das threads
   pthread_barrier_wait(&myBarrier);
   chrono_start(&parallelReductionTime);

   bSearchLowerBound(&my_thread_id[0], 10); // main faz papel da thread 0
   // chegando aqui todas as threads sincronizaram, até a 0 (main)

   // main faz a reducao da soma global
   element_TYPE globalIndex = 0;
   for (int i = 0; i < nThreads; i++)
   {
      if (answersArray[i])
         globalIndex = answersArray[i];
   }

   // Medindo tempo APOS FIM das threads
   chrono_stop(&parallelReductionTime);

// main imprime o resultado global
#if TYPE == FLOAT
   printf("globalIndex = %f\n", globalIndex);
#elif TYPE == DOUBLE
   printf("globalIndex = %lf\n", globalIndex);
#endif

   for (i = 1; i < nThreads; i++)
      pthread_join(Thread[i], NULL); // isso é necessário ?

   pthread_barrier_destroy(&myBarrier);

   chrono_reportTime(&parallelReductionTime, "parallelReductionTime");

   // calcular e imprimir a VAZAO (numero de operacoes/s)
   double total_time_in_seconds = (double)chrono_gettotal(&parallelReductionTime) /
                                  ((double)1000 * 1000 * 1000);
   printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);

   double OPS = (nTotalElements) / total_time_in_seconds;
   printf("Throughput: %lf OP/s\n", OPS);

   return 0;
}
