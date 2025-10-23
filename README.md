# Virtual Memory Simulator

A modular **virtual memory management simulator** in C, implementing the **Flush-When-Full (FWF)** page replacement algorithm.
Built using **POSIX shared memory** and **named semaphores** for inter-process communication.

## Overview

The simulator consists of three processes:

- `main` – sets up shared memory and semaphores, forks other processes.
- `memory_manager` – handles page requests, applies FWF, collects statistics.
- `pm` – replays trace files and sends memory access requests.

Communication between processes is achieved via a **shared circular buffer** and semaphores implementing a **producer-consumer** model.

## Build

```bash
make
```

Creates three executables: `main`, `memory_manager`, and `pm`.

## Usage

```bash
./main k total_frames q trace1 trace2 [--debug]
```

| Parameter          | Description                                          |
| ------------------ | ---------------------------------------------------- |
| `k`                | Page fault threshold before a flush (FWF parameter)  |
| `total_frames`     | Total physical frames (split equally per process)    |
| `q`                | Number of requests each PM issues before alternating |
| `trace1`, `trace2` | Memory trace files (one per PM)                      |
| `--debug`          | Optional verbose mode (disables progress bar)        |

### Example

```bash
./main 10 256 5000 traces/bzip.trace traces/gcc.trace
```
