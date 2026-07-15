package DesignPatterns;

import java.io.CharArrayReader;
import java.util.HashMap;

interface Document{
    String readDocument(String docId);
}

class RealDocument implements Document{
    @Override
    public String readDocument(String docId) {
        System.out.println("reading the actual document from DB"); // latency heavy task
    }
}

class CacheProxy implements Document{
    HashMap<String,String> cache;
    Document realDocument;
    public CacheProxy(Document realDocument){
        cache = new HashMap<>();
        this.realDocument = realDocument;
    }
    @Override
    public String readDocument(String docId) {
        //Now things in my control, do whatever u want to do here 
        if(cache.containsKey(docId)){
            return cache.get(docId);
        }

        String doc = realDocument.readDocument(docId);
        cache.put(docId, doc);
        return doc;
    }

}


public class ProxyDesign_pattern {
    
}
