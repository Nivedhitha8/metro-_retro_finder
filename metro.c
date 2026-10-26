/*
 * metro.c - Metro Route Finder
 * DSA concepts: Graph (adjacency list), BFS, Dijkstra's algorithm, Binary Min-Heap (priority queue)
 *
 *   Station  = vertex            Track between stations = undirected weighted edge (weight = minutes)
 *   BFS      -> route with the FEWEST STOPS   (every edge counts as 1)          O(V + E)
 *   Dijkstra -> route with the LEAST TIME     (min-heap)                        O((V + E) log V)
 *
 * Run as a website:   ./metro          (open http://localhost:8080)
 * Run in the console: ./metro --cli
 * Build: gcc -O2 -o metro metro.c            (Windows/MinGW: add -lws2_32)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
  #include <winsock2.h>
  typedef int socklen_t;
  #define CLOSE closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <signal.h>
  #include <sys/time.h>
  typedef int SOCKET;
  #define INVALID_SOCKET (-1)
  #define CLOSE close
#endif

#define MAXN 256
#define MAXL 8
#define INF  1000000

/* ---------- Graph: adjacency list ---------- */
typedef struct Edge { int to, w, line; struct Edge *next; } Edge;
static char  name[MAXN][64];
static Edge *adj[MAXN];
static unsigned lmask[MAXN];          /* which lines serve a station (bit per line) */
static char  lname[MAXL][24];
static int   n = 0, nl = 0;

static int ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}
static int find(const char *s) { for (int i = 0; i < n; i++) if (ieq(name[i], s)) return i; return -1; }
static int station(const char *s) {
    int i = find(s);
    if (i >= 0) return i;
    if (n >= MAXN) { fprintf(stderr, "Too many stations\n"); exit(1); }
    strncpy(name[n], s, 63); return n++;
}
static void add_edge(int a, int b, int w, int line) {
    Edge *e = malloc(sizeof *e); e->to = b; e->w = w; e->line = line; e->next = adj[a]; adj[a] = e;
    e = malloc(sizeof *e);       e->to = a; e->w = w; e->line = line; e->next = adj[b]; adj[b] = e;
}
static void trim(char *s) {
    int l = (int)strlen(s);
    while (l > 0 && isspace((unsigned char)s[l - 1])) s[--l] = 0;
}
static int load(const char *file) {
    FILE *f = fopen(file, "r"); if (!f) return 0;
    char ln[256]; int prev = -1, pw = 2, line = -1;
    while (fgets(ln, sizeof ln, f)) {
        trim(ln);
        if (!*ln) { prev = -1; continue; }
        if (!strncmp(ln, "#line ", 6)) {
            if (nl >= MAXL) break;
            line = nl++; strncpy(lname[line], ln + 6, 23); prev = -1; continue;
        }
        if (ln[0] == '#' || line < 0) continue;
        char *c = strrchr(ln, ','); int w = 2;
        if (c) { *c = 0; w = atoi(c + 1); }
        int id = station(ln); lmask[id] |= 1u << line;
        if (prev >= 0) add_edge(prev, id, pw > 0 ? pw : 2, line);
        prev = id; pw = w;
    }
    fclose(f); return n > 0;
}

/* ---------- Binary min-heap (priority queue) ---------- */
typedef struct { int v, d; } HN;
static HN hp[8192]; static int hn;
static void hpush(int v, int d) {
    if (hn >= 8191) return;
    int i = hn++; hp[i].v = v; hp[i].d = d;
    while (i > 0 && hp[(i - 1) / 2].d > hp[i].d) { HN t = hp[i]; hp[i] = hp[(i - 1) / 2]; hp[(i - 1) / 2] = t; i = (i - 1) / 2; }
}
static HN hpop(void) {
    HN top = hp[0]; hp[0] = hp[--hn]; int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < hn && hp[l].d < hp[m].d) m = l;
        if (r < hn && hp[r].d < hp[m].d) m = r;
        if (m == i) break;
        HN t = hp[i]; hp[i] = hp[m]; hp[m] = t; i = m;
    }
    return top;
}

/* ---------- Algorithms ---------- */
static int prevn[MAXN];

/* BFS: fewest stops */
static void bfs(int s) {
    int q[MAXN], h = 0, t = 0, seen[MAXN] = {0};
    for (int i = 0; i < n; i++) prevn[i] = -1;
    q[t++] = s; seen[s] = 1;
    while (h < t) {
        int u = q[h++];
        for (Edge *e = adj[u]; e; e = e->next)
            if (!seen[e->to]) { seen[e->to] = 1; prevn[e->to] = u; q[t++] = e->to; }
    }
}
/* Dijkstra: least travel time */
static void dijkstra(int s) {
    int dist[MAXN];
    for (int i = 0; i < n; i++) { dist[i] = INF; prevn[i] = -1; }
    dist[s] = 0; hn = 0; hpush(s, 0);
    while (hn) {
        HN c = hpop();
        if (c.d > dist[c.v]) continue;                 /* stale heap entry */
        for (Edge *e = adj[c.v]; e; e = e->next)
            if (dist[c.v] + e->w < dist[e->to]) { dist[e->to] = dist[c.v] + e->w; prevn[e->to] = c.v; hpush(e->to, dist[e->to]); }
    }
}
static Edge *edge(int a, int b) {
    Edge *best = NULL;
    for (Edge *e = adj[a]; e; e = e->next) if (e->to == b && (!best || e->w < best->w)) best = e;
    return best;
}
/* mode 1 = fewest stops (BFS), 0 = least time (Dijkstra). Returns number of stations on the path, 0 if none. */
static int find_route(int s, int d, int mode, int *path) {
    if (mode == 1) bfs(s); else dijkstra(s);
    if (s != d && prevn[d] < 0) return 0;
    int tmp[MAXN], k = 0;
    for (int v = d; v != -1; v = prevn[v]) tmp[k++] = v;
    for (int i = 0; i < k; i++) path[i] = tmp[k - 1 - i];
    return k;
}
/* Line used at path position i (the track leaving it; for the last station, the track arriving). */
static int line_at(const int *p, int k, int i) {
    if (k < 2) return -1;
    Edge *e = i < k - 1 ? edge(p[i], p[i + 1]) : edge(p[i - 1], p[i]);
    return e ? e->line : -1;
}

/* ---------- Console mode ---------- */
static void cli(void) {
    char a[128], b[128], m[16];
    printf("Metro Route Finder (%d stations). Type 'list' to see them, Ctrl+C to quit.\n", n);
    for (;;) {
        printf("\nFrom: "); if (!fgets(a, sizeof a, stdin)) return; trim(a);
        if (ieq(a, "list")) { for (int i = 0; i < n; i++) printf("  %s\n", name[i]); continue; }
        printf("To: ");   if (!fgets(b, sizeof b, stdin)) return; trim(b);
        printf("Optimise for (time/stops) [time]: "); if (!fgets(m, sizeof m, stdin)) return; trim(m);
        int s = find(a), d = find(b), path[MAXN];
        if (s < 0 || d < 0) { printf("Unknown station.\n"); continue; }
        int k = find_route(s, d, ieq(m, "stops"), path);
        if (!k) { printf("No route found.\n"); continue; }
        int mins = 0, changes = 0;
        for (int i = 0; i < k; i++) {
            int ln = line_at(path, k, i);
            printf("  %-52s %s\n", name[path[i]], ln >= 0 ? lname[ln] : "");
            if (i < k - 1) { mins += edge(path[i], path[i + 1])->w; if (i > 0 && line_at(path, k, i) != line_at(path, k, i - 1)) changes++; }
        }
        printf("Stops: %d | Time: %d min | Interchanges: %d\n", k - 1, mins, changes);
    }
}

/* ---------- Tiny HTTP server (website + JSON API) ---------- */
static char out[1 << 16];
static int  off;
#define ADD(...) (off += snprintf(out + off, sizeof out - off, __VA_ARGS__))

static void urldecode(char *s) {
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') *o++ = ' ';
        else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) { char h[3] = { s[1], s[2], 0 }; *o++ = (char)strtol(h, NULL, 16); s += 2; }
        else *o++ = *s;
    }
    *o = 0;
}
static void qparam(const char *q, const char *key, char *dst, int cap) {
    dst[0] = 0; size_t kl = strlen(key);
    for (const char *p = q; p && *p; ) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            p += kl + 1; int i = 0; while (*p && *p != '&' && i < cap - 1) dst[i++] = *p++;
            dst[i] = 0; urldecode(dst); return;
        }
        p = strchr(p, '&'); if (p) p++;
    }
}
static void json_stations(void) {
    ADD("[");
    for (int i = 0; i < n; i++) {
        ADD("%s{\"name\":\"%s\",\"lines\":[", i ? "," : "", name[i]);
        int first = 1; for (int l = 0; l < nl; l++) if (lmask[i] >> l & 1) { ADD("%s\"%s\"", first ? "" : ",", lname[l]); first = 0; }
        ADD("]}");
    }
    ADD("]");
}
static void json_route(const char *from, const char *to, const char *mode) {
    int s = find(from), d = find(to), path[MAXN];
    if (s < 0 || d < 0) { ADD("{\"ok\":false,\"error\":\"Choose both stations from the list.\"}"); return; }
    if (s == d)         { ADD("{\"ok\":false,\"error\":\"Start and destination are the same station.\"}"); return; }
    int m = !strcmp(mode, "stops"), k = find_route(s, d, m, path);
    if (!k) { ADD("{\"ok\":false,\"error\":\"No route between these stations.\"}"); return; }
    int mins = 0, changes = 0;
    for (int i = 0; i < k - 1; i++) {
        mins += edge(path[i], path[i + 1])->w;
        if (i > 0 && line_at(path, k, i) != line_at(path, k, i - 1)) changes++;
    }
    ADD("{\"ok\":true,\"mode\":\"%s\",\"stops\":%d,\"minutes\":%d,\"interchanges\":%d,\"path\":[", m ? "stops" : "time", k - 1, mins, changes);
    for (int i = 0; i < k; i++) {
        int ln = line_at(path, k, i), change = i > 0 && i < k - 1 && ln != line_at(path, k, i - 1);
        ADD("%s{\"station\":\"%s\",\"line\":\"%s\",\"change\":%s}", i ? "," : "", name[path[i]], ln >= 0 ? lname[ln] : "", change ? "true" : "false");
    }
    ADD("]}");
}
static void reply(SOCKET c, int code, const char *type, const char *body, int len) {
    char h[256];
    int hl = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", code, code == 200 ? "OK" : "Not Found", type, len);
    send(c, h, hl, 0); if (len) send(c, body, len, 0);
}
static void serve(int port) {
#ifdef _WIN32
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
#else
    signal(SIGPIPE, SIG_IGN);              /* a client that disconnects must not kill the server */
#endif
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0); int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((unsigned short)port);
    if (srv == INVALID_SOCKET || bind(srv, (struct sockaddr *)&a, sizeof a) < 0 || listen(srv, 16) < 0) { fprintf(stderr, "Cannot listen on port %d\n", port); exit(1); }
    printf("Metro Route Finder: %d stations, %d lines. Listening on 0.0.0.0:%d\n", n, nl, port); fflush(stdout);
    for (;;) {
        SOCKET c = accept(srv, NULL, NULL); if (c == INVALID_SOCKET) continue;
#ifdef _WIN32
        DWORD tmo = 3000; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof tmo);
#else
        struct timeval tmo = { 3, 0 }; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tmo, sizeof tmo);
#endif                                     /* idle connections (health probes) must not block the server */
        char req[4096]; int got = recv(c, req, sizeof req - 1, 0);
        if (got <= 0) { CLOSE(c); continue; }
        req[got] = 0;
        char path[512] = "", *q = NULL;
        if (sscanf(req, "GET %511s", path) != 1) { reply(c, 404, "text/plain", "Not found", 9); CLOSE(c); continue; }
        if ((q = strchr(path, '?'))) *q++ = 0;
        off = 0;
        if (!strcmp(path, "/api/health"))        { ADD("{\"status\":\"ok\",\"stations\":%d}", n); reply(c, 200, "application/json", out, off); }
        else if (!strcmp(path, "/api/stations")) { json_stations(); reply(c, 200, "application/json", out, off); }
        else if (!strcmp(path, "/api/route")) {
            char f[128], t[128], m[16]; qparam(q, "from", f, sizeof f); qparam(q, "to", t, sizeof t); qparam(q, "mode", m, sizeof m);
            json_route(f, t, m); reply(c, 200, "application/json", out, off);
        } else if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
            FILE *fp = fopen("frontend/index.html", "rb");
            if (!fp) reply(c, 404, "text/plain", "frontend/index.html missing", 27);
            else { off = (int)fread(out, 1, sizeof out, fp); fclose(fp); reply(c, 200, "text/html; charset=utf-8", out, off); }
        } else reply(c, 404, "text/plain", "Not found", 9);
        CLOSE(c);
    }
}

int main(int argc, char **argv) {
    if (!load("data/stations.txt")) { fprintf(stderr, "Run from the project folder: data/stations.txt not found.\n"); return 1; }
    if (argc > 1 && !strcmp(argv[1], "--cli")) { cli(); return 0; }
    const char *p = getenv("PORT"); int port = p ? atoi(p) : 0;
    serve(port > 0 ? port : 8080);
    return 0;
}
