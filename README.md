# Costruzione Lu Lv
Quando viene eseguita un operazione di cancellazione di un arco `x` in msf è necessario trovare se esiste un ulteriore arco (di costo minimo) che colleghi le due componenti disconnesse createsi dopo la rimozione di `x`.
Per farlo il mio codice crea due array di booleani di dimensione `n_nodi` ,`Lu` e `Lv`, tramite `calloc()` così da settarli a `false`.
Lu[i] vale true se il nodo i è raggiungibile da u; analogamente, Lv[i] vale true se il nodo i è raggiungibile da v.
Per popolare questi due array ho implementato la funzione `get_Lx`.
``` c
int get_Lx(grafo *g, int x, bool *Lx)
```
Si tratta di una funzione che esegue una visita DFS del grafo partendo da un nodo `x` seguendo solamente  archi appartenenti alla msf e visitando nodi non ancora esplorati.
Ho scelto di implementarla iterativamente per evitare problemi di overflow dello stack.
La funzione popola quindi l'array `Lx` passato come riferimento e restituisce un `int` ovvero il minimo nodo appartenente alla compoente connessa a cui `x` appartiene, in questo modo nel caso in cui non venga trovato un nuovo arco che collega le due compoenenti createsi sarà possibile aggiornare rapidamente gli identificatori in `cCon[]`.

`get_Lx()` sfrutta uno stack di interi `stack` di dimensione `n_nodi` indicizzato dalla variabile `top` ovvero l'indice della prima posizione libera. implementando così una politica LiFo fondamentale per una DFS.

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
si estrae un nodo `curr_node` ,si verifica se è minore del nodo minore trovato fino ad adesso. in tal caso si aggiorna `min_id`
``` c
if (curr_node < min_id)
            min_id = curr_node;
```
a questo punto si vanno a scoprire i nodi collegati a `curr_node` scorrendo la lista di adiacenza di `g->vicini[curr_node]`
``` c
elemento *curr = g->vicini[curr_node];
```
Dato che ci interessano solamente gli archi gia in msf si va ad aggiungere allo stack solamente i nodi collegati a `curr_node` tramite un arco già in msf.
Dato che siamo un un grafo non orientato è necessario soddisfare la condizione `!Lx[curr->id]` in quanto DFS tornerebbe indietro al padre.
```
u -> v
v -> u 
```
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
---
# Sincronizzazione

Per la sincronizzazione dei thread che si occupano della cancellazione e dell'aggiunta di  un nuovo arco ho deciso di implementare una logica simile a quella descritta nella traccia del progetto.
Fondamentalmente si protegge le strutture del grafo tramite le seguenti tecniche.
### gHash
Per proteggere la tabella hash `gHash` viene utilizzato un array di mutex `mut_gHash` di dimensione `nmutex`. Ogni bucket `gHash[i]` non possiede necessariamente un mutex dedicato: il mutex associato al bucket viene calcolato come `mut_gHash[i % nmutex]`.
Di conseguenza, bucket diversi della tabella hash possono condividere lo stesso mutex. Ad esempio, se nmutex è minore di hashSize, i bucket `gHash[0]`, `gHash[nmutex]`, `gHash[2*nmutex]` e così via vengono tutti protetti dallo stesso mutex `mut_gHash[0]`.
Questa scelta riduce il numero di mutex necessari, ma può diminuire il parallelismo: due thread che accedono a bucket distinti ma associati allo stesso mutex non posso eseguire in parallelo.
Quindi ogni qual volta si vuole accedere ad un bucket di gHash è necessario fare una lock.
```c
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
```
### cCon e Vicini

Due thread possono eseguire operazioni in parallelo quando coinvolgono componenti connesse diverse del grafo. Per stabilire a quale componente appartiene ciascun nodo viene usato l’array `cCon`: nella posizione `i` è memorizzato l’identificatore della componente connessa a cui appartiene il nodo `i`.

Un’operazione di inserimento o cancellazione di un arco può coinvolgere una sola componente, quando i due estremi dell’arco appartengono già alla stessa componente, oppure due componenti distinte, quando gli estremi appartengono a componenti diverse. È quindi necessario acquisire l’accesso esclusivo a tutte le componenti coinvolte.

Le strutture utilizzate sono:

```c
bool *busy;                 // dimensione n_nodi
pthread_mutex_t mut_busy;
pthread_cond_t cond_busy;
```

* `busy[i]` vale `true` se la componente identificata da `i` è attualmente riservata a un thread; vale `false` se la componente è libera.
* `mut_busy` protegge l’accesso concorrente a `busy` e a `cCon`.
* `cond_busy` è la condition variable usata dai thread per attendere che una componente coinvolta nell’operazione venga liberata.

Le funzioni principali sono `lock_componenti()` e `unlock_componenti()`:

```c
void lock_componenti(grafo *g, int u, int v, int *cu, int *cv)
{
    xpthread_mutex_lock(&g->mut_busy, __LINE__, __FILE__);//lock a protezione di busy
    *cu = g->cCon[u];//ID della compoenente connessa a cui appartiene u
    *cv = g->cCon[v];//ID della compoenente connessa a cui appartiene v
    //se almeno una delle due componenti connesse è segnata come occupata -> wait
    while (g->busy[*cu] || g->busy[*cv])
    {
        xpthread_cond_wait(&g->cond_busy, &g->mut_busy, __LINE__, __FILE__);
        //ri-recupero gli ID
        *cu = g->cCon[u];
        *cv = g->cCon[v];
    }
    //setto le compoenenti a busy
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
```

`lock_componenti()` acquisisce prima `mut_busy`, ricava da `cCon` gli identificatori delle componenti contenenti `u` e `v`, e verifica se una di esse risulta occupata. Se almeno una componente è già occupata, il thread esegue una `wait` su `cond_busy`. Al risveglio deve ricalcolare `cu` e `cv`, perché un altro thread potrebbe avere nel frattempo unito o diviso delle componenti e quindi modificato `cCon`.

Quando entrambe le componenti sono disponibili, il thread imposta i relativi valori di `busy` a `true` e rilascia `mut_busy`. Da quel momento l’operazione possiede l’accesso esclusivo alle componenti coinvolte.

Le liste di adiacenza `vicini` non hanno un mutex dedicato, ma sono protette indirettamente dal lock sulle componenti. Se una componente è marcata come `busy`, nessun altro thread può iniziare un’operazione sui nodi appartenenti a quella componente. Di conseguenza, durante l’operazione il thread può modificare o attraversare le liste `vicini` della componente in modo sicuro.

Alla conclusione dell’operazione, `unlock_componenti()` marca le componenti come libere e usa `pthread_cond_broadcast()` per risvegliare tutti i thread in attesa. 

### Aggiornamento cCon post-operazione

Se la cancellazione di un arco appartenente alla MSF divide una componente connessa in due parti e non viene trovato un arco sostitutivo, è necessario aggiornare `cCon` assegnando a ciascun nodo l’identificatore della nuova componente a cui appartiene.

```c
for (int i = 0; i < g->n_nodi; i++)
{
    if (Lu[i] == true)
        g->cCon[i] = min_u;
    else if (Lv[i] == true)
        g->cCon[i] = min_v;
}
```

Questo codice viene eseguito dopo che `lock_componenti()` ha rilasciato `mut_busy`. Il rilascio è necessario per permettere agli altri thread di continuare a lavorare su componenti diverse, ma introduce un problema se l’aggiornamento di `cCon` non viene protetto nuovamente dallo stesso mutex.

Una situazione problematica si verifica se il thread A sta aggiornando `cCon`, mentre il thread B entra in `lock_componenti()` ed esegue:

```c
*cu = g->cCon[u];
*cv = g->cCon[v];

while (g->busy[*cu] || g->busy[*cv])
```

Il thread B può leggere un valore di `cCon` mentre A lo sta modificando. In questo modo B potrebbe controllare `busy` usando un identificatore vecchio o nuovo ma non ancora completamente aggiornato.
Per evitare questa situazione, le scritture di `cCon` devono essere eseguite sotto `mut_busy`, lo stesso mutex usato da `lock_componenti()` per leggere `cCon`.

Per l'aggiornamento di cCon in caso di aggiunta di un arco che collega due compoenenti precedentemente separate il problema risulta piu limitato.
Per esempio, se vengono unite le componenti 2 e 5, tutti i nodi con identificatore 5 vengono aggiornati a 2:
```c
for (int i = 0; i < g->n_nodi; i++)
{
    if (g->cCon[i] == 5)
        g->cCon[i] = 2;
}
```
Prima di eseguire questo aggiornamento, lock_componenti() ha già impostato:
```c
busy[2] = true
busy[5] = true
```
Di conseguenza, se un altro thread legge un nodo della componente durante l’aggiornamento, può trovare il vecchio identificatore 5 oppure il nuovo identificatore 2, ma in entrambi i casi trova la componente marcata come occupata e quindi attende.
Tuttavia ho preferito proteggere anche questa parte di codice con una lock su `mut_busy`

### Aggiornamento `costoMSF` `n_archi` `numCoCo`
Queste statistiche generali del grafo sono aggiornate contemporaneamente da più thread tramite operazioni come ad esempio:
```c
g->numCoCo--;
g->costoMSF += w;
g->n_archi++;
...
```
tuttavia non sono operazioni atomiche, un operazione del tipo `a++` ma a livello della CPU diventa
``` txt
    tmp = a
    tmp = tmp + 1
    a = tmp
```
per cui potrebbe accadere che due aggiornamenti si sovrappongano creando incoerenza nei risultati.
Per risolvere questa criticità ho aggiunto un ulteriore mutex `mut_statistiche`
ogni qual volta si accede ad almeno una di queste tre variabili in codice condiviso tra piu thread.

### Stampe Operazioni
Per le stampe risultanti dalle operazioni di aggiunta e cancellazione di un arco ho aggiunto un mutex `mut_stampa` in quanto senza di esso le stampe potrebbero risultare mescolate e difficili da leggere.
```c
xpthread_mutex_lock(mut_stampa, __LINE__, __FILE__);
printf("+ %d %d %d 0\n", u, v, w);
xpthread_mutex_unlock(mut_stampa, __LINE__, __FILE__);
```

### Schema Produttore Consumatore
Il programma usa uno schema produttore-consumatori. Il thread produttore legge sequenzialmente le operazioni dal file e le inserisce nel buffer condiviso; i thread consumer estraggono le operazioni e le applicano al grafo.

Il buffer condiviso è protetto da:
``` c
pthread_mutex_t mutex;
pthread_cond_t non_pieno;
pthread_cond_t non_vuoto;
```
Il produttore acquisisce `buf.mutex`, attende su `non_pieno` se il buffer è pieno, inserisce l’operazione, aggiorna `p_index` e `count`, segnala `non_vuoto` e rilascia il mutex. Il consumer acquisisce lo stesso mutex, attende su `non_vuoto` se il buffer è vuoto, estrae un’operazione, aggiorna `c_index` e `count`, segnala `non_pieno` e rilascia il mutex.

 Dopo aver copiato l’operazione in una variabile locale, il consumer rilascia il mutex del buffer prima di modificare il grafo, così gli altri thread possono continuare a produrre o consumare operazioni.

### Sequenza aggiunta arco

Quando un consumer esegue `aggiungi_arco(g, u, v, w, ...)`, la sequenza è:

1. controlla che l’arco sia valido;
2. acquisisce l’esclusiva sulle componenti di `u` e `v` tramite `lock_componenti()`;
3. acquisisce il mutex del bucket hash dell’arco;
4. verifica che l’arco non sia già presente e, se non esiste, lo inserisce in `gHash`;
5. rilascia il mutex della hash;
6. inserisce l’arco nelle due liste di adiacenza;
7. aggiorna la MSF e le statistiche;
8. stampa l’esito;
9. libera le componenti con `unlock_componenti()`.

Se `u` e `v` appartengono a componenti diverse, il nuovo arco entra sicuramente nella MSF perché collega due componenti prima disconnesse. Le componenti vengono unite, `cCon` viene aggiornato e il numero di componenti diminuisce.

Se invece `u` e `v` appartengono già alla stessa componente, il nuovo arco crea un ciclo. Il programma cerca l’arco di peso massimo nel cammino MSF tra `u` e `v`. Se il nuovo arco ha peso minore, sostituisce quell’arco massimo nella MSF; altrimenti resta un arco non-MSF. In questo caso la componente non cambia e `cCon` non viene modificato.

### Sequenza temporale di una cancellazione

Quando un consumer esegue `cancella_arco(g, u, v, ...)`, la sequenza è:

1. controlla che l’arco sia valido;
2. acquisisce l’esclusiva sulle componenti di `u` e `v`;
3. acquisisce il mutex del bucket hash e cerca l’arco;
4. se l’arco non esiste, rilascia i lock e termina;
5. se l’arco non appartiene alla MSF, lo rimuove da `gHash` e dalle liste di adiacenza e decrementa `n_archi`;
6. se l’arco appartiene alla MSF, lo rimuove e costruisce gli insiemi `Lu` e `Lv` tramite due DFS sulla MSF residua;
7. cerca un arco non-MSF di peso minimo che colleghi `Lu` e `Lv`;
8. se lo trova, quell’arco entra nella MSF;
9. se non lo trova, la componente rimane divisa, `cCon` viene aggiornato e `numCoCo` aumenta;
10. stampa il risultato e libera le componenti.

### Quando le operazioni avvengono davvero in parallelo

Le operazioni possono essere eseguite in parallelo quando coinvolgono componenti connesse disgiunte.

Esempio:

```text
Thread A: aggiunge o cancella un arco nella componente {1, 2, 3, 4}
Thread B: aggiunge o cancella un arco nella componente {10, 11, 12}
```

Se gli identificatori delle componenti sono diversi e nessuna delle due è `busy`, entrambi i thread superano `lock_componenti()` e lavorano in parallelo sulle rispettive liste di adiacenza e sulla rispettiva parte della MSF.

Le operazioni avvengono in modo sequenziale quando coinvolgono almeno una componente comune.

Esempio:

```text
Thread A: operazione sull’arco (2, 5)
Thread B: operazione sull’arco (4, 7)
```

Se tutti questi nodi appartengono alla stessa componente connessa, A marca quella componente come `busy`. B, entrando in `lock_componenti()`, trova il flag occupato e attende su `cond_busy` fino al completamento dell’operazione di A.

Infine, anche operazioni su componenti diverse possono essere temporaneamente serializzate se accedono a bucket hash associati allo stesso mutex `mut_gHash[hash % nmutex]`, oppure mentre aggiornano statistiche globali, `cCon`.
