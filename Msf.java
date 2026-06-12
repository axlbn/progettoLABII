import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class Msf {

    static class Arco {
        int u;
        int v;
        int w;

        Arco(int u, int v, int w) {
            this.u = u;
            this.v = v;
            this.w = w;
        }

        @Override
        public String toString() {
            return u + " " + v + " " + w;
        }

    }

    static class Grafo {
        int nNodi;
        int n_archi;
        long n_cCon;
        long costo_MSF = 0;
        HashMap<String, Arco> archi;

        Grafo(int nNodi, int n_archi, HashMap<String, Arco> archi) {
            this.nNodi = nNodi;
            this.n_archi = n_archi;
            this.archi = archi;
            this.n_cCon = nNodi;
        }

    }

    static String key(int u, int v) {
        return u + "," + v;
    }

    static boolean checkArco(int nNodi, int u, int v) {
        return u < v && u >= 0 && v >= 0 && u < nNodi && v < nNodi;
    }

    static Grafo parseGr(String filename) throws IOException {
        Integer nNodi = null;
        Integer archiDichiarati = null;
        HashMap<String, Arco> archi = new HashMap<>();

        try (BufferedReader br = new BufferedReader(new FileReader(filename))) {
            String line;

            while ((line = br.readLine()) != null) {
                line = line.trim();

                if (line.isEmpty()) {
                    continue;
                }

                String[] parti = line.split(" ");

                if (parti[0].equals("c")) {
                    continue;
                }

                if (parti[0].equals("p")) {
                    if (parti.length != 4 || !parti[1].equals("sp")) {
                        throw new IllegalArgumentException("riga p non valida");
                    }

                    if (nNodi != null) {
                        throw new IllegalArgumentException("riga p duplicata");
                    }

                    try {
                        nNodi = Integer.parseInt(parti[2]) + 1;
                        archiDichiarati = Integer.parseInt(parti[3]);
                    } catch (NumberFormatException e) {
                        throw new IllegalArgumentException("numero nodi/archi non valido nella riga p");
                    }

                    if (nNodi <= 0) {
                        throw new IllegalArgumentException("numero nodi non valido");
                    }

                    if (archiDichiarati < 0) {
                        throw new IllegalArgumentException("numero archi non valido");
                    }

                } else if (parti[0].equals("a")) {
                    if (parti.length != 4) {
                        throw new IllegalArgumentException("riga a non valida");
                    }

                    if (nNodi == null || archiDichiarati == null) {
                        throw new IllegalArgumentException("arco trovato prima della riga p sp");
                    }

                    int u;
                    int v;
                    int w;

                    try {
                        u = Integer.parseInt(parti[1]);
                        v = Integer.parseInt(parti[2]);
                        w = Integer.parseInt(parti[3]);
                    } catch (NumberFormatException e) {
                        throw new IllegalArgumentException("u-v-w non valido nella riga a");
                    }

                    if (!checkArco(nNodi, u, v)) {
                        throw new IllegalArgumentException("arco non valido nella riga a");
                    }

                    String k = key(u, v);

                    if (archi.containsKey(k)) {
                        throw new IllegalArgumentException("arco duplicato");
                    }

                    archi.put(k, new Arco(u, v, w));

                } else {
                    continue;
                }
            }
        }

        if (nNodi == null || archiDichiarati == null) {
            throw new IllegalArgumentException("riga p sp non trovata");
        }

        if (archi.size() != archiDichiarati) {
            throw new IllegalArgumentException("numero archi dichiarati diverso da archi presenti");
        }

        return new Grafo(nNodi, archiDichiarati, archi);
    }

    static void execOp(Grafo g, String filename_op) throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(filename_op))) {
            String line;

            while ((line = br.readLine()) != null) {
                line = line.trim();

                if (line.isEmpty()) {
                    continue;
                }
                int u;
                int v;
                int w;
                String[] parti = line.split("\\s+");

                if (parti[0].equals("+")) {
                    if (parti.length != 4) {
                        continue;
                    }
                    try {
                        u = Integer.parseInt(parti[1]);
                        v = Integer.parseInt(parti[2]);
                        w = Integer.parseInt(parti[3]);
                    } catch (NumberFormatException e) {
                        continue;
                    }

                    if (!checkArco(g.nNodi, u, v)) {
                        // arco non valido
                        continue;
                    }

                    String k = key(u, v);

                    if (g.archi.containsKey(k)) {
                        // arco già presente
                        continue;
                    }

                    g.archi.put(k, new Arco(u, v, w));
                    g.n_archi++;
                } else if (parti[0].equals("-")) {
                    if (parti.length != 3) {
                        continue;
                    }
                    try {
                        u = Integer.parseInt(parti[1]);
                        v = Integer.parseInt(parti[2]);
                    } catch (NumberFormatException e) {
                        continue;
                    }
                    String k = key(u, v);
                    if (!checkArco(g.nNodi, u, v)) {
                        continue;
                    }
                    if (!g.archi.containsKey(k)) {
                        // arco non presente
                        continue;
                    }
                    g.archi.remove(k);
                    g.n_archi--;
                }
            }
        }
    }

    static class UnionFind {
        int[] parent;
        int[] rank;

        UnionFind(int n) {
            parent = new int[n];
            rank = new int[n];
            for (int i = 0; i < n; i++) {
                parent[i] = i;
                rank[i] = 0;
            }
        }

        int find(int node) {
            if (parent[node] != node) {
                node = find(parent[node]);
            }
            return parent[node];
        }

        boolean union(int u, int v) {
            int u_parent = find(u);
            int v_parent = find(v);
            if (u_parent == v_parent) {
                // stessa cCon
                return false;
            }
            if (rank[u_parent] > rank[v_parent]) {
                parent[v_parent] = u_parent;
            }
            if (rank[v_parent] > rank[u_parent]) {
                parent[u_parent] = v_parent;
            } else {
                parent[u_parent] = v_parent;
                rank[u_parent]++;
            }
            return true;
        }
    }

    static void kruskal(Grafo g) {
        List<Arco> archi_ordinati = new ArrayList<>(g.archi.values());
        UnionFind uf = new UnionFind(g.nNodi);
        archi_ordinati.sort((a, b) -> {
            return Integer.compare(a.w, b.w);
        });
        archi_ordinati.forEach(a -> {
            if (uf.union(a.u, a.v)) {
                g.n_cCon--;
                g.costo_MSF += a.w;
            }
        });
    }

    public static void main(String[] args) {
        if (args.length != 2) {
            System.err.println("Uso: java Msf file_grafo file_archi");
            System.exit(1);
        }

        try {
            Grafo g = parseGr(args[0]);
            execOp(g, args[1]);
            kruskal(g);
            System.out.println(g.n_archi + " " + g.n_cCon + " " + g.costo_MSF);

        } catch (IOException e) {
            System.err.println("Errore file: " + e.getMessage());
            System.exit(1);

        } catch (IllegalArgumentException e) {
            System.err.println("Errore: " + e.getMessage());
            System.exit(1);
        }
    }
}