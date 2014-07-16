package es.upm.dit.gsi.siren.demo.gsiweb;

import java.io.File;
import java.io.IOException;
import java.io.Reader;

import org.apache.lucene.analysis.Analyzer;
import org.apache.lucene.analysis.TokenStream;
import org.apache.lucene.analysis.core.LowerCaseFilter;
import org.apache.lucene.analysis.core.SimpleAnalyzer;
import org.apache.lucene.analysis.standard.StandardAnalyzer;
import org.apache.lucene.codecs.Codec;
import org.apache.lucene.codecs.PostingsFormat;
import org.apache.lucene.codecs.lucene40.Lucene40Codec;
import org.apache.lucene.codecs.lucene40.Lucene40PostingsFormat;
import org.apache.lucene.document.Document;
import org.apache.lucene.document.Field;
import org.apache.lucene.document.FieldType;
import org.apache.lucene.document.Field.Store;
import org.apache.lucene.document.StringField;
import org.apache.lucene.index.IndexWriter;
import org.apache.lucene.index.IndexWriterConfig;
import org.apache.lucene.store.Directory;
import org.apache.lucene.store.FSDirectory;
import org.apache.lucene.util.Version;
import org.sindice.siren.analysis.IntNumericAnalyzer;
import org.sindice.siren.analysis.JsonTokenizer;
import org.sindice.siren.analysis.filter.DatatypeAnalyzerFilter;
import org.sindice.siren.analysis.filter.PositionAttributeFilter;
import org.sindice.siren.analysis.filter.SirenPayloadFilter;
import org.sindice.siren.index.codecs.siren10.Siren10AForPostingsFormat;

/**
 * 
 * Class to configure the siren codec to import json data
 * 
 * 
 * Based on the "SimpleIndexer" class, from the siren-demo
 * at https://github.com/rdelbru/SIREn
 * 
 * @author Alberto Mardomingo
 *
 */
public class DemoIndexer {
    
    private Directory dir;
    private IndexWriter indexwriter;
    
    public static final String ID_FIELD = "id"; 
    public static final String SIREN_FIELD = "siren-field";
    public static final String CONTENT_FIELD = "content";

    
    /**
     * Constructor
     * 
     * @param path - The path to the index directory
     * @throws IOException
     */
    public DemoIndexer(File path) throws IOException{
        this.dir = FSDirectory.open(path);
        this.indexwriter = this.initializeIndexWriter();
    }
    
    
    public void close() throws IOException {
        indexwriter.close();
        dir.close();
    }

    
    public void addDocument(String id, String json) throws IOException{
        Document doc = new Document();
        
        doc.add(new StringField(ID_FIELD, id, Store.YES));
        doc.add(new StringField(CONTENT_FIELD, json, Store.YES));
        
        FieldType sirenFieldType = new FieldType();
        sirenFieldType.setIndexed(true);
        sirenFieldType.setTokenized(true);
        sirenFieldType.setOmitNorms(true);
        sirenFieldType.setStored(false);
        sirenFieldType.setStoreTermVectors(true);

        doc.add(new Field(SIREN_FIELD, json, sirenFieldType));
        
        this.indexwriter.addDocument(doc);
    }
    

    public void commit() throws IOException {
        indexwriter.commit();
    }
    
    /**
     * Initializes the IndexWtriter, with Lucene 4.0
     * and the Siren10 example codec 
     * 
     * @return IndexWriter - The new index writer
     * @throws IOException
     */
    private IndexWriter initializeIndexWriter() throws IOException {
        IndexWriterConfig config = new IndexWriterConfig(Version.LUCENE_40,
                this.initializeAnalyzer());
        
        config.setCodec( new Siren10Codec());
        
        return new IndexWriter(this.dir, config);
    }
    
    /**
     * Creates the analizer for the IndexWriter
     * 
     */
    private Analyzer initializeAnalyzer() {
        return new Analyzer() {

            @Override
            protected TokenStreamComponents createComponents(String fieldName,
                    Reader reader) {
                 Version matchVersion = Version.LUCENE_40;
                 JsonTokenizer tokenizer = new JsonTokenizer(reader);
                 DatatypeAnalyzerFilter daf = new DatatypeAnalyzerFilter(matchVersion,
                                        tokenizer, new StandardAnalyzer(matchVersion),
                                        new StandardAnalyzer(matchVersion));
                 daf.register("http://www.w3.org/2001/XMLSchema#boolean".toCharArray(), new SimpleAnalyzer(matchVersion));
                 daf.register("http://www.w3.org/2001/XMLSchema#long".toCharArray(), new IntNumericAnalyzer(1));
                 
                 TokenStream tok = new LowerCaseFilter(matchVersion, daf);
                 
                 // The PositionAttributeFilter and SirenPayloadFilter are mandatory
                 // and must be always the last filters in the token stream
                 tok = new PositionAttributeFilter(tok);
                 tok = new SirenPayloadFilter(tok);
                 return new TokenStreamComponents(tokenizer, tok);
            }
            
        };
    }
    
    /**
     * Simple example of a SIREn codec that will use the SIREn posting format
     * for a given field.
     */
    private class Siren10Codec extends Lucene40Codec {

        final PostingsFormat lucene40 = new Lucene40PostingsFormat();
        PostingsFormat defaultTestFormat = new Siren10AForPostingsFormat();

        public Siren10Codec() {
            Codec.setDefault(this);
        }

        @Override
        public PostingsFormat getPostingsFormatForField(final String field) {
            if (field.equals(SIREN_FIELD)) {
                return defaultTestFormat;
            }
            else {
                return lucene40;
            }
        }

        @Override
        public String toString() {
            return "Siren10Codec[" + defaultTestFormat.toString() + "]";
        }

    }
    
}
