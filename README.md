# 🛢️ RigGuard - Concurrent Oil Rig Emergency Response & Resource Management System

> OS Lab Mini Project | EGC 301P | Language: C

---

## 📋 Project Overview

RigGuard is a multi-user, CLI-based oil rig emergency response simulation built in C.
Multiple crew members concurrently request shared critical equipment (Fire Suppression,
Emergency Pump, Pressure Valve, etc.) across separate terminals. The system implements
all required OS concepts: role-based auth, file locking, concurrency control, data
consistency, socket programming and IPC via signals.

---

## ⚙️ How to Build

```bash
make            # builds both rigguard and rigguard_server
make clean      # removes binaries
make reset      # clears all CSV data and semaphores (fresh start)
```

---

## 🚀 How to Run

**Step 1 — Always start the server first (Terminal 0):**
```bash
./rigguard_server
```

**Step 2 — Start crew clients (separate terminals):**
```bash
./rigguard      # Terminal 1 (e.g., login as commander)
./rigguard      # Terminal 2 (e.g., login as ravi)
./rigguard      # Terminal 3 (e.g., login as arjun)
```

**Step 3 — Reset between test runs:**
```bash
make reset
```
---

## 🎯 OS Concepts Covered

| Concept | Implementation |
|---------|---------------|
| Role-Based Auth | 5 roles, commander-only options 10/11/12 |
| File Locking | flock() advisory lock + pthread_mutex |
| Concurrency | Multiple processes + 2 threads per process |
| Mutex | file_mutex (filestore), clients_mutex (server) |
| Semaphores | Named POSIX semaphores per resource |
| Data Consistency | Atomic read-modify-write with cross_lock() |
| Socket Programming | TCP client-server, rigguard_server + clients |
| IPC — Signals | SIGUSR1 (emergency), SIGUSR2 (deadlock) |

---

## 📡 Architecture

```
rigguard_server (TCP :9000)
       ├── handle_client thread ─── commander
       ├── handle_client thread ─── ravi
       └── handle_client thread ─── arjun

Each ./rigguard client:
       ├── Main thread     (menu, user input)
       ├── Listener thread (recv() socket messages)
       └── Monitor thread  (background deadlock scan every 5s)

Shared via OS kernel:
       ├── Named semaphores (/rigguard_sem_0 ... /rigguard_sem_N)
       ├── flock() on data/rigguard.lock
       └── POSIX signals (SIGUSR1, SIGUSR2) via data/pids/
```
