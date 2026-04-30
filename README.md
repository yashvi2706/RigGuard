# 🛢️ RigGuard — Concurrent Oil Rig Emergency Response & Resource Management System

> OS Lab Mini Project | EGC 301P | Language: C

---

## 📋 Project Overview

RigGuard is a multi-user, CLI-based oil rig emergency response simulation built in C.
Multiple crew members concurrently request shared critical equipment (Fire Suppression,
Emergency Pump, Pressure Valve, etc.) across separate terminals. The system implements
all required OS concepts: role-based auth, file locking, concurrency control, data
consistency, socket programming, and IPC via signals.

---

## 🗂️ Directory Structure

```
RigGuard/
├── src/                    # All source code
│   ├── main.c              # Entry point, boot, menu loop
│   ├── auth.c / auth.h     # Password auth, login, lockout
│   ├── resources.c / .h    # Equipment request/release, semaphores
│   ├── deadlock.c / .h     # Banker's Algorithm + RAG cycle detection
│   ├── comms.c / .h        # Crew messaging, emergency broadcast
│   ├── threads.c / .h      # Listener thread + monitor thread
│   ├── signals.c / .h      # SIGUSR1/SIGUSR2 handlers, PID management
│   ├── filestore.c / .h    # CSV read/write operations
│   ├── server.c            # TCP relay server (rigguard_server)
│   ├── socket_common.h     # Shared Packet struct, message types
│   └── utils.c / utils.h   # Global structs, mutex, colors, timestamps
├── data/                   # Auto-created on first run
│   ├── users.csv           # Crew credentials and roles
│   ├── resources.csv       # Equipment allocation state
│   ├── messages.csv        # Crew communication log
│   ├── incidents.csv       # Full audit trail
│   └── pids/               # PID files for signal-based IPC
├── Makefile                # Build system
└── README.md               # This file
```

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

## 🔐 Default Credentials

| Username    | Password   | Role                  |
|-------------|------------|-----------------------|
| commander   | cmd123     | Rig\_Commander (Admin)|
| ravi        | fire123    | Fire\_Safety\_Officer |
| arjun       | drill123   | Drilling\_Operator    |
| meera       | maint123   | Maintenance\_Engineer |
| priya       | comms123   | Comms\_Officer        |

---

## 🧪 Test Scenarios

### Scenario 1 — Normal Resource Allocation
1. Login as `ravi` → Option 3 → Request any free equipment
2. See: MUTEX LOCKED → GRANTED → MUTEX UNLOCKED

### Scenario 2 — Resource Contention (Semaphore blocking)
1. Terminal 1 (ravi): Request `Emergency_Pump`
2. Terminal 2 (arjun): Request `Emergency_Pump` → blocks on semaphore
3. Terminal 1 (ravi): Release `Emergency_Pump`
4. Terminal 2: THREAD AWAKE → RESOURCE GRANTED

### Scenario 3 — Deadlock Detection (Main Demo)
1. Terminal 1 (ravi): Request `Fire_Suppression`
2. Terminal 2 (arjun): Request `Emergency_Pump`
3. Terminal 3 (meera): Request `Pressure_Valve`
4. Terminal 1 (ravi): Request `Emergency_Pump` → BLOCKS
5. Terminal 2 (arjun): Request `Pressure_Valve` → BLOCKS
6. Terminal 3 (meera): Request `Fire_Suppression` → BLOCKS
7. Terminal 4 (commander): Option 6 → DEADLOCK DETECTED + Recovery

### Scenario 4 — Emergency Broadcast
1. Any terminal: Option 7 → Type message
2. All other terminals: Receive alert via socket + SIGUSR1 signal

### Scenario 5 — Commander Admin Features
1. Login as `commander`
2. Option 10: Register new crew member
3. Option 11: Add new equipment
4. Option 12: Unlock a locked account

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
