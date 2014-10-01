package es.upm.dit.gsi.siren.demo.gsiweb;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.core.WhitespaceAnalyzer;
import org.apache.lucene.analysis.standard.StandardAnalyzer;
import org.apache.lucene.document.Document;
import org.apache.lucene.queryparser.flexible.core.QueryNodeException;
import org.apache.lucene.search.IndexSearcher;
import org.apache.lucene.search.Query;
import org.apache.lucene.search.ScoreDoc;
import org.apache.lucene.search.SearcherManager;
import org.apache.lucene.search.TopDocs;
import org.apache.lucene.store.Directory;
import org.apache.lucene.store.FSDirectory;
import org.apache.lucene.util.Version;
import org.sindice.siren.qparser.json.JsonQueryParser;
import org.sindice.siren.qparser.keyword.KeywordQueryParser;
import org.sindice.siren.util.JSONDatatype;
import org.sindice.siren.util.XSDDatatype;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


/**
 * 
 * @author Alberto Mardomingo
 *
 */
public class DemoSearcher {
    
    private Directory dir;
    
    private SearcherManager manager;
    
    private Logger logger;

    
    /**
     * Constructor
     * 
     * @param path
     * @throws IOException
     */
    public DemoSearcher(File path, Logger logger) throws IOException{
        
    	this.logger = logger;
        this.dir = FSDirectory.open(path);
        this.manager = new SearcherManager(this.dir, null);
    }
    
    public void close() throws IOException {
        this.manager.close();
        this.dir.close();
    }
    
    public String[] search(final Query q, final int n) throws IOException {
        IndexSearcher searcher = this.manager.acquire();
        try {
            logger.error("q: {}, n: {}", q, n);
            TopDocs docs = searcher.search(q, null, n); // 
            ScoreDoc[] results = docs.scoreDocs;
            final String[] ids = new String[results.length];
            
            for(int i = 0; i< results.length; i++) {
                ids[i] = this.retrieve(results[i].doc).get(DemoIndexer.CONTENT_FIELD);
            }
            
            return ids;
            
        } catch (Exception e) {
            logger.error("{}", e.getClass());
            StackTraceElement[] strace = e.getStackTrace();
            for (StackTraceElement element : strace) {
                logger.error(element.toString());
            }
        } finally {
            this.manager.release(searcher);
            searcher = null;
        }
        return null;
    }
    
    public Document retrieve(final int docID) throws IOException {
        IndexSearcher searcher = this.manager.acquire();   
        Set <String> retrievefields = new HashSet<String>(); 
        retrievefields.add(DemoIndexer.ID_FIELD); 
        retrievefields.add(DemoIndexer.CONTENT_FIELD);

        try {
            return searcher.document(docID, retrievefields); 
        }
        finally {
            this.manager.release(searcher);
            searcher = null;
        }
    }
    
    public Query parseKeywordQuery(final String keywordQuery) throws QueryNodeException {
        final KeywordQueryParser parser = new KeywordQueryParser();
        parser.setDatatypeAnalyzers(this.getDatatypeAnalyzers());
        return parser.parse(keywordQuery, DemoIndexer.SIREN_FIELD);
    }

    public Query parseJsonQuery(final String JsonQuery) throws QueryNodeException {
        final JsonQueryParser parser = new JsonQueryParser();
        final KeywordQueryParser kParser = new KeywordQueryParser();
        kParser.setDatatypeAnalyzers(this.getDatatypeAnalyzers());
        kParser.setAllowTwig(false);
        parser.setKeywordQueryParser(kParser);
        return parser.parse(JsonQuery, DemoIndexer.SIREN_FIELD);
    }

    private Map<String, Analyzer> getDatatypeAnalyzers() {
        final Map<String, Analyzer> analyzers = new HashMap<String, Analyzer>();
        analyzers.put(XSDDatatype.XSD_STRING, new StandardAnalyzer(Version.LUCENE_40));
        analyzers.put(JSONDatatype.JSON_FIELD, new WhitespaceAnalyzer(Version.LUCENE_40));
        return analyzers;
    }

}
