#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "grafo.h"
#include "xerrori.h"

#include <limits.h>

typedef struct
{
    char op; // + oppure -
    int u;
    int v;
    int w;
} operazione;

#define BUFFER_SIZE 1000

typedef struct
{
    operazione buffer[BUFFER_SIZE];

    int size;
    int p_index;
    int c_index;
    int count;

    bool fine_lettura;

    pthread_mutex_t mutex;

    pthread_cond_t non_pieno;
    pthread_cond_t non_vuoto;

} buffer_condiviso;

typedef struct
{
    grafo *g;
    buffer_condiviso *buf;
    pthread_mutex_t *mut_stampa;
} dati_thread;

int calcola_hash(int u, int v, int hashSize)
{
    unsigned long hash = (unsigned long)u * 31 + (unsigned long)v * 17;
    return (int)(hash % hashSize);
}

// utils di sincronizzazione
void lock_ghash(grafo *g, int u, int v)
{
    int hash = calcola_hash(u, v, g->hashSize);
    xpthread_mutex_lock(&g->mut_gHash[hash % g->nmutex], __LINE__, __FILE__);
}
void unlock_ghash(grafo *g, int u, int v)
{
    int hash = calcola_hash(u, v, g->hashSize);
    xpthread_mutex_unlock(&g->mut_gHash[hash % g->nmutex], __LINE__, __FILE__);
}
void lock_componenti(grafo *g, int u, int v, int *cu, int *cv)
{
    xpthread_mutex_lock(&g->mut_busy, __LINE__, __FILE__);
    *cu = g->cCon[u];
    *cv = g->cCon[v];
    while (g->busy[*cu] || g->busy[*cv])
    {
        xpthread_cond_wait(&g->cond_busy, &g->mut_busy, __LINE__, __FILE__);
        *cu = g->cCon[u];
        *cv = g->cCon[v];
    }
    g->busy[*cu] = true;
    g->busy[*cv] = true;
    xpthread_mutex_unlock(&g->mut_busy, __LINE__, __FILE__);
}
void unlock_componenti(grafo *g, int cu, int cv)
{
    xpthread_mutex_lock(&g->mut_busy, __LINE__, __FILE__);
    g->busy[cu] = false;
    g->busy[cv] = false;
    xpthread_cond_broadcast(&g->cond_busy, __LINE__, __FILE__);
    xpthread_mutex_unlock(&g->mut_busy, __LINE__, __FILE__);
}

arco *parser(const char *filename, int *n_archi, int *n_nodi)
{
    FILE *f = xfopen(filename, "r", __LINE__, __FILE__);
    int nread;
    char *line = NULL;
    size_t len = 0;
    bool header_trovato = false;

    while ((nread = getline(&line, &len, f)) != -1)
    {
        if (line[0] == 'c')
        {
            continue;
        }

        if (line[0] == 'p')
        {
            if (sscanf(line, "p sp %d %d", n_nodi, n_archi) == 2 && *n_nodi >= 0 && *n_archi >= 0)
            {
                header_trovato = true;
                break;
            }
        }
    }
    if (!header_trovato)
    {
        xtermina("Errore:intestazione file non trovata", __LINE__, __FILE__);
    }
    *(n_nodi) += 1;
    arco *archi = malloc((*n_archi) * sizeof(arco));
    if (archi == NULL)
    {
        xtermina("Errore:malloc() fallita", __LINE__, __FILE__);
    }
    int i = 0;
    while ((nread = getline(&line, &len, f)) != -1)
    {
        if (line[0] == 'c')
        {
            continue;
        }
        if (line[0] == 'a')
        {
            int u = -1, v = -1;
            int w = 0;
            int n_read;
            if ((n_read = sscanf(line, "a %d %d %d", &u, &v, &w)) == 3 && u >= 0 && v >= 0 && u < v && u < *n_nodi && v < *n_nodi)
            {
                // trovato arco valido -> aggiungo ad insieme archi
                if (i < *n_archi)
                {
                    archi[i].u = u;
                    archi[i].v = v;
                    archi[i].weight = w;
                    archi[i].msf = false;
                    archi[i].next = NULL;
                    i++;
                }
                else
                {
                    // ci sono piu archi rispetto a quelli dichiarati nell'intestazione
                    fprintf(stderr, "Errore: file di input %s possiede troppi archi rispetto a quelli dichiarti nell'intestazione (%d)<(%d)", filename, *n_archi, i);
                    exit(1);
                }
            }
            else
            {
                // linea malformata/non valida
                fprintf(stderr, "Errore: file di input malformato o valori non validi\t n_read:%d\tu:%d\tv:%d\tw:%d", n_read, u, v, w);
                exit(1);
            }
        }
    }
    if (i != *n_archi)
    {
        fprintf(stderr,
                "Errore: archi letti %d, archi dichiarati %d\n",
                i, *n_archi);
        exit(1);
    }
    free(line);
    fclose(f);
    return archi;
}

void vicini_inserisci_ordinato(elemento **lista, int id, int w, bool msf)
{
    elemento *nuovo = calloc(1, sizeof(elemento));
    if (nuovo == NULL)
    {
        xtermina("calloc elemento fallita", __LINE__, __FILE__);
    }

    nuovo->id = id;
    nuovo->w = w;
    nuovo->msf = msf;
    nuovo->next = NULL;

    elemento *prec = NULL;
    elemento *curr = *lista;

    while (curr != NULL && curr->id < id)
    {
        prec = curr;
        curr = curr->next;
    }

    if (prec == NULL)
    {
        nuovo->next = *lista;
        *lista = nuovo;
    }
    else
    {
        nuovo->next = curr;
        prec->next = nuovo;
    }
}
void genera_vicini(grafo *g, arco *archi)
{
    g->vicini = calloc(g->n_nodi, sizeof(elemento *));
    if (g->vicini == NULL)
    {
        xtermina("calloc elemento fallita", __LINE__, __FILE__);
    }
    for (int i = 0; i < g->n_archi; i++)
    {
        int u = archi[i].u;
        int v = archi[i].v;
        int w = archi[i].weight;
        vicini_inserisci_ordinato(&g->vicini[u], v, w, false);
        vicini_inserisci_ordinato(&g->vicini[v], u, w, false);
    }
}

arco *ghash_get_arco(grafo *g, int u, int v) // return arco se trovato NULL altrimenti
{
    int hash = calcola_hash(u, v, g->hashSize);
    arco *curr = g->gHash[hash];
    while (curr != NULL)
    {
        if (curr->u == u && curr->v == v)
        {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}
bool ghash_inserisci(grafo *g, int u, int v, int w, bool msf) // returna false se è l'arco è gia presente
{
    int hash = calcola_hash(u, v, g->hashSize);
    arco *nuovo = malloc(sizeof(arco));

    if (nuovo == NULL)
    {
        xtermina("Errore allocazione arco", __LINE__, __FILE__);
    }

    nuovo->u = u;
    nuovo->v = v;
    nuovo->weight = w;
    nuovo->msf = msf;
    if (ghash_get_arco(g, u, v) != NULL)
    {
        free(nuovo);
        return false;
    }
    nuovo->next = g->gHash[hash];
    g->gHash[hash] = nuovo;
    return true;
}
void genera_ghash(grafo *g, arco *archi)
{
    g->gHash = calloc(g->hashSize, sizeof(arco *)); // inizializza a NULL

    if (g->gHash == NULL)
    {
        xtermina("Errore allocazione ghash", __LINE__, __FILE__);
    }

    for (int i = 0; i < g->n_archi; i++)
    {
        ghash_inserisci(g, archi[i].u, archi[i].v, archi[i].weight, false);
    }
}

int find(grafo *g, int x)
{
    if (g->cCon[x] != x)
        g->cCon[x] = find(g, g->cCon[x]);

    return g->cCon[x];
}
bool union_set(grafo *g, int *rank, int *min_root, int u, int v)
{
    int u_parent = find(g, u);
    int v_parent = find(g, v);

    if (u_parent == v_parent)
        return false;

    if (rank[u_parent] < rank[v_parent])
    {
        g->cCon[u_parent] = v_parent;

        if (min_root[u_parent] < min_root[v_parent])
            min_root[v_parent] = min_root[u_parent];
    }
    else if (rank[u_parent] > rank[v_parent])
    {
        g->cCon[v_parent] = u_parent;

        if (min_root[v_parent] < min_root[u_parent])
            min_root[u_parent] = min_root[v_parent];
    }
    else
    {
        g->cCon[u_parent] = v_parent;
        rank[v_parent]++;

        if (min_root[u_parent] < min_root[v_parent])
            min_root[v_parent] = min_root[u_parent];
    }

    return true;
}

void vicini_set_msf(grafo *g, arco *a, bool msf)
{
    elemento *curr = g->vicini[a->u];
    while (curr != NULL)
    {
        if (curr->id == a->v)
        {
            curr->msf = msf;
            break;
        }
        curr = curr->next;
    }
    curr = g->vicini[a->v];
    while (curr != NULL)
    {
        if (curr->id == a->u)
        {
            curr->msf = msf;
            break;
        }
        curr = curr->next;
    }
}
void ghash_set_msf(grafo *g, arco *a, bool msf)
{
    int hash = calcola_hash(a->u, a->v, g->hashSize);
    arco *curr = g->gHash[hash];
    while (curr != NULL)
    {
        if (curr->u == a->u && curr->v == a->v && curr->weight == a->weight)
        {
            curr->msf = msf;
            break;
        }
        curr = curr->next;
    }
}

static int compara_pesi(const void *a, const void *b)
{
    arco *arcoA = (arco *)a;
    arco *arcoB = (arco *)b;
    if (arcoA->weight < arcoB->weight)
        return -1;
    if (arcoA->weight > arcoB->weight)
        return 1;
    return 0;
}
void kruskal(grafo *g, arco *archi)
{
    qsort(archi, g->n_archi, sizeof(arco), compara_pesi);

    g->cCon = malloc(g->n_nodi * sizeof(int));
    int *rank = malloc(g->n_nodi * sizeof(int));
    int *minimi = malloc(g->n_nodi * sizeof(int));
    if (g->cCon == NULL || rank == NULL || minimi==NULL)
    {
        xtermina("Errore malloc kruskal", __LINE__, __FILE__);
    }

    g->costoMSF = 0;
    g->numCoCo = g->n_nodi;

    for (int i = 0; i < g->n_nodi; i++)
    {
        g->cCon[i] = i;
        minimi[i] = i;
        rank[i] = 0;
    }

    for (int i = 0; i < g->n_archi; i++)
    {
        int u = archi[i].u;
        int v = archi[i].v;
        int w = archi[i].weight;

        if (union_set(g, rank, minimi, u, v))
        {
            g->numCoCo--;
            g->costoMSF += w;
            archi[i].msf = true;
            vicini_set_msf(g, &archi[i], true);
            ghash_set_msf(g, &archi[i], true);
        }
        else
        {
            archi[i].msf = false;
        }
    }
    // normalizzo cCon settando per ogni componente connessa il minimo come identificatore
    for (int i = 0; i < g->n_nodi; i++)
    {
        int root = find(g, i);
        g->cCon[i] = minimi[root];
    }
    free(rank);
    free(minimi);
}

bool dfs_max_arco_msf(grafo *g, int curr, int target, bool *visited, int *max_u, int *max_v, int *max_w)
{
    if (curr == target)
        return true;

    visited[curr] = true;
    elemento *e = g->vicini[curr];

    while (e != NULL)
    {
        if (e->msf && !visited[e->id])
        {

            if (dfs_max_arco_msf(g, e->id, target, visited, max_u, max_v, max_w))
            {

                if (e->w > *max_w)
                {
                    *max_w = e->w;
                    *max_u = curr;
                    *max_v = e->id;
                }

                return true;
            }
        }

        e = e->next;
    }

    return false;
}
bool trova_max_arco_msf(grafo *g, int u, int v, int *max_u, int *max_v, int *max_w)
{
    bool *visited = calloc(g->n_nodi, sizeof(bool));

    if (visited == NULL)
        xtermina("calloc visited fallita", __LINE__, __FILE__);

    *max_w = INT_MIN;
    *max_u = -1;
    *max_v = -1;

    bool trovato = dfs_max_arco_msf(g, u, v, visited, max_u, max_v, max_w);
    if (!trovato)
        xtermina("Cammino MSF non trovato", __LINE__, __FILE__);
    free(visited);

    return trovato;
}
void aggiungi_arco(grafo *g, int u, int v, int w, pthread_mutex_t *mut_stampa)
{
    if (u > v || u < 0 || v < 0 || u > g->n_nodi - 1 || v > g->n_nodi - 1)
    {
        fprintf(stderr, "op + ad arco non valido (%d) (%d) (%d)\n", u, v, w);

        xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
        printf("+ %d %d %d 0\n", u, v, w);
        xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);

        return ;
    }
    int cu, cv;
    lock_componenti(g, u, v, &cu, &cv);
    if (cv != cu)
    {
        lock_ghash(g, u, v);
        if (!ghash_inserisci(g, u, v, w, true))
        {
            // arco gia presente
            fprintf(stderr, "arco (%d)\t(%d)\t(%d) gia presente", u, v, w);
            unlock_ghash(g, u, v);

            xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
            printf("+ %d %d %d 0\n", u, v, w);
            xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);

            unlock_componenti(g, cu, cv);
            return ;
        }
        unlock_ghash(g, u, v);
        vicini_inserisci_ordinato(&g->vicini[u], v, w, true);
        vicini_inserisci_ordinato(&g->vicini[v], u, w, true);

        xpthread_mutex_lock(&g->mut_busy, __LINE__, __FILE__);
        if (cu < cv)
        {
            // aggiorno gli identificatori di cCon di v con u
            for (int i = 0; i < g->n_nodi; i++)
            {
                if (g->cCon[i] == cv)
                {
                    g->cCon[i] = cu;
                }
            }
        }
        else
        {
            for (int i = 0; i < g->n_nodi; i++)
            {
                if (g->cCon[i] == cu)
                {
                    g->cCon[i] = cv;
                }
            }
        }
        xpthread_mutex_unlock(&g->mut_busy, __LINE__, __FILE__);

        xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
        g->numCoCo--;
        g->costoMSF += w;
        g->n_archi++;
        xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);
    }
    else
    {

        lock_ghash(g, u, v);
        if (!ghash_inserisci(g, u, v, w, false))
        {
            fprintf(stderr, "arco (%d)\t(%d)\t(%d) gia presente", u, v, w);
            unlock_ghash(g, u, v);

            xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
            printf("+ %d %d %d 0\n", u, v, w);
            xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);

            unlock_componenti(g, cu, cv);
            return ;
        }
        unlock_ghash(g, u, v);
        vicini_inserisci_ordinato(&g->vicini[u], v, w, false);
        vicini_inserisci_ordinato(&g->vicini[v], u, w, false);

        xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
        g->n_archi++;
        xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);

        int max_u, max_v;
        int max_w;

        trova_max_arco_msf(g, u, v, &max_u, &max_v, &max_w);

        if (w < max_w)
        {
            // normalizzo l'arco perché in gHash gli archi sono sempre salvati con u < v
            int old_u = max_u < max_v ? max_u : max_v;
            int old_v = max_u < max_v ? max_v : max_u;

            arco nuovo = {.u = u, .v = v, .weight = w};
            arco old = {.u = old_u, .v = old_v, .weight = max_w};
            lock_ghash(g, old.u, old.v);
            ghash_set_msf(g, &old, false);
            unlock_ghash(g, old.u, old.v);

            vicini_set_msf(g, &old, false);

            lock_ghash(g, nuovo.u, nuovo.v);
            ghash_set_msf(g, &nuovo, true);
            unlock_ghash(g, nuovo.u, nuovo.v);

            vicini_set_msf(g, &nuovo, true);

            xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
            g->costoMSF = g->costoMSF - max_w + w;
            xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);
        }
    }
    xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
    printf("+ %d %d %d %d %d %ld\n", u, v, w, g->n_archi, g->numCoCo, g->costoMSF);
    xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);

    unlock_componenti(g, cu, cv);

    return ;
}

int get_Lx(grafo *g, int x, bool *Lx)
{
    int min_id = x;
    int *stack = malloc(g->n_nodi * sizeof(int));
    if (stack == NULL)
    {
        xtermina("Errore malloc", __LINE__, __FILE__);
    }

    int top = 0;
    stack[top] = x;
    top++;
    Lx[x] = true;
    while (top > 0)
    {
        top--;
        int curr_node = stack[top];

        if (curr_node < min_id)
            min_id = curr_node;

        elemento *curr = g->vicini[curr_node];

        while (curr != NULL)
        {
            if (curr->msf && !Lx[curr->id])
            {
                Lx[curr->id] = true;
                stack[top] = curr->id;
                top++;
            }

            curr = curr->next;
        }
    }
    free(stack);
    return min_id;
}
void vicini_cancella_arco(grafo *g, arco *to_delete)
{
    int u = to_delete->u;
    int v = to_delete->v;
    elemento *curr = g->vicini[u];
    elemento *prec = NULL;
    while (curr != NULL)
    {
        if (curr->id == v)
        {
            if (prec == NULL)
                g->vicini[u] = curr->next;
            else
                prec->next = curr->next;
            free(curr);
            break;
        }
        prec = curr;
        curr = curr->next;
    }
    curr = g->vicini[v];
    prec = NULL;
    while (curr != NULL)
    {
        if (curr->id == u)
        {
            if (prec == NULL)
                g->vicini[v] = curr->next;
            else
                prec->next = curr->next;
            free(curr);
            return;
        }
        prec = curr;
        curr = curr->next;
    }
}
void ghash_cancella_arco(grafo *g, arco *to_delete)
{ // non controlla se esiste e non decrementa n_archi
    int hash = calcola_hash(to_delete->u, to_delete->v, g->hashSize);
    arco *curr = g->gHash[hash];
    arco *prec = NULL;
    while (curr != NULL)
    {
        if (curr->u == to_delete->u && curr->v == to_delete->v)
        {
            if (prec == NULL)
                g->gHash[hash] = curr->next;
            else
                prec->next = curr->next;
            free(curr);
            return;
        }
        prec = curr;
        curr = curr->next;
    }
}
arco trova_best_arco_Lu_Lv(grafo *g, bool *Lu, bool *Lv, bool *trovato)
{
    arco best;
    *trovato = false;
    for (int i = 0; i < g->hashSize; i++)
    {
        int m = i % g->nmutex;
        xpthread_mutex_lock(&g->mut_gHash[m], __LINE__, __FILE__);

        arco *curr = g->gHash[i];
        while (curr != NULL)
        {
            if (!curr->msf)
            {
                if ((Lu[curr->u] == true && Lv[curr->v] == true) || (Lu[curr->v] == true && Lv[curr->u] == true))
                {
                    if (!(*trovato) || curr->weight < best.weight)
                    {
                        best = *curr;
                        *trovato = true;
                    }
                }
            }
            curr = curr->next;
        }
        xpthread_mutex_unlock(&g->mut_gHash[m], __LINE__, __FILE__);
    }
    return best;
}
void cancella_arco(grafo *g, int u, int v, pthread_mutex_t *mut_stampa)
{ // true se concellato,false se arco non esiste
    if (u > v || u < 0 || v < 0 || u > g->n_nodi - 1 || v > g->n_nodi - 1)
    {
        fprintf(stderr, "op - ad arco non valido (%d) (%d)\n", u, v);
        xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
        printf("- %d %d 0\n", u, v);
        xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);
        return ;
    }
    int cu, cv;
    lock_componenti(g, u, v, &cu, &cv);
    lock_ghash(g, u, v);
    arco *to_delete = ghash_get_arco(g, u, v);
    if (to_delete == NULL)
    {
        unlock_ghash(g, u, v);

        xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
        printf("- %d %d 0\n", u, v);
        xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);

        unlock_componenti(g, cu, cv);
        return ;
    }
    arco tmp = *to_delete;
    if (!tmp.msf)
    {
        vicini_cancella_arco(g, &tmp);
        ghash_cancella_arco(g, &tmp);

        xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
        g->n_archi--;
        xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);

        unlock_ghash(g, u, v);
    }
    else
    {
        // in msf
        vicini_cancella_arco(g, &tmp);
        ghash_cancella_arco(g, &tmp);

        xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
        g->costoMSF -= tmp.weight;
        g->n_archi--;
        xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);

        unlock_ghash(g, u, v);

        bool *Lu = calloc(g->n_nodi, sizeof(bool));
        bool *Lv = calloc(g->n_nodi, sizeof(bool));
        if (Lu == NULL || Lv == NULL)
            xtermina("calloc Lu/Lv fallita", __LINE__, __FILE__);
        int min_u = get_Lx(g, u, Lu);
        int min_v = get_Lx(g, v, Lv);

        bool trovato;
        arco best = trova_best_arco_Lu_Lv(g, Lu, Lv, &trovato);
        if (trovato == false)
        {
            // diventano componenti disconnesse: aggiorno gli identificatori di componente
            xpthread_mutex_lock(&g->mut_busy, __LINE__, __FILE__);
            for (int i = 0; i < g->n_nodi; i++)
            {
                if (Lu[i] == true)
                    g->cCon[i] = min_u;
                else if (Lv[i] == true)
                    g->cCon[i] = min_v;
            }
            xpthread_mutex_unlock(&g->mut_busy, __LINE__, __FILE__);
            
            xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
            g->numCoCo++;
            xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);
        }
        else
        {
            lock_ghash(g, best.u, best.v);
            ghash_set_msf(g, &best, true);
            unlock_ghash(g, best.u, best.v);

            vicini_set_msf(g, &best, true);

            xpthread_mutex_lock(&g->mut_statistiche,__LINE__,__FILE__);
            g->costoMSF += best.weight;
            xpthread_mutex_unlock(&g->mut_statistiche,__LINE__,__FILE__);
        }

        free(Lu);
        free(Lv);
    }
    xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
    printf("- %d %d %d %d %ld\n", u, v, g->n_archi, g->numCoCo, g->costoMSF);
    xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);

    unlock_componenti(g, cu, cv);
    return ;
}

void libera_ghash(grafo *g)
{
    for (int i = 0; i < g->hashSize; i++)
    {

        arco *curr = g->gHash[i];

        while (curr != NULL)
        {
            arco *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }

    free(g->gHash);
}
void libera_vicini(grafo *g)
{
    for (int i = 0; i < g->n_nodi; i++)
    {

        elemento *curr = g->vicini[i];

        while (curr != NULL)
        {
            elemento *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }

    free(g->vicini);
}
void *consumer(void *arg)
{
    dati_thread *d = arg;
    buffer_condiviso *buf = d->buf;
    grafo *g = d->g;

    while (true)
    {
        operazione op;
        xpthread_mutex_lock(&buf->mutex, __LINE__, __FILE__);
        while (buf->count == 0 && !buf->fine_lettura)
        {
            xpthread_cond_wait(&buf->non_vuoto, &buf->mutex, __LINE__, __FILE__);
        }
        if (buf->count == 0 && buf->fine_lettura)
        {
            xpthread_mutex_unlock(&buf->mutex, __LINE__, __FILE__);
            return NULL;
        }
        op = buf->buffer[buf->c_index];
        buf->c_index = (buf->c_index + 1) % buf->size;
        buf->count--;

        xpthread_cond_signal(&buf->non_pieno, __LINE__, __FILE__);
        xpthread_mutex_unlock(&buf->mutex, __LINE__, __FILE__);

        if (op.op == '+')
        {
            aggiungi_arco(g, op.u, op.v, op.w, d->mut_stampa);
        }
        else if (op.op == '-')
        {
            cancella_arco(g, op.u, op.v, d->mut_stampa);
        }
        else
        {
            fprintf(stderr, "Operazione non valida: %c\n", op.op);
        }
    }
}
void producer(buffer_condiviso *buff, char *op_filename)
{
    FILE *op_file = xfopen(op_filename, "r", __LINE__, __FILE__);

    int nread;
    char *line = NULL;
    size_t len = 0;

    while ((nread = getline(&line, &len, op_file)) != -1)
    {
        operazione op;

        if (line[0] == '+')
        {
            if (sscanf(line, "+ %d %d %d", &op.u, &op.v, &op.w) == 3)
            {
                op.op = '+';
                xpthread_mutex_lock(&buff->mutex, __LINE__, __FILE__);
                while (buff->count == buff->size)
                {
                    xpthread_cond_wait(&buff->non_pieno, &buff->mutex, __LINE__, __FILE__);
                }
                buff->buffer[buff->p_index] = op;
                buff->p_index = (buff->p_index + 1) % buff->size;
                buff->count++;
                xpthread_cond_signal(&buff->non_vuoto, __LINE__, __FILE__);
                xpthread_mutex_unlock(&buff->mutex, __LINE__, __FILE__);
            }
        }

        if (line[0] == '-')
        {
            if (sscanf(line, "- %d %d", &op.u, &op.v) == 2)
            {
                op.op = '-';
                xpthread_mutex_lock(&buff->mutex, __LINE__, __FILE__);
                while (buff->count == buff->size)
                {
                    xpthread_cond_wait(&buff->non_pieno, &buff->mutex, __LINE__, __FILE__);
                }
                buff->buffer[buff->p_index] = op;
                buff->p_index = (buff->p_index + 1) % buff->size;
                buff->count++;
                xpthread_cond_signal(&buff->non_vuoto, __LINE__, __FILE__);
                xpthread_mutex_unlock(&buff->mutex, __LINE__, __FILE__);
            }
        }
    }

    xpthread_mutex_lock(&buff->mutex, __LINE__, __FILE__);
    buff->fine_lettura = true;
    xpthread_cond_broadcast(&buff->non_vuoto, __LINE__, __FILE__);
    xpthread_mutex_unlock(&buff->mutex, __LINE__, __FILE__);
    free(line);
    fclose(op_file);
}

void esegui_operazioni(grafo *g, char *filename_op, int n_thread)
{
    buffer_condiviso buf;

    buf.size = BUFFER_SIZE;

    buf.p_index = 0;
    buf.c_index = 0;
    buf.count = 0;
    buf.fine_lettura = false;

    xpthread_mutex_init(&buf.mutex, NULL, __LINE__, __FILE__);
    xpthread_cond_init(&buf.non_pieno, NULL, __LINE__, __FILE__);
    xpthread_cond_init(&buf.non_vuoto, NULL, __LINE__, __FILE__);

    pthread_t *threads = malloc(n_thread * sizeof(pthread_t));
    if (threads == NULL)
        xtermina("malloc threads fallita", __LINE__, __FILE__);

    pthread_mutex_t mut_stampa;
    xpthread_mutex_init(&mut_stampa, NULL, __LINE__, __FILE__);

    dati_thread dati;
    dati.g = g;
    dati.buf = &buf;
    dati.mut_stampa = &mut_stampa;

    for (int i = 0; i < n_thread; i++)
    {
        xpthread_create(&threads[i], NULL, consumer, &dati, __LINE__, __FILE__);
    }

    producer(&buf, filename_op);

    for (int i = 0; i < n_thread; i++)
    {
        xpthread_join(threads[i], NULL, __LINE__, __FILE__);
    }

    xpthread_mutex_destroy(&mut_stampa, __LINE__, __FILE__);

    xpthread_mutex_destroy(&buf.mutex, __LINE__, __FILE__);
    xpthread_cond_destroy(&buf.non_pieno, __LINE__, __FILE__);
    xpthread_cond_destroy(&buf.non_vuoto, __LINE__, __FILE__);

    free(threads);
}

void calcola_statistiche(grafo *g)
{
    int posizioni_non_vuote = 0;
    int max_lenght_list = 0;
    int n_archi = 0;
    long costo_MSF = 0;
    for (int i = 0; i < g->hashSize; i++)
    {
        int len = 0;
        arco *curr = g->gHash[i];
        while (curr != NULL)
        {
            len++;
            n_archi++;
            if (curr->msf)
                costo_MSF += curr->weight;
            curr = curr->next;
        }
        if (len > 0)
        {
            posizioni_non_vuote++;
            if (len > max_lenght_list)
                max_lenght_list = len;
        }
    }
    double adv = 0.0;
    if (posizioni_non_vuote > 0)
    {
        adv = (double)n_archi / posizioni_non_vuote;
    }
    bool *visto = calloc(g->n_nodi, sizeof(bool));
    if (visto == NULL)
        xtermina("Errore calloc", __LINE__, __FILE__);
    int numCoCo = 0;
    for (int i = 0; i < g->n_nodi; i++)
    {
        int id = g->cCon[i];
        if (!visto[id])
        {
            visto[id] = true;
            numCoCo++;
        }
    }
    free(visto);

    printf("Numero posizioni non vuote: %d\n", posizioni_non_vuote);
    printf("Lunghezza media liste: %.7f\n", adv);
    printf("Lunghezza massima liste: %d\n", max_lenght_list);
    printf("%d %d %ld\n", n_archi, numCoCo, costo_MSF);
}

int main(int argc, char **argv)
{
    int hashSize = 100000; // default
    int n_thread = 3;      // default
    int n_mutex = 1000;
    int opt;
    while ((opt = getopt(argc, argv, "t:H:M:")) != -1)
    {

        switch (opt)
        {

        case 't':
            n_thread = atoi(optarg);
            break;

        case 'H':
            hashSize = atoi(optarg);
            break;

        case 'M':
            n_mutex = atoi(optarg);
            break;

        default:
            fprintf(stderr, "Uso: %s file_grafo file_archi [-t threads] [-H hashsize] [-M nmutex]\n", argv[0]);
            exit(1);
        }
    }
    if (n_thread <= 0 || hashSize <= 0 || n_mutex <= 0)
    {
        fprintf(stderr, "Errore: parametri -t, -H, -M devono essere positivi\n");
        exit(1);
    }
    if (argc - optind != 2)
    {
        fprintf(stderr, "Uso: %s file_grafo file_archi [-t threads] [-H hashsize] [-M nmutex]\n", argv[0]);
        exit(1);
    }
    char *filename_grafo = argv[optind++];
    char *filename_op = argv[optind];

    int n_archi = 0;
    int n_nodi = 0;
    arco *archi = parser(filename_grafo, &n_archi, &n_nodi);

    grafo *g = calloc(1, sizeof(grafo));
    g->hashSize = hashSize;
    g->n_nodi = n_nodi;
    g->n_archi = n_archi;

    g->nmutex = n_mutex;
    g->mut_gHash = malloc(g->nmutex * sizeof(pthread_mutex_t));
    for (int i = 0; i < g->nmutex; i++)
    {
        xpthread_mutex_init(&g->mut_gHash[i], NULL, __LINE__, __FILE__);
    }

    g->busy = calloc(g->n_nodi, sizeof(bool));
    if(g->busy==NULL) xtermina("Errore calloc",__LINE__,__FILE__);
    xpthread_mutex_init(&g->mut_statistiche, NULL, __LINE__, __FILE__);
    xpthread_mutex_init(&g->mut_busy, NULL, __LINE__, __FILE__);
    xpthread_cond_init(&g->cond_busy, NULL, __LINE__, __FILE__);

    genera_vicini(g, archi);
    genera_ghash(g, archi);
    kruskal(g, archi);
    printf("%d %d %ld\n", g->n_archi, g->numCoCo, g->costoMSF);

    esegui_operazioni(g, filename_op, n_thread);
    printf("Operazioni terminate\n");
    calcola_statistiche(g);

    for (int i = 0; i < g->nmutex; i++)
        xpthread_mutex_destroy(&g->mut_gHash[i], __LINE__, __FILE__);
    free(g->mut_gHash);
    xpthread_mutex_destroy(&g->mut_statistiche, __LINE__, __FILE__);
    xpthread_mutex_destroy(&g->mut_busy, __LINE__, __FILE__);
    xpthread_cond_destroy(&g->cond_busy, __LINE__, __FILE__);

    free(g->busy);
    libera_ghash(g);
    libera_vicini(g);
    free(g->cCon);
    free(archi);
    free(g);
    return 0;
}
