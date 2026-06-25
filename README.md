# Costruzione Lu Lv
Quando viene eseguita un operazione di cancellazione di un arco `x` in msf è necessario trovare se esiste un ulteriore arco (di costo minimo) che colleghi le due componenti disconnesse createsi dopo la rimozione di `x`.
Per farlo il mio codice crea due array di booleani di dimensione `n_nodi` ,`Lu` e `Lv`, tramite `calloc()` così da settarli a `false`.
Questi array conterranno `true` all'indice `i`  se il nodo `i` è raggiungibile da `u` o da `v` se ci troviamo in `Lv`.

Per popolare questi due array ho implementato la funzione `get_Lx`.
``` c
int get_Lx(grafo *g, int x, bool *Lx)
```
Si tratta di una funzione che esegue una visita DFS del grafo partendo da un nodo `x` seguendo solamente  archi appartenenti alla msf e che non sono stati ancora esplorati.
Ho scelto di implementarla iterativamente per evitare problemi di overflow dello stack.
La funzione popola quindi l'array `Lx` passato come riferimento e restituisce un `int` ovvero il minimo nodo appartenente alla compoente connessa a cui `x` appartiene, in questo modo nel caso in cui non venga trovato un nuovo arco che collega le due compoenenti createsi sarà possibile aggiornare rapidamente gli identificatori in `cCon[]`.

`get_Lx()` srutta uno stack di interi `stack` di dimensione `n_nodi` indicizzato dalla variabile `top` ovvero l'indice della prima posizione libera. implementando così una politica LiFo fondamentale per una DFS.

Per iniziare si inserisce `x` nello stack e lo si marca come visitato
```
stack[top] = x;
top++;
Lx[x] = true;
```
, poi finche lo stack non è vuoto
``` c
while (top > 0)
```
si estrae un nodo `curr_node` ,si verifica se è minore il compoenente minore trovato fino ad adesso. in tal caso si aggiorna `min_id`
``` c
if (curr_node < min_id)
            min_id = curr_node;
```
a questo punto si vanno a scoprire i nodi collegati a `curr_node` scorrendo la lista di adiacenza di `g->vicini[curr_node]`
``` c
elemento *curr = g->vicini[curr_node];
```
Dato che ci interessano solamente gli archi gia in msf si va ad aggiungere allo stack solamente i nodi collegati a `curr_node` tramite un arco già in msf.
Dato che siamo un un grafo non orientato è necessario soddisfare la condizione `!Lx[curr->id]` in quanto dato che ci troviamo in una struttura non orientata la DFS tornerebbe subito al padre.
u -> v
v -> u 
``` c
if (curr->msf && !Lx[curr->id])
            {
                Lx[curr->id] = true;
                stack[top] = curr->id;
                top++;
            }

```
dopo di che si continua scorrendo la lista di adiacenza
```
curr = curr->next;
```



