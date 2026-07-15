class Document {
    readDocument(docId) {
        throw new Error("Not implemented");
    }
}

class RealDocument extends Document {
    readDocument(docId) {
        console.log("reading the actual document from DB"); // latency heavy task
        return `Content of ${docId}`;
    }
}

class CacheProxy extends Document {
    constructor(realDocument) {
        super();
        this.cache = new Map();
        this.realDocument = realDocument;
    }

    readDocument(docId) {
        // Now things in my control, do whatever u want to do here
        if (this.cache.has(docId)) {
            return this.cache.get(docId);
        }

        const doc = this.realDocument.readDocument(docId);
        this.cache.set(docId, doc);
        return doc;
    }
}

const proxy = new CacheProxy(new RealDocument());
console.log(proxy.readDocument("doc1"));
console.log(proxy.readDocument("doc1"));
