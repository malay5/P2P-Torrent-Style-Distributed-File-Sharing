
---

# 🔷 Distributed Peer-to-Peer File Sharing System

A **robust, scalable, and fault-tolerant peer-to-peer (P2P) file sharing framework** implemented in **C/C++**.
The system uses **TCP-based socket communication**, **multi-threading**, and **system-level constructs** to enable high-throughput, verified, and distributed file exchange among peers.

The architecture is designed around **multi-tracker coordination**, **chunk-level parallelism**, and **integrity validation**, ensuring consistent availability and strong resilience to node and tracker failures.

---

## 📘 Abstract

This project presents a hybrid P2P model combining centralized metadata management with decentralized file distribution.
Trackers maintain global metadata, while peers directly exchange file segments.
The system supports:

* Secure user and group authentication
* Multi-source parallel downloads
* SHA1-based integrity validation
* On-demand serving of file pieces
* Multi-tracker metadata synchronization

The result is a **high-availability distributed system** suitable for real-world file sharing scenarios.

---

## ✔️ Key Features

### 1️⃣ Secure User Authentication Layer

* User credentials hashed using **SHA1**.
* Tracker-managed session lifecycle.
* Prevents unauthorized access to shared resources.

---

### 2️⃣ Group-Based Access Control

* Logical separation of shared content via groups.
* Group admins manage membership requests.
* Strong permission enforcement for file visibility and sharing.

---

### 3️⃣ Distributed File Sharing Workflow

* Files split into **512 KB chunks** for efficient distribution.
* Users upload file metadata (path + per-chunk hashes) to the tracker.
* Peers download chunks concurrently from multiple sources.
* Immediate re-sharing of downloaded chunks promotes decentralization.

App-level guarantees include:

✔ High transfer speeds
✔ Load distribution across peers
✔ Successful reconstruction with verified integrity

---

### 4️⃣ Load-Aware Piece Selection Algorithm

* Dynamically computes peer load.
* Prioritizes chunk download from least-loaded peers.
* Avoids network hotspots and increases overall throughput.

---

### 5️⃣ Multi-Tracker Robustness

* Multiple trackers operate in parallel.
* Continuous metadata synchronization through TCP.
* Auto-recovery and full resync when a tracker rejoins after downtime.
* Provides strong fault tolerance and high system availability.

---

### 6️⃣ Multi-Threaded Peer Engine

Each client node internally spawns:

* A **server thread** to serve chunk requests.
* Multiple **worker threads** for parallel chunk downloads.
* Independent communication handlers for both peer and tracker modules.

This ensures scalability under heavy workloads and minimizes transfer latency.

---

## 🖼️ System Architecture

<p align="center">
  <img src="./p2pimage.png" width="480" alt="Architecture Diagram">
</p>

---

## ⚙️ Build & Execution

### ▶️ Run Trackers

```bash
make
./tracker tracker_info.txt <tracker_id>

# Example:
./tracker tracker_info.txt 1
```

`tracker_info.txt` specifies the IP and port of each tracker.

---

### ▶️ Run Clients

```bash
make
./client <IP>:<PORT> tracker_info.txt

# Example:
./client 127.0.0.1:6000 tracker_info.txt
```

---

## 📚 Supported Commands

| Command                                              | Description                      |
| ---------------------------------------------------- | -------------------------------- |
| `create_user <user_id> <passwd>`                     | Create a new account             |
| `login <user_id> <passwd>`                           | Authenticate user                |
| `create_group <group_id>`                            | Create a sharing group           |
| `join_group <group_id>`                              | Request to join group            |
| `leave_group <group_id>`                             | Leave an existing group          |
| `list_requests <group_id>`                           | View pending join requests       |
| `accept_request <group_id> <user_id>`                | Approve a join request           |
| `reject_request <group_id> <user_id>`                | Reject a membership request      |
| `list_groups`                                        | List all groups                  |
| `list_my_groups`                                     | List groups the user belongs to  |
| `list_files <group_id>`                              | List files shared within a group |
| `upload_file <filepath> <group_id>`                  | Upload file metadata             |
| `download_file <group_id> <file_name> <destination>` | Download a file                  |
| `show_downloads`                                     | Display all downloads            |
| `stop_share <group_id> <file_name>`                  | Stop sharing a file              |
| `logout`                                             | Logout current user              |
| `exit`                                               | Terminate the client             |

---

## 🧪 Testing & Validation

The system has been tested under:

* Multi-peer parallel downloads
* Sudden peer shutdowns
* Tracker failure and recovery
* Artificial latency and network disturbance
* Consistency checks across multiple trackers

Results confirm:

✔ Strong fault tolerance
✔ No metadata inconsistency
✔ Graceful degradation under partial failures

---

## 📈 Future Enhancements

* TLS-based encrypted peer and tracker communication
* Leader election for tracker decentralization
* Visual dashboard for monitoring transfers and cluster health
* Adaptive chunking for dynamic network conditions
* Auto-resume support for interrupted downloads

---

## 👤 Author

**Malay Damani**
**Tech Stack:** C++, TCP/IP Sockets, POSIX Threads, Distributed Systems
🔗 **LinkedIn:** [https://www.linkedin.com/in/malay-damani9126hacks/](https://www.linkedin.com/in/malay-damani9126hacks/)

---
