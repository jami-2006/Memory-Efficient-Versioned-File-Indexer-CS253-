#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <cctype>
#include <sstream>

class BufferedReader {
private:
    std::ifstream  fileStream;
    std::vector<char> buffer;      // reused buffer allocated once
    size_t  bufferBytes;    // buffer capacity in bytes

public:
    // Constructor: validate buffer size, open file, allocate buffer
    BufferedReader(const std::string& filePath, size_t bufferSizeKB) {
        if (bufferSizeKB < 256 || bufferSizeKB > 1024)
            throw std::invalid_argument(
                "Buffer size must be between 256 KB and 1024 KB. Got: "
                + std::to_string(bufferSizeKB) + " KB");

        bufferBytes = bufferSizeKB * 1024;
        buffer.resize(bufferBytes);   // one-time heap allocation

        fileStream.open(filePath, std::ios::binary);
        if (!fileStream.is_open())
            throw std::runtime_error("Cannot open file: " + filePath);
    }

    ~BufferedReader() {
        if (fileStream.is_open()) fileStream.close();
    }

    // read chunk fill the internal buffer; return number of bytes actually read.
    // Returns 0 only when the file is exhausted.
    size_t readChunk() {
        fileStream.read(buffer.data(), bufferBytes);
        return static_cast<size_t>(fileStream.gcount());
    }

    bool isEOF()          const { return fileStream.eof(); }
    const char* data()           const { return buffer.data(); }
    size_t  capacity()       const { return bufferBytes; }
    size_t getBufferSizeKB()const { return bufferBytes / 1024; }
};



// Tokenizer extracts clean, lowercase, alphanumeric words
//   from a byte buffer.

// The challenge: a word can span two consecutive buffers.

// Solution: keep a `leftover` string.
//   At the end of every chunk, if we are in the middle of a word
//     we save it in `leftover` instead of emitting it.
//   The next call to tokenize() prepends `leftover` to its scan.
//   On the very last chunk (isLast == true) we flush whatever
//     remains.
class Tokenizer {
private:
    std::string leftover;   // partial word carried over from previous chunk

public:
    Tokenizer() : leftover("") {}

    // Overloaded normalize 
    static std::string normalize(const std::string& word) {
        std::string result;
        result.reserve(word.size());
        for (unsigned char c : word)
            if (std::isalnum(c))
                result += static_cast<char>(std::tolower(c));
        return result;
    }

    // Overloaded normalize 
    static char normalize(char c) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            return static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        return '\0';   // sentinel
    }

    // buffer :raw bytes for this chunk
    // length : how many bytes are valid in buffer
    // isLast: true when this is the final chunk of the file
    std::vector<std::string> tokenize(const char* buffer,
                                      size_t      length,
                                      bool        isLast) {
        std::vector<std::string> words;
        std::string current = leftover;   // resume any partial word
        leftover.clear();

        for (size_t i = 0; i < length; ++i) {
            char nc = normalize(buffer[i]);   // normalize single char

            if (nc != '\0') {
                current += nc;             // alphanumeric → grow token
            } else {
                if (!current.empty()) {    // delimiter → emit token
                    words.push_back(current);
                    current.clear();
                }
            }
        }

        // End of this chunk
        if (!current.empty()) {
            if (isLast)
                words.push_back(current);  // nothing more to read
            else
                leftover = current;        // might continue in next chunk
        }

        return words;
    }

    void reset() { leftover.clear(); }
};


//VersionIndex stores and query the word-frequency map for one
//   named version of a file.

// Function overloading is on getFrequency():
//   getFrequency(word) returns 0 if absent
//  getFrequency(word, defaultVal) returns defaultVal if absent
class VersionIndex {
private:
    std::string versionName;
    std::unordered_map<std::string,int> index;       // word → count
    long long  totalWords;

public:
    explicit VersionIndex(const std::string& name)
        : versionName(name), totalWords(0) {}

    // Called once per word while building the index
    void addWord(const std::string& word) {
        ++index[word];
        ++totalWords;
    }

    // Overload 1 — no default; missing words return 0
    int getFrequency(const std::string& word) const {
        auto it = index.find(word);
        return (it != index.end()) ? it->second : 0;
    }

    // Overload 2 — caller supplies fallback value
    int getFrequency(const std::string& word, int defaultVal) const {
        auto it = index.find(word);
        return (it != index.end()) ? it->second : defaultVal;
    }

    const std::unordered_map<std::string,int>& getIndex()      const { return index; }
    const std::string&                          getVersionName()const { return versionName; }
    long long                                   getTotalWords() const { return totalWords; }
    size_t                                      uniqueWords()   const { return index.size(); }
};


//Template for findTopK
// A generic function template that works with any unordered_map
// whose value type supports operator>.
// Uses std::partial_sort so we only fully sort the K elements we need, not the entire map — O(N log K) instead of O(N log N).
template<typename KeyType, typename ValueType>
std::vector<std::pair<KeyType, ValueType>>
findTopK(const std::unordered_map<KeyType, ValueType>& map, int k) {
    // Copy map entries into a sortable vector
    std::vector<std::pair<KeyType, ValueType>> items(map.begin(), map.end());

    // Clamp k to the actual number of entries
    if (k > static_cast<int>(items.size()))
        k = static_cast<int>(items.size());

    // Partial sort: only guarantee the first k positions are correct
    std::partial_sort(
        items.begin(),
        items.begin() + k,
        items.end(),
        [](const std::pair<KeyType,ValueType>& a,
           const std::pair<KeyType,ValueType>& b) {
            return a.second > b.second;   // descending by frequency
        });

    items.resize(k);
    return items;
}


// STEP 5 — Abstract Base Class: Query
// Pure virtual interface that every query type must satisfy.
// This enables runtime polymorphism
//
// Two pure virtual methods force all derived classes to: execute()  and printResult()

class Query {
protected:
    std::string queryType;

public:
    explicit Query(const std::string& type) : queryType(type) {}
    virtual ~Query() = default;

    virtual void execute()          = 0;   // must be overridden
    virtual void printResult() const = 0;  // must be overridden

    std::string getType() const { return queryType; }
};


// Looks up a single word in one VersionIndex and stores the count.
class WordQuery : public Query {
private:
    const VersionIndex& vindex;
    std::string     word;
    int   result = 0;

public:
    WordQuery(const VersionIndex& vi, const std::string& w)
        : Query("word"), vindex(vi), word(w) {}

    void execute() override {
        result = vindex.getFrequency(word);   // calls overload 1
    }

    void printResult() const override {
        std::cout << "Version   : " << vindex.getVersionName() << "\n";
        std::cout << "Query     : word\n";
        std::cout << "Word      : " << word << "\n";
        std::cout << "Frequency : " << result << "\n";
    }
};


// Uses the findTopK<> template to pull top-K words by frequency.
class TopKQuery : public Query {
private:
    const VersionIndex& vindex;
    int  k;
    std::vector<std::pair<std::string,int>> results;

public:
    TopKQuery(const VersionIndex& vi, int topK)
        : Query("top"), vindex(vi), k(topK) {}

    void execute() override {
        // Instantiates the template with <string, int>
        results = findTopK(vindex.getIndex(), k);
    }

    void printResult() const override {
        std::cout << "Version   : " << vindex.getVersionName() << "\n";
        std::cout << "Query     : top-" << k << "\n";
        std::cout << "Results   :\n";
        for (int i = 0; i < static_cast<int>(results.size()); ++i)
            std::cout << "  " << (i + 1) << ". "
                      << results[i].first << " -> "
      << results[i].second << "\n";
    }
};


// Compares the frequency of one word between two VersionIndexes.
class DiffQuery : public Query {
private:
    const VersionIndex& vindex1;
    const VersionIndex& vindex2;
    std::string         word;
    int freq1 = 0, freq2 = 0, diff = 0;

public:
    DiffQuery(const VersionIndex& vi1,
              const VersionIndex& vi2,
              const std::string&  w)
        : Query("diff"), vindex1(vi1), vindex2(vi2), word(w) {}

    void execute() override {
        freq1 = vindex1.getFrequency(word, 0);   // overload 2
        freq2 = vindex2.getFrequency(word, 0);   // overload 2
        diff  = freq1 - freq2;
    }

    void printResult() const override {
        std::cout << "Version 1  : " << vindex1.getVersionName() << "\n";
        std::cout << "Version 2  : " << vindex2.getVersionName() << "\n";
        std::cout << "Query      : diff\n";
        std::cout << "Word       : " << word << "\n";
        std::cout << "Freq in "  << vindex1.getVersionName()
          << "  : " << freq1 << "\n";
        std::cout << "Freq in "  << vindex2.getVersionName()
                  << "  : " << freq2 << "\n";
        std::cout << "Difference : " << diff
          << (diff > 0 ? "  (more in v1)"
           : diff < 0 ? "  (more in v2)"
     : "  (equal)") << "\n";
    }
};


//QueryProcessor orchestrate the full pipeline for any query type.
//   Create a BufferedReader for the file(s)
//   read chunk then tokenize then insert into VersionIndex
//   Instantiate the correct Query subclass
//   Call execute() then printResult() via the base-class pointer
//      (this is the runtime polymorphism / dynamic dispatch)
class QueryProcessor {
private:
    size_t bufferSizeKB;

    // Internal helper: reads an entire file into a VersionIndex
    VersionIndex buildIndex(const std::string& filePath,
                            const std::string& version) {
        BufferedReader reader(filePath, bufferSizeKB);
        Tokenizer      tokenizer;
        VersionIndex   vindex(version);

        while (true) {
            size_t bytes = reader.readChunk();
            if (bytes == 0) break;

            bool isLast = (bytes < reader.capacity()) || reader.isEOF();

            // Tokenize this chunk; words may straddle chunk boundaries
            std::vector<std::string> words =
                tokenizer.tokenize(reader.data(), bytes, isLast);

            for (const auto& w : words)
                vindex.addWord(w);

            if (isLast) break;
        }

        std::cout << "  [Indexed] version=" << version
                  << "  unique_words=" << vindex.uniqueWords()
                  << "  total_words=" << vindex.getTotalWords() << "\n";

        return vindex;
    }

public:
    explicit QueryProcessor(size_t bufKB) : bufferSizeKB(bufKB) {}

    void runWordQuery(const std::string& file,
                      const std::string& version,
         const std::string& word) {
        VersionIndex vindex = buildIndex(file, version);
        // Polymorphic pointer — concrete type is WordQuery
        std::unique_ptr<Query> q =
            std::make_unique<WordQuery>(vindex,
            Tokenizer::normalize(word));
        q->execute();
        q->printResult();
    }

    void runTopKQuery(const std::string& file,
      const std::string& version,
      int k) {
        if (k <= 0)
            throw std::invalid_argument("--top value must be positive");
        VersionIndex vindex = buildIndex(file, version);
        std::unique_ptr<Query> q =
        std::make_unique<TopKQuery>(vindex, k);
        q->execute();
        q->printResult();
    }

    void runDiffQuery(const std::string& file1,
         const std::string& version1,
        const std::string& file2,
                      const std::string& version2,
           const std::string& word) {
        VersionIndex v1 = buildIndex(file1, version1);
        VersionIndex v2 = buildIndex(file2, version2);
        std::unique_ptr<Query> q =
            std::make_unique<DiffQuery>(v1, v2,
            Tokenizer::normalize(word));
        q->execute();
        q->printResult();
    }
};



// Argument generation
// Simple linear scan of argv[] looking for --key value pairs.
// Wraps std::stoi in try/catch to produce friendly error messages.
struct Args {
    std::string file, file1, file2;
    std::string version, version1, version2;
    std::string query, word;
    int bufferKB = 512;
    int topK     = 10;
};

Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc - 1; ++i) {
        std::string key = argv[i];
        std::string val = argv[i + 1];
        if      (key == "--file")     a.file     = val;
        else if (key == "--file1")    a.file1    = val;
        else if (key == "--file2")    a.file2    = val;
        else if (key == "--version")  a.version  = val;
        else if (key == "--version1") a.version1 = val;
        else if (key == "--version2") a.version2 = val;
        else if (key == "--query")    a.query    = val;
        else if (key == "--word")     a.word     = val;
        else if (key == "--buffer") {
        try { a.bufferKB = std::stoi(val); }
            catch (...) {
            throw std::invalid_argument(
        "--buffer must be an integer, got: " + val);
            }
        }
        else if (key == "--top") {
            try { a.topK = std::stoi(val); }
            catch (...) {
                throw std::invalid_argument(
                    "--top must be an integer, got: " + val);
            }
        }
    }
    return a;
}


//starts timer, validates and dispatches to the right QueryProessor method
int main(int argc, char* argv[]) {
    auto t0 = std::chrono::high_resolution_clock::now();

    try {
        if (argc < 2)
            throw std::invalid_argument(
                "Usage: ./analyzer --file <path> --version <name> "
                "--buffer <kb> --query word|top|diff [--word <w>] [--top <k>]");

        Args args = parseArgs(argc, argv);

        // Validate buffer range
        if (args.bufferKB < 256 || args.bufferKB > 1024)
            throw std::invalid_argument(
                "Buffer size must be 256–1024 KB, got: "
                + std::to_string(args.bufferKB));

        if (args.query.empty())
            throw std::invalid_argument(
                "--query is required (word | top | diff)");

        std::cout << "============================\n";
        std::cout << "Buffer Size : " << args.bufferKB << " KB\n";
        std::cout << "Query Type  : " << args.query << "\n";
        std::cout << "============================\n";

        QueryProcessor processor(static_cast<size_t>(args.bufferKB));

        if (args.query == "word") {
            if (args.file.empty() || args.version.empty() || args.word.empty())
                throw std::invalid_argument(
                    "word query needs: --file --version --word");
            processor.runWordQuery(args.file, args.version, args.word);

        } else if (args.query == "top") {
            if (args.file.empty() || args.version.empty())
                throw std::invalid_argument(
                    "top query needs: --file --version [--top k]");
            processor.runTopKQuery(args.file, args.version, args.topK);

        } else if (args.query == "diff") {
            if (args.file1.empty() || args.file2.empty()   ||
                args.version1.empty() || args.version2.empty() ||
                args.word.empty())
                throw std::invalid_argument(
                    "diff query needs: --file1 --file2 --version1 --version2 --word");
            processor.runDiffQuery(args.file1, args.version1,
                                   args.file2, args.version2,
                                   args.word);

        } else {
            throw std::invalid_argument(
                "Unknown query type: '" + args.query
                + "'. Use word | top | diff");
        }

    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n";
        return 1;
    }

    auto t1  = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = t1 - t0;

    std::cout << "============================\n";
    std::cout << "Execution Time : " << elapsed.count() << " seconds\n";

    return 0;
}