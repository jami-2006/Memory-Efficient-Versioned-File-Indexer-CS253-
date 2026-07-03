# CS253 Assignment 1 - Memory Efficient Versioned File Indexer

Name: Jashan
Roll Number: 240488
Submission Date: March 6, 2026


## What this program does

This program reads a large text file and counts how many times each word appears.
It does this without loading the whole file into memory. Instead it reads the file
in small chunks(256-1024KB buffers) using a fixed size buffer. After building the word count it can
answer three types of queries in a memory efficient way.


## How to compile

g++ -std=c++17 -O2 -o analyzer 240488_Jashan.cpp


## How to run

Word query - find how many times a word appears:

./analyzer.exe --file dataset_v1.txt --version v1 --buffer 512 --query word --word error

Top K query - find the most frequent words:

./analyzer.exe --file dataset_v1.txt --version v1 --buffer 512 --query top --top 10

Diff query - compare a word between two files:

./analyzer.exe --file1 dataset_v1.txt --version1 v1 --file2 dataset_v2.txt --version2 v2 --buffer 512 --query diff --word error

Note: If you are using linux instead of windows replace ".analyzer.exe" with ".analyzer"


## Arguments

--file        path to input file
--file1       path to first file, used in diff query
--file2       path to second file, used in diff query
--version     name for the version
--version1    name for first version, used in diff query
--version2    name for second version, used in diff query
--buffer      buffer size in KB, must be between 256 and 1024
--query       type of query: word, top, or diff
--word        the word to search for
--top         how many top words to show


## Classes

BufferedReader - reads the file one chunk at a time using a fixed buffer.
Memory stays constant no matter how large the file is.

Tokenizer - extracts words from raw bytes and converts them to lowercase.
Handles words that are split across two buffer chunks using a leftover string.

VersionIndex - stores the word frequency map for one version of a file.

QueryProcessor - runs the full pipeline. builds the index and runs the query.

Query - abstract base class. WordQuery, TopKQuery, and DiffQuery inherit from it.


## C++ features used

- Abstract base class and inheritance
- Runtime polymorphism using virtual functions
- Function overloading
- Function template
- Exception handling using try catch throw


## Files submitted

240488_Jashan.cpp
240488_Jashan.md
240488_Jashan.pdf
240488_Jashan.jpg(screenshots)
