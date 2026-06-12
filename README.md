## Costruzione Lu e Lv in canella_arco()
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



----------
## Sincronizzazione delle operazioni concorrenti

Le operazioni di inserimento e cancellazione degli archi vengono eseguite da più thread consumatori secondo uno schema produttore/consumatori. Il thread principale legge sequenzialmente il file delle operazioni e inserisce ogni operazione valida nel buffer condiviso. I thread consumatori prelevano una operazione dal buffer e la eseguono sul grafo.

Il buffer condiviso è protetto da un mutex `buf.mutex` e da due condition variables: `non_pieno` e `non_vuoto`. Il produttore acquisisce il mutex del buffer, attende su `non_pieno` se il buffer è pieno, inserisce l’operazione, aggiorna gli indici circolari e segnala `non_vuoto`. Ogni consumatore acquisisce lo stesso mutex, attende su `non_vuoto` se il buffer è vuoto, preleva una operazione, aggiorna gli indici e segnala `non_pieno`. In questo modo non ci sono race condition sugli indici `p_index`, `c_index`, sul campo `count` e sul flag `fine_lettura`.

Per proteggere la tabella hash `gHash` viene usato un array di mutex `mut_gHash` di dimensione `nmutex`. Quando un thread deve accedere alla lista della tabella hash contenente un arco `(u,v)`, calcola prima la posizione hash tramite `calcola_hash(u,v,hashSize)` e poi acquisisce il mutex `mut_gHash[hash % nmutex]`. Questo mutex viene mantenuto durante la ricerca, l’inserimento, la cancellazione o la modifica del flag `msf` dell’arco nella lista hash corrispondente. La dimensione dell’array di mutex può essere minore della dimensione della tabella hash: più posizioni della tabella hash possono quindi condividere lo stesso mutex, secondo la regola `i % nmutex`.

La sincronizzazione principale tra operazioni concorrenti avviene però a livello di componenti connesse. Il grafo contiene un array booleano `busy`, un mutex `mut_busy` e una condition variable `cond_busy`. Prima di modificare un arco `(u,v)`, un thread chiama `lock_componenti(g,u,v,&cu,&cv)`. Questa funzione acquisisce `mut_busy`, legge gli identificatori correnti delle componenti `cu = cCon[u]` e `cv = cCon[v]`, e controlla se una delle due componenti è già marcata come occupata. Se `busy[cu]` o `busy[cv]` è vero, il thread attende sulla condition variable `cond_busy`. Quando viene risvegliato, rilegge gli identificatori di componente, perché nel frattempo un altro thread potrebbe avere modificato `cCon`. Quando entrambe le componenti sono libere, il thread imposta `busy[cu] = true` e `busy[cv] = true`, quindi rilascia `mut_busy`.

Le liste di adiacenza `vicini` non sono protette da mutex specifici, ma sono protette indirettamente dal lock sulle componenti connesse. Ogni operazione su un arco `(u,v)` acquisisce prima l’esclusiva sulle componenti contenenti `u` e `v` tramite l’array `busy` e la condition variable `cond_busy`. Solo dopo questa acquisizione il thread può modificare `vicini[u]`, `vicini[v]`, i flag `msf` o visitare la MSF.

Questo garantisce che due thread non possano modificare contemporaneamente liste di adiacenza appartenenti alla stessa componente connessa. Le operazioni su componenti diverse, invece, possono procedere in parallelo perché coinvolgono insiemi disgiunti di nodi e quindi liste `vicini[i]` diverse. Anche le visite usate per cercare il massimo arco nel cammino MSF o per costruire `Lu` e `Lv` sono sicure, perché vengono eseguite mentre la componente interessata è marcata come occupata.

Dopo aver acquisito le componenti interessate, il thread può eseguire l’operazione di aggiunta o cancellazione. In questa fase può modificare le liste di adiacenza `vicini`, la tabella hash `gHash`, l’array delle componenti `cCon`, il numero di componenti `numCoCo`, il numero di archi `n_archi` e il costo della MSF `costoMSF`. Poiché nessun altro thread può lavorare contemporaneamente sulle stesse componenti connesse, queste modifiche non entrano in conflitto con operazioni che riguardano gli stessi nodi o la stessa componente.

Alla fine dell’operazione, il thread chiama `unlock_componenti(g,cu,cv)`. Questa funzione acquisisce `mut_busy`, imposta `busy[cu] = false` e `busy[cv] = false`, esegue una `broadcast` su `cond_busy` e rilascia il mutex. La `broadcast` risveglia tutti i thread eventualmente in attesa, che ricontrollano la disponibilità delle componenti prima di procedere.

Le stampe su `stdout` sono protette da un mutex separato `mut_stampa`. Ogni thread acquisisce questo mutex subito prima della `printf` relativa all’esito dell’operazione e lo rilascia subito dopo. Questo garantisce che l’output di una singola operazione venga scritto in modo atomico rispetto alle altre stampe, evitando righe mischiate tra thread diversi.

Questa strategia evita race condition perché:
- il buffer produttore/consumatori è sempre modificato tenendo acquisito `buf.mutex`;
- ogni accesso modificante alla lista corretta di `gHash` è protetto dal mutex associato alla posizione hash;
- due operazioni che modificano la stessa componente connessa non possono essere eseguite contemporaneamente, perché la componente viene marcata come `busy`;
- l’array `cCon` viene riletto dopo ogni attesa sulla condition variable, evitando di usare identificatori di componente diventati obsoleti;
- l’output è serializzato tramite `mut_stampa`.

Le operazioni possono invece essere eseguite effettivamente in parallelo quando riguardano componenti connesse diverse. Ad esempio, se un thread sta cancellando un arco `(2,5)` appartenente alla componente identificata da `1`, un secondo thread può contemporaneamente aggiungere o cancellare un arco `(10,14)` appartenente a una componente identificata da `10`, purché nessuna delle due componenti sia già occupata. In questo caso i due thread marcano come busy insiemi diversi di componenti e possono procedere in parallelo.

Analogamente, due operazioni su archi non correlati possono procedere in parallelo anche sulla tabella hash se gli archi finiscono in posizioni protette da mutex diversi. Per esempio, se l’arco `(3,7)` è associato a `mut_gHash[5]` e l’arco `(20,25)` è associato a `mut_gHash[31]`, le due operazioni possono modificare le rispettive liste hash contemporaneamente. Se invece due archi finiscono in posizioni hash diverse ma mappate sullo stesso mutex, le operazioni sulla hash table vengono serializzate per quella parte, anche se le componenti sono diverse. Questa scelta riduce il numero totale di mutex rispettando il vincolo sul parametro `nmutex`.

Le operazioni non vengono invece eseguite in parallelo quando coinvolgono la stessa componente connessa. Ad esempio, se un thread sta aggiungendo l’arco `(4,9)` e i nodi 4 e 9 appartengono alla componente identificata da 2, un altro thread che vuole cancellare l’arco `(5,8)` nella stessa componente deve attendere. Questo è necessario perché entrambe le operazioni potrebbero modificare gli stessi flag `msf`, il costo della MSF, le liste di adiacenza e potenzialmente l’array `cCon`.