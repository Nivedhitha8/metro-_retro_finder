# Metro Route Finder (C, graph algorithms)

A DSA project: stations are graph vertices, tracks are weighted edges.
- **Fewest stops**: Breadth-First Search, O(V + E)
- **Fastest**: Dijkstra's algorithm with a binary min-heap, O((V + E) log V)

The C program is also the web server, so the whole site is one small binary.

## Run on localhost
1. Install **gcc** (Windows: MinGW-w64 or MSYS2; Linux/macOS: already available or `xcode-select --install`).
2. Double-click `run-local.bat` (Windows) or run `./run-local.sh`.
3. Open http://localhost:8080

Console version (no website): `gcc -O2 -o metro metro.c` then `./metro --cli`
(Windows: `gcc -O2 -o metro.exe metro.c -lws2_32`, then `metro.exe --cli`).

## Change the network
Edit `data/stations.txt` (format is explained at the top) and restart. A station listed under two lines becomes an interchange automatically. The times are approximate sample values.

## Deploy on Render
No database, no state, nothing to configure beyond the steps below.
1. Push this folder to a GitHub repository (the `Dockerfile` must be at the repo root).
2. On [render.com](https://render.com): **New +** > **Web Service** > connect the repo.
   (Or **New +** > **Blueprint**, which reads `render.yaml` and fills in everything.)
3. Set **Runtime** to **Docker**. Leave Build/Start commands empty, pick the Free plan.
4. Under **Advanced**, set **Health Check Path** to `/api/health` (already set if you used the Blueprint).
5. Click **Create Web Service**. When the build finishes, your site is live at `https://<name>.onrender.com`.

Render injects the `PORT` variable; the server binds to `0.0.0.0:$PORT`. The frontend calls `/api/...` with relative URLs, so no URL needs changing. On the free plan the service sleeps when idle, so the first request after a pause takes ~30-60 s.

Other Docker hosts work the same way. Local Docker: `docker build -t metro . && docker run -e PORT=8080 -p 8080:8080 metro`

## API
- `GET /api/stations`
- `GET /api/route?from=Guindy&to=Egmore&mode=time` (`mode` is `time` or `stops`)
