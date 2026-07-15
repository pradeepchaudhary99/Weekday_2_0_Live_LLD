#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

struct Document {
    virtual std::string readDocument(const std::string& docId) = 0;
    virtual ~Document() = default;
};

class RealDocument : public Document {
public:
    std::string readDocument(const std::string& docId) override {
        std::cout << "reading the actual document from DB" << std::endl; // latency heavy task
        return "Content of " + docId;
    }
};

class CacheProxy : public Document {
private:
    std::unordered_map<std::string, std::string> cache;
    std::unique_ptr<Document> realDocument;

public:
    explicit CacheProxy(std::unique_ptr<Document> realDocument)
        : realDocument(std::move(realDocument)) {}

    std::string readDocument(const std::string& docId) override {
        // Now things in my control, do whatever u want to do here
        auto it = cache.find(docId);
        if (it != cache.end()) {
            return it->second;
        }

        std::string doc = realDocument->readDocument(docId);
        cache[docId] = doc;
        return doc;
    }
};

int main() {
    CacheProxy proxy(std::make_unique<RealDocument>());
    std::cout << proxy.readDocument("doc1") << std::endl;
    std::cout << proxy.readDocument("doc1") << std::endl;
    return 0;
}
