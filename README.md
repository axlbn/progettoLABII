All’interno della funzione `cancella_arco`, le strutture `Lu` e `Lv` vengono costruite solo quando l’arco eliminato appartiene alla MSF. In questo caso, la rimozione dell’arco può dividere una componente connessa in due parti: una raggiungibile da `u` e una raggiungibile da `v`.

`Lu` e `Lv` sono due array booleani, allocati con `calloc`, di dimensione pari al numero di nodi del grafo:

```c
bool *Lu = calloc(g->n_nodi, sizeof(bool));
bool *Lv = calloc(g->n_nodi, sizeof(bool));
```

Ogni posizione dell’array rappresenta un nodo. Se `Lu[i]` vale `true`, allora il nodo `i` appartiene alla componente raggiungibile da `u`; se `Lv[i]` vale `true`, appartiene alla componente raggiungibile da `v`.

La costruzione avviene tramite la funzione `get_Lx`, chiamata due volte:

```c
int min_u = get_Lx(g, u, Lu);
int min_v = get_Lx(g, v, Lv);
```

Questa funzione esegue una DFS iterativa, usando uno stack dinamico di interi per memorizzare i nodi da visitare:

```c
int *stack = malloc(g->n_nodi * sizeof(int));
```

Durante la visita, vengono esplorati i vicini del nodo corrente tramite le liste di adiacenza `g->vicini`. Tuttavia, vengono seguiti solo gli archi marcati come appartenenti alla MSF, cioè quelli con `curr->msf == true`.

Quando un nodo viene raggiunto per la prima volta, viene marcato nell’array booleano corrispondente:

```c
Lx[curr->id] = true;
```

Alla fine, `Lu` contiene tutti i nodi raggiungibili da `u` nella MSF dopo la cancellazione dell’arco, mentre `Lv` contiene quelli raggiungibili da `v`.

Queste due strutture vengono poi usate da `trova_best_arco_Lu_Lv` per cercare il miglior arco non appartenente alla MSF che colleghi un nodo di `Lu` con un nodo di `Lv`. Se questo arco esiste, viene inserito nella MSF; altrimenti le due componenti restano separate e vengono aggiornati gli identificatori di componente.


-------------------------
All’interno della funzione cancella_arco, le strutture Lu e Lv vengono costruite solo quando l’arco eliminato appartiene alla MSF. In questo caso, la rimozione dell’arco può dividere una componente connessa in due parti: una raggiungibile da u e una raggiungibile da v.

Lu e Lv sono due array booleani, allocati con calloc, di dimensione pari al numero di nodi del grafo. Ogni posizione dell’array rappresenta un nodo. Se Lu[i] vale true, allora il nodo i appartiene alla componente raggiungibile da u; se Lv[i] vale true, appartiene alla componente raggiungibile da v.

La costruzione avviene tramite la funzione get_Lx, chiamata due volte: una per partire dal nodo u e costruire Lu, e una per partire dal nodo v e costruire Lv.

La funzione get_Lx esegue una DFS iterativa, cioè una visita in profondità senza ricorsione. Per farlo usa uno stack dinamico di interi, che serve a memorizzare i nodi ancora da visitare.

Durante la visita vengono esplorati i vicini del nodo corrente tramite le liste di adiacenza g->vicini. Tuttavia, vengono seguiti solo gli archi marcati come appartenenti alla MSF, cioè quelli con curr->msf uguale a true.

Quando un nodo viene raggiunto per la prima volta, viene marcato nell’array booleano corrispondente impostando la sua posizione a true.

Alla fine, Lu contiene tutti i nodi raggiungibili da u nella MSF dopo la cancellazione dell’arco, mentre Lv contiene tutti i nodi raggiungibili da v.

Queste due strutture vengono poi usate da trova_best_arco_Lu_Lv per cercare il miglior arco non appartenente alla MSF che colleghi un nodo di Lu con un nodo di Lv. Se questo arco esiste, viene inserito nella MSF; altrimenti le due componenti restano separate e vengono aggiornati gli identificatori di componente.
