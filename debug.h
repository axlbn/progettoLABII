#ifndef DEBUG_H
#define DEBUG_H
#define _GNU_SOURCE   
#include "grafo.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
void stampa_archi_tmp(arco *archi, int n_archi);
void stampa_debug_vicini(grafo *g);
void stampa_ghash(grafo *g);
void stampa_cCon(grafo *g);
#endif