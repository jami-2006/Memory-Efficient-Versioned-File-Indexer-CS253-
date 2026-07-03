# CS253 Course Project: Memory Efficient Versioned File Indexer

This repository contains my course project for **CS253 (Software Development & Operations)**. The goal of the project was to build a versioned file indexing system in C++ that can operate under extremely tight memory constraints (specifically a strict **256–1024 KB** fixed-size memory buffer).

---

## What It Does

The indexer reads massive text files in small chunks, builds a per-version word-frequency index without blowing up the heap, and answers specific analytical queries:

* **Word count** across different file versions.
* **Top-K** most frequent words.
* **Version difference comparisons** (diff queries).

---

## Technical Details

I structured the codebase using proper object-oriented principles. You'll find heavy use of **inheritance**, **runtime polymorphism**, and **C++ templates** in the source code to keep things clean and reusable. I also added proper **exception handling** to make sure the program doesn't crash if it hits a bad query or a corrupted file chunk.

---

## Repository Structure

* `src/VersionedFileIndexer.cpp`: The core C++ source code.
* `docs/Project_Report.pdf`: The final report detailing the memory management strategies, data structures used, and benchmarking results.
