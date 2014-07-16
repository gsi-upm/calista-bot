/*
 * Copyright 2013 miguel, Javier Herrera Grupo de Sistemas Inteligentes (GSI UPM) 
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
package maia.client;


import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;

import maia.client.MaiaClientAdapter;
import maia.client.annotation.OnMessage;
import maia.utils.JSONUtils;

import java.util.Arrays;

import es.upm.dit.gsi.siren.demo.gsiweb.DemoSearcher;
import es.upm.dit.gsi.siren.demo.gsiweb.WebProjectsDemo;

import org.apache.lucene.queryparser.flexible.core.QueryNodeException;
import org.apache.lucene.search.Query;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;



/**
 * <b>Project:</b> maia-client<br />
 * <b>Package:</b> maia.client<br />
 * <b>Class:</b> MaiaClient.java<br />
 * <br />
 * <i>GSI 2013</i><br />
 *
 * @author Miguel Coronado (miguelcb@gsi.dit.upm.es), Javier Herrera
 * @version	Jun 3, 2013
 *
 */

public class MaiaService extends MaiaClientAdapter {
	
	private static final Logger logger = LoggerFactory.getLogger(WebProjectsDemo.class);
    private String username;
    
    
    /**
     * @param serverURI
     * @throws URISyntaxException 
     * @throws RevisionFailedException 
     */
    public MaiaService(String serverURI) throws URISyntaxException {
        super(new URI(serverURI));     
        logger.info("Starting Maia subscription...");
		
    }

    @OnMessage("subscribed")
    public void onSubscribe(String message) {
    	
        logger.info("Subscribed >> " + message + "\n");
        logger.info("Service ready: listening event messages on Maia network");
    }
    
    
    @OnMessage("username::accepted")
    public void getUsername(String message) {
    	
        this.username = JSONUtils.getDataName(message);
        
        
    }
    
    @OnMessage("message")
    public void message(String message) {
        String content = JSONUtils.getDataName(message);
        String origin = JSONUtils.find(message, "origin");
        String response = "";
        String kword = "";
        if (origin != null && origin.equals(this.username)) {
            return;
        }

        logger.info("Message received >> " + content );
        
        
        //Discard the message if not asking to retrieve
        if (!content.contains("retrieve")){
        	return;
        }
        
        //Sample accepted message: [retrieve dowhile]
        
        
        //Extract keyword to search from the Maia message
        //TODO: '+9'?? Remove magic numbers!!
        kword=content.substring(content.indexOf("retrieve")+9, content.indexOf("]"));
        
        logger.info("Searching keyword '"+ kword+"'");
        
        try {
        	
			response = search(kword);
			
		} catch (QueryNodeException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
        
        logger.info("Answering >> "+ response);
        // TODO: Add the bot user to the reply
        sendMessage("[updatekb]"+response.replace("\"","\\\""));

    }
    
    
    public String search(String query) throws QueryNodeException, IOException {
    	
    	final File indexDir = new File("./target/index/");
        final DemoSearcher searcher = new DemoSearcher(indexDir);
         
        // The searcher interprets the '-', so we remove it from our keywords,
        // in case is present. (I think...)
        query = query.replace("-"," ");
        query = "label:\""+query.trim()+"\"";
        Query q = searcher.parseKeywordQuery(query);
        logger.info("q: {}", q.toString());
        logger.info("Executing keyword query: '{}'", query);
        // FIXME: Why 1000?
        String[] results = searcher.search(q, 1000);
        
        if (results != null) {
            // FIXME: results can (and will!) be null.
            logger.info("Keyword query returned {} results: {}  \n \n \n \n", results.length, Arrays.toString(results));
            return results[0];
        } else {
            // TODO: Return a proper response, not a hard-coded empty string
            // Also, this response causes the talkbot to fail. (is not json!)
            logger.error("No results where found for the query {}", query);
            return "";
        }

    }

}
