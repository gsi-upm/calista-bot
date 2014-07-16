/*
 * Copyright 2013 miguel, Javi Herrera Grupo de Sistemas Inteligentes (GSI UPM) 
 *                                         <http://gsi.dit.upm.es>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package es.upm.dit.gsi.siren.demo.gsiweb;


import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Arrays;

import net.sf.json.JSONArray;
import net.sf.json.JSONObject;

import org.apache.commons.io.FileUtils;
import org.apache.lucene.queryparser.flexible.core.QueryNodeException;
import org.apache.lucene.search.Query;
import org.sindice.siren.qparser.keyword.KeywordQueryParser;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import maia.client.MaiaService;


/**
 * Index a set of bibliographical references encoded in JSON and execute various
 * search queries over the JSON data structure.
 * <p>
 * Each search query is written using both the keyword query syntax and the
 * JSON query syntax.
 */
public class WebProjectsDemo {

    private final File indexDir;
    private static final InputStream DATA_PATH = WebProjectsDemo.class
            .getClassLoader().getResourceAsStream("gsiweb/vademecum.json");
    private static final Logger logger = LoggerFactory.getLogger(WebProjectsDemo.class);
    private static final String ROOT_ELEMENT = "items";
    private static final String LABEL_DATA = "label";
    
    
    public WebProjectsDemo(final File indexDir) {
        this.indexDir = indexDir;
        if (indexDir.exists()) {
            logger.error("Existing directory {} - aborting", indexDir);
            System.exit(1);
        }
        logger.info("Creating index directory {}", indexDir);
        indexDir.mkdirs();
    }

    
    public void index() throws IOException {
        // Read json file (one or more lines)
        BufferedReader br = new BufferedReader(new InputStreamReader(DATA_PATH));
        String json = "";
        while(br.ready()) {
            json += br.readLine();
        }
        br.close();

        // Extract array of items, they will be the documents
        JSONObject jsonObject = JSONObject.fromObject(json);
        JSONArray objects = jsonObject.getJSONArray(ROOT_ELEMENT);
        logger.debug("Found {} objects in the file", objects.size());

        // Prepare indexer
        final DemoIndexer indexer = new DemoIndexer(indexDir);
        try {
            int counter = 0;

            // Add documents
            for(Object obj : objects){
                if(!(obj instanceof JSONObject)){
                    logger.warn("JSON format error. Found a not JSONObject when one was expected {}", obj);
                }
                final String id = Integer.toString(counter++);
                JSONObject app = (JSONObject)obj;
                final String content = app.toString();
                String label = app.getString(LABEL_DATA);
                logger.info("Indexing document #{}: {}", id, label);
                indexer.addDocument(id, content);
            }
            logger.info("Commiting all pending documents");
            indexer.commit();
        }
        finally {
            logger.info("Closing index");
            indexer.close();
        }
    }

    
    
    public void localSearch() throws QueryNodeException, IOException {
        final DemoSearcher searcher = new DemoSearcher(indexDir);
        final String[] keywordQueries = this.getKeywordQueries();

        for (int i = 0; i < keywordQueries.length; i++) {
            Query q = searcher.parseKeywordQuery(keywordQueries[i]);
            logger.info("Executing keyword query: '{}'", keywordQueries[i]);
            String[] results = searcher.search(q, 1000);
            logger.info("Keyword query returned {} results: {}", results.length, Arrays.toString(results)+"\n \n \n");


        }

    }
    

    /**
     * Get a list of queries that are based on the keyword query syntax
     *
     * @see KeywordQueryParser
     */
    private String[] getKeywordQueries() {
        final String[] queries = {
				"label : bucles",
				"label : for",
				"label : do while",
				"label : break",
				"label : continue",
				"label : while",
				"label : iteracion"
        };
        return queries;
    }
    

    public static void main(final String[] args) throws IOException {
        logger.info("info");
        final File indexDir = new File("./target/index/");
        if (indexDir.exists()){
            FileUtils.deleteDirectory(indexDir);
        }
        
        final WebProjectsDemo demo = new WebProjectsDemo(indexDir);
        String maiauri = "http://alpha.gsi.dit.upm.es:1337";
        MaiaService mservice;        
        
        try {
            demo.index();
            //demo.localSearch(); 
        

			try {
				mservice = new MaiaService (maiauri);
				mservice.connect();
				mservice.waitUntilConnected();    
	            mservice.subscribe("message");

			} catch (URISyntaxException e) {
				logger.info("URISyntaxException");
				e.printStackTrace();
			} catch (InterruptedException e) {
				logger.info("InterruptedException");
				e.printStackTrace();
			} 

        } catch (final Throwable e) {
            logger.error("Unexpected error", e);
            logger.error(e.getMessage());
        }


    }

}
