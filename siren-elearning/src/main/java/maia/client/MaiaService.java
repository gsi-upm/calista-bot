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
	
	private Logger logger;
    private String username;
    
    
    /**
     * @param serverURI
     * @throws URISyntaxException 
     * @throws RevisionFailedException 
     */
    public MaiaService(String serverURI, Logger logger) throws URISyntaxException {
        super(new URI(serverURI), logger);
        this.logger = logger;
        logger.info("Starting Maia subscription...");
		
    }

    @OnMessage("subscribed")
    public void onSubscribe(String message) {
    	
        logger.info("[loading] Subscribed >> " + message + "\n");
        logger.info("[loading] Service ready: listening event messages on Maia network");
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

        logger.debug("Message received >> " + content );
        
        
        //Discard the message if not asking to retrieve
        if (!content.contains("retrieve")){
        	return;
        }
        
        //Sample accepted message: [retrieve dowhile]
        
        
        //Extract keyword to search from the Maia message
        //TODO: '+9'?? Remove magic numbers!!
        kword=content.substring(content.indexOf("retrieve")+9, content.indexOf("]"));
        
        // get the User
        String userData = content.substring(content.indexOf("[user"));
        String userName = userData.substring(0, userData.indexOf("]")+1);
        // We have something like [user HqeCrvi68Ah7SEyzuJ5m]
        
        
        logger.info("[{}] Searching keyword '{}", userName, kword);
        
        try {
        	
			response = search(kword);
			
		} catch (QueryNodeException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
        
        logger.info("[{}] Answering >> {}", userName, response);
        // TODO: Add the bot user to the reply
        sendMessage("[updatekb]"+response.replace("\"","\\\"")+ " " + userName);

    }
    
    
    public String search(String query) throws QueryNodeException, IOException {
    	
    	final File indexDir = new File("./target/index/");
        final DemoSearcher searcher = new DemoSearcher(indexDir, logger);
         
        // The searcher interprets the '-', so we remove it from our keywords,
        // in case is present. (I think...)
        query = query.replace("-"," ");
        query = "label:\""+query.trim()+"\"";
        Query q = searcher.parseKeywordQuery(query);
        logger.debug("q: {}", q.toString());
        logger.debug("Executing keyword query: '{}'", query);
        // FIXME: Why 1000?
        String[] results = searcher.search(q, 1000);
        
        if (results != null) {
            // FIXME: results can (and will!) be null.
            logger.debug("Keyword query returned {} results: {}  \n \n \n \n", results.length, Arrays.toString(results));
            return results[0];
        } else {
            // TODO: Return a proper response, not a hard-coded empty string
            // Also, this response causes the talkbot to fail. (is not json!)
            logger.error("No results where found for the query {}", query);
            return "";
        }

    }

}
