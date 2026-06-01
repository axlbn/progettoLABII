#ifndef GRAFO_H
#define GRAFO_H

#include <stdbool.h>
#include <pthread.h>

typedef struct arco {
    int u, v;
    int weight;
    bool msf;
    struct arco *next;
} arco;

typedef struct elemento {
    int id;
    int w;
    bool msf;
    struct elemento *next;
} elemento;

typedef struct {
    arco **gHash;
    elemento **vicini;
    int *cCon;
    int numCoCo;
    long costoMSF;

    int n_nodi;
    int n_archi;
    int hashSize;

    pthread_mutex_t *mut_gHash;
    int nmutex;

    bool *busy;
    pthread_mutex_t mut_busy;
    pthread_cond_t cond_busy;
} grafo;




#endif