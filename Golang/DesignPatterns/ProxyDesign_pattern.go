package main

import "fmt"

type Document interface {
	ReadDocument(docId string) string
}

type RealDocument struct{}

func (r *RealDocument) ReadDocument(docId string) string {
	fmt.Println("reading the actual document from DB") // latency heavy task
	return "Content of " + docId
}

type CacheProxy struct {
	cache        map[string]string
	realDocument Document
}

func NewCacheProxy(realDocument Document) *CacheProxy {
	return &CacheProxy{
		cache:        make(map[string]string),
		realDocument: realDocument,
	}
}

func (p *CacheProxy) ReadDocument(docId string) string {
	// Now things in my control, do whatever u want to do here
	if doc, ok := p.cache[docId]; ok {
		return doc
	}

	doc := p.realDocument.ReadDocument(docId)
	p.cache[docId] = doc
	return doc
}

func main() {
	proxy := NewCacheProxy(&RealDocument{})
	fmt.Println(proxy.ReadDocument("doc1"))
	fmt.Println(proxy.ReadDocument("doc1"))
}
