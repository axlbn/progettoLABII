#ifndef GRAFO_H
#define GRAFO_H

#include <stdbool.h>

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
} grafo;

#endif