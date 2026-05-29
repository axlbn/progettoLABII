#include "debug.h"

void stampa_debug_vicini(grafo *g)
{
    if (g == NULL)
    {
        fprintf(stderr, "DEBUG vicini: grafo NULL\n");
        return;
    }

    if (g->vicini == NULL)
    {
        fprintf(stderr, "DEBUG vicini: g->vicini NULL\n");
        return;
    }

    fprintf(stderr, "\nDEBUG LISTE DI ADIACENZA\n");
    fprintf(stderr, "n_nodi: %d\n", g->n_nodi);

    for (int i = 0; i < g->n_nodi; i++)
    {
        fprintf(stderr, "vicini[%d]:", i);

        elemento *curr = g->vicini[i];

        if (curr == NULL)
        {
            fprintf(stderr, " NULL");
        }

        while (curr != NULL)
        {
            fprintf(stderr,
                    " -> (id:%d, w:%d, msf:%s)",
                    curr->id,
                    curr->w,
                    curr->msf ? "true" : "false");

            curr = curr->next;
        }

        fprintf(stderr, "\n");
    }

    fprintf(stderr, "FINE DEBUG LISTE DI ADIACENZA\n\n");
}

void stampa_cCon(grafo *g)
{
    printf("\n===== ARRAY cCon =====\n");

    for (int i = 0; i < g->n_nodi; i++) {
        printf("Nodo %d -> parent %d\n", i, g->cCon[i]);
    }

    printf("======================\n");
}
void stampa_archi_tmp(arco *archi, int n_archi)
{
    for (int i = 0; i < n_archi; i++)
    {
        fprintf(stderr,
                "%d)\tu:%d\tv:%d\tw:%d\n",
                i,
                archi[i].u,
                archi[i].v,
                archi[i].weight);
    }
}
void stampa_ghash(grafo *g)
{
    printf("\n===== CONTENUTO HASH TABLE =====\n");

    for (int i = 0; i < g->hashSize; i++) {

        printf("[%d] -> ", i);

        arco *corrente = g->gHash[i];

        if (corrente == NULL) {
            printf("NULL");
        }

        while (corrente != NULL) {

            printf("(u=%d, v=%d, w=%d)",
                   corrente->u,
                   corrente->v,
                   corrente->weight);

            if (corrente->msf)
                printf("[MSF]");

            printf(" -> ");

            corrente = corrente->next;
        }

        printf("NULL\n");
    }

    printf("================================\n");
}