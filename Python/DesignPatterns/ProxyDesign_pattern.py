from abc import ABC, abstractmethod


class Document(ABC):
    @abstractmethod
    def read_document(self, doc_id: str) -> str:
        pass


class RealDocument(Document):
    def read_document(self, doc_id: str) -> str:
        print("reading the actual document from DB")  # latency heavy task
        return f"Content of {doc_id}"


class CacheProxy(Document):
    def __init__(self, real_document: Document) -> None:
        self.cache: dict[str, str] = {}
        self.real_document = real_document

    def read_document(self, doc_id: str) -> str:
        # Now things in my control, do whatever u want to do here
        if doc_id in self.cache:
            return self.cache[doc_id]

        doc = self.real_document.read_document(doc_id)
        self.cache[doc_id] = doc
        return doc


if __name__ == "__main__":
    proxy = CacheProxy(RealDocument())
    print(proxy.read_document("doc1"))
    print(proxy.read_document("doc1"))
