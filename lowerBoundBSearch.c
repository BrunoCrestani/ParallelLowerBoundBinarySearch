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
#define MAX_TOTAL_ELEMENTS  (long long)250e6


pthread_t parallelReduce_Thread[ MAX_THREADS ];
int parallelReduce_thread_id[ MAX_THREADS ];
long long parallelReduce_partialSum[ MAX_THREADS ];   

int parallelReduce_nThreads;  // numero efetivo de threads
int parallelReduce_nTotalElements;  // numero total de elementos
               
               
long long InputVector[ MAX_TOTAL_ELEMENTS ];   // will NOT use malloc
                                     // for simplicity                              
long long *InVec = InputVector;
        
pthread_barrier_t parallelReduce_barrier;

int min( int a, int b )
{
   if( a < b )
      return a;
   else
      return b;
}


void *reducePartialSum(void *ptr)
{
    int myIndex = *((int *)ptr);
    int nElements = (parallelReduce_nTotalElements+(parallelReduce_nThreads-1))
                    / parallelReduce_nThreads;
        
    int first = myIndex * nElements;
    int last = min( (myIndex+1) * nElements, parallelReduce_nTotalElements ) - 1;

    printf("thread %d here! first=%d last=%d nElements=%d\n", 
                                  myIndex, first, last, nElements );
    
    while( true ) {
        pthread_barrier_wait( &parallelReduce_barrier );    
        
       long long myPartialSum = 0;

       for( int i=first; i<=last ; i++ )
           myPartialSum += InVec[i];

       parallelReduce_partialSum[ myIndex ] = myPartialSum;     
        
       pthread_barrier_wait( &parallelReduce_barrier );    
       if( myIndex == 0 )
          return NULL;           
    }
    
    // NEVER HERE!
    if( myIndex != 0 )
          pthread_exit( NULL );
          
    return NULL;      
}


long long parallel_reduceSum( long long InputVec[], int nTotalElements, int nThreads )
{

    static int initialized = 0;
    parallelReduce_nTotalElements = nTotalElements;
    parallelReduce_nThreads = nThreads;
    
    InVec = InputVec;
    
    if( ! initialized ) { 
       pthread_barrier_init( &parallelReduce_barrier, NULL, nThreads );
       // thread 0 will be the caller thread
    
       // cria todas as outra threds trabalhadoras
       parallelReduce_thread_id[0] = 0;
       for( int i=1; i < nThreads; i++ ) {
         parallelReduce_thread_id[i] = i;
         pthread_create( &parallelReduce_Thread[i], NULL, 
                      reducePartialSum, &parallelReduce_thread_id[i]);
       }

       initialized = 1;
    }

    
    // caller thread will be thread 0, and will start working on its chunk
    reducePartialSum( &parallelReduce_thread_id[0] ); 
        
    
    // a thread chamadora faz, entao, a reduçao da soma global
    long long globalSum = 0;
    for( int i=0; i<nThreads ; i++ ) {
        globalSum += parallelReduce_partialSum[i];
    }    
    
    return globalSum;
}

int main( int argc, char *argv[] )
{
    int i;
    int nThreads;
    int nTotalElements;
    
    chronometer_t parallelReductionTime;
    
    if( argc != 3 ) {
         printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] ); 
         return 0;
    } else {
         nThreads = atoi( argv[2] );
         if( nThreads == 0 ) {
              printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] );
              printf( "<nThreads> can't be 0\n" );
              return 0;
         }     
         if( nThreads > MAX_THREADS ) {  
              printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] );
              printf( "<nThreads> must be less than %d\n", MAX_THREADS );
              return 0;
         }     
         nTotalElements = atoi( argv[1] ); 
         if( nTotalElements > MAX_TOTAL_ELEMENTS ) {  
              printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] );
              printf( "<nTotalElements> must be up to %d\n", MAX_TOTAL_ELEMENTS );
              return 0;
         }     
    }
    
    
    #if TYPE == FLOAT
        printf( "will use %d threads to reduce %d total FLOAT elements\n\n", nThreads, nTotalElements );
    #elif TYPE == DOUBLE   
        printf( "will use %d threads to reduce %d total DOUBLE elements\n\n", nThreads, nTotalElements );
    #endif   
    
    for( int i=0; i<MAX_TOTAL_ELEMENTS ; i++ )
        InputVector[i] = 1LL;
       
    chrono_reset( &parallelReductionTime );
    chrono_start( &parallelReductionTime );

      // call it N times
      #define NTIMES 1000
      printf( "will call parallel_reduceSum %d times\n", NTIMES );
            
      long long globalSum;
      int start_position = 0;
      InVec = &InputVector[start_position];

      for( int i=0; i<NTIMES ; i++ ) {
           
           globalSum = parallel_reduceSum( InVec,
                                           nTotalElements, nThreads );
           // garante que na proxima rodada todos os elementos estarão FORA do cache
           start_position += nTotalElements;
           // volta ao inicio do vetor 
           //   SE nao cabem nTotalElements a partir de start_position
           if( (start_position + nTotalElements) > MAX_TOTAL_ELEMENTS )
              start_position = 0;
           InVec = &InputVector[start_position];  

      }     
                                           
    // Measuring time after parallel_reduceSum finished...
    chrono_stop( &parallelReductionTime );

   printf( "globalSum = %ld\n", globalSum );
    
    chrono_reportTime( &parallelReductionTime, "parallelReductionTime" );
    
    // calcular e imprimir a VAZAO (numero de operacoes/s)
    double total_time_in_seconds = (double) chrono_gettotal( &parallelReductionTime ) /
                                      ((double)1000*1000*1000);
    printf( "total_time_in_seconds: %lf s\n", total_time_in_seconds );
                                  
    double OPS = ((double)nTotalElements*NTIMES)/total_time_in_seconds;
    printf( "Throughput: %lf OP/s\n", OPS );
    
    return 0;
}
