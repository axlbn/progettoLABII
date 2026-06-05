#!/usr/bin/env python3
import sys
import heapq
def parse_gr(filename):
    archi_dichiarati = None
    n_nodi = None
    archi = {}

    with open(filename, "r") as f:
        for line in f:
            parti = line.split()

            if not parti:
                continue

            if parti[0] == "c":
                continue

            if parti[0] == "p":
                if len(parti) != 4 or parti[1] != "sp":
                    raise ValueError("riga p non valida")

                if n_nodi is not None:
                    raise ValueError("riga p duplicata")

                try:
                    n_nodi = int(parti[2]) + 1
                    archi_dichiarati = int(parti[3])
                except ValueError:
                    raise ValueError("numero nodi/archi non valido nella riga p")

                if n_nodi <= 0:
                    raise ValueError("numero nodi non valido")

                if archi_dichiarati < 0:
                    raise ValueError("numero archi non valido")

            elif parti[0] == "a":
                if n_nodi is None or archi_dichiarati is None:
                    raise ValueError("arco trovato prima di riga p sp nel file grafo")
                if len(parti) != 4:
                    raise ValueError("riga a non valida")

                if n_nodi is None:
                    raise ValueError("arco trovato prima della riga p")

                try:
                    u = int(parti[1])
                    v = int(parti[2])
                    w = int(parti[3])
                except ValueError:
                    raise ValueError("u-v-w non valido nella riga a")

                if not check_arco(n_nodi,u,v,w):
                    raise ValueError("nodo fuori range nella riga a")

                if (u, v) in archi:
                    raise ValueError("arco duplicato")

                archi[(u, v)] = w

            else:
                continue

    if n_nodi is None or archi_dichiarati is None:
        raise ValueError("riga p sp non trovata nel file grafo")

    if archi_dichiarati != len(archi):
        raise ValueError("numero archi dichiarati diverso da archi presenti")

    return n_nodi, archi
def check_arco(n_nodi,u,v,w=0):#w is optional
    if u>=v :
        return False
    if(u<0 or v<0 or u>=n_nodi or v>=n_nodi ):
        return False
    return True

def aggiungi_arco(archi, u, v, w):
    if (u, v) not in archi:
        archi[(u, v)] = w


def cancella_arco(archi, u, v):
    if (u, v) in archi:
        del archi[(u, v)]

def exec_op(file_op, archi, n_nodi):
    with open(file_op, "r") as f:
        for line in f:
            parti = line.split()

            if not parti:
                continue

            if parti[0] == "+":
                if len(parti) != 4:
                    continue

                try:
                    u = int(parti[1])
                    v = int(parti[2])
                    w = int(parti[3])
                except ValueError:
                    continue

                if not check_arco(n_nodi, u, v):
                    continue

                aggiungi_arco(archi, u, v, w)
                    
            elif parti[0] == "-":
                if len(parti) != 3:
                    continue

                try:
                    u = int(parti[1])
                    v = int(parti[2])
                except ValueError:
                    continue

                if not check_arco(n_nodi, u, v):
                    continue

                cancella_arco(archi, u, v)

def genera_vicini(archi,n_nodi):
    vicini = [[] for _ in range(n_nodi)]
    for (u,v),w in archi.items():
        vicini[u].append((v,w))
        vicini[v].append((u,w))
    return vicini

def prim(start,vicini,visitato):
    heap = []
    costo = 0
    visitato[start]=True
    for nodo_dest, peso in vicini[start]:
        heapq.heappush(heap,(peso,nodo_dest))
    while heap:
        peso,nodo = heapq.heappop(heap)
        if visitato[nodo] == True:
            continue
        visitato[nodo]=True
        costo+=peso
        for prossimo,peso_arco in vicini[nodo]:
            if not visitato[prossimo]:
                heapq.heappush(heap,(peso_arco,prossimo))
    return costo

def exec_prim(n_nodi,vicini):
    visitato = [False]*n_nodi
    componenti =0
    costo_totale =0
    for nodo in range(n_nodi):
        if not visitato[nodo]:
            componenti+=1
            costo_totale+=prim(nodo,vicini,visitato)
    return componenti,costo_totale

def main():
    if len(sys.argv) != 3:
        print("Uso: python msf.py file_grafo file_archi", file=sys.stderr)
        sys.exit(1)
    file_grafo = sys.argv[1]
    file_op = sys.argv[2]

    try:
        n_nodi, archi = parse_gr(file_grafo)
        exec_op(file_op,archi,n_nodi);
        vicini = genera_vicini(archi,n_nodi)
        componenti, costo_msf = exec_prim(n_nodi, vicini)
        print(len(archi), componenti, costo_msf)

    except ValueError as e:
        print(f"Errore: {e}", file=sys.stderr)
        sys.exit(1)
if __name__ == '__main__':
    main()