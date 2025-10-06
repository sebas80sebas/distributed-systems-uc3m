# P2P File Sharing Distributed System

Final Project - Distributed Systems - Universidad Carlos III de Madrid (2024-25)

## 📋 Description

Client-server system that allows users to register, connect, and share files through direct P2P transfers. The project evolves in three parts, progressively adding temporal logging and distributed logging functionalities.

### Key Features

- ✅ User management (registration, connection, disconnection)
- ✅ Publishing and deletion of shared files
- ✅ Direct P2P transfer between clients (without going through the server)
- ✅ Listing of connected users and published content
- ✅ Temporal logging of all operations
- ✅ Distributed logging system via RPC
- ✅ Concurrency: support for multiple simultaneous clients

## 🏗️ Architecture

```
┌─────────────┐          ┌──────────────────┐          ┌─────────────┐
│ Web Service │◄────HTTP─┤   Main Server    ├─────TCP──►│  Client A   │
│   (Flask)   │          │       (C)        │          │  (Python)   │
└─────────────┘          └────────┬─────────┘          └──────┬──────┘
                                  │                            │
                                  │ RPC                        │ P2P
                                  ▼                            │
                         ┌─────────────┐                       │
                         │ Log Service │                       ▼
                         │    (RPC)    │                 ┌─────────────┐
                         └─────────────┘                 │  Client B   │
                                                         │  (Python)   │
                                                         └─────────────┘
```

## 🚀 Project Parts

### Part 1: Base System
Basic client-server system with user management and P2P file transfer.

**Components:**
- `server.c`: Concurrent server in C (pthreads)
- `client.py`: Interactive client in Python

**Available Operations:**
- `REGISTER <username>` - Register new user
- `UNREGISTER <username>` - Unregister user
- `CONNECT <username>` - Connect to the system
- `DISCONNECT <username>` - Disconnect from the system
- `PUBLISH <file> <description>` - Publish file
- `DELETE <file>` - Delete published file
- `LIST_USERS` - List connected users
- `LIST_CONTENT <username>` - View user's files
- `GET_FILE <user> <remote_file> <local_file>` - Download file via P2P

### Part 2: Temporal Logging
Adds web service to obtain timestamps and record when operations are performed.

**New Components:**
- `web_service.py`: REST service with Flask providing current date/time
- Modifications in client and server to include timestamps

### Part 3: Distributed Logging
Implements an independent RPC service to centralize system logging.

**New Components:**
- `log_service.x`: RPC interface definition
- `log_server`: RPC server that receives and records logs
- `log_client.c`: RPC client integrated into the main server

## 📦 Requirements

### Required Software
```bash
# GCC Compiler
sudo apt-get install gcc

# POSIX Threads Library
sudo apt-get install build-essential

# TI-RPC (for Part 3)
sudo apt-get install libtirpc-dev

# Python 3 and dependencies
sudo apt-get install python3 python3-pip
pip3 install flask requests
```

## 🔧 Compilation

### Part 1 and 2
```bash
gcc -o server server.c -lpthread
```

### Part 3
```bash
# Generate RPC files
rpcgen -C log_service.x

# Compile entire project
make all

# Clean generated files
make clean
```

## ▶️ Execution

### Part 1: Base System

**Terminal 1 - Server:**
```bash
./server -p 8888
```

**Terminal 2+ - Clients:**
```bash
python3 client.py -s localhost -p 8888
```

### Part 2: With Timestamps

**Terminal 1 - Web Service:**
```bash
python3 web_service.py
```

**Terminal 2 - Server:**
```bash
./server -p 8888
```

**Terminal 3+ - Clients:**
```bash
python3 client.py -s localhost -p 8888
```

### Part 3: Complete System

**Terminal 1 - Web Service:**
```bash
python3 web_service.py
```

**Terminal 2 - RPC Log Server:**
```bash
./log_server
```

**Terminal 3 - Main Server:**
```bash
export LOG_RPC_IP=localhost
./server -p 8888
```

**Terminal 4+ - Clients:**
```bash
python3 client.py -s localhost -p 8888
```

## 🧪 Usage Example

```bash
# On Client A
> REGISTER alice
REGISTER OK

> CONNECT alice
CONNECT OK

> PUBLISH /home/alice/document.txt "My document"
PUBLISH OK

# On Client B
> REGISTER bob
REGISTER OK

> CONNECT bob
CONNECT OK

> LIST_USERS
LIST_USERS OK
alice 192.168.1.10 39653
bob 192.168.1.11 55665

> LIST_CONTENT alice
LIST_CONTENT OK
/home/alice/document.txt

> GET_FILE alice /home/alice/document.txt ./downloaded.txt
GET_FILE OK
```

## 🛠️ Technologies

- **Languages:** C, Python 3
- **C Libraries:** pthread (concurrency), POSIX sockets
- **Python Libraries:** socket, threading, requests, flask
- **Protocols:** TCP/IP, HTTP, RPC/XDR
- **Tools:** rpcgen, gcc, make

## 📊 Test Cases

The project includes a complete test suite that verifies:
- ✅ User registration (duplicates, usernames in use)
- ✅ Concurrent connections
- ✅ File publishing (duplicates, permissions)
- ✅ Successful and failed P2P transfers
- ✅ Operations without authentication
- ✅ Error and exception handling

See `memoria.pdf` for complete details of tests performed.

## 🔐 Technical Features

### Server
- Concurrent programming with pthreads
- Synchronization using mutexes to protect shared structures
- Robust signal handling (SIGINT for clean shutdown)
- Dynamic memory management

### Client
- Socket listener in separate thread to receive P2P requests
- Interactive command-line interface
- Error handling and reconnection

### Security and Robustness
- Input validation
- Access control (operations only for connected users)
- Handling of unexpected disconnections
- Race condition prevention

## 👥 Authors

- Noelia Martínez López
- Iván Sebastián Loor Weir

**Universidad Carlos III de Madrid**  
Computer Engineering - Distributed Systems (2024-25)

## 📄 License

Academic project - Universidad Carlos III de Madrid

---

**Note:** This project was developed for educational purposes as part of the Distributed Systems course.
