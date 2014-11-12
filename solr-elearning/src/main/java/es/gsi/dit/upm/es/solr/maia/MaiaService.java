package es.gsi.dit.upm.es.solr.maia;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Arrays;




import es.gsi.dit.upm.es.solr.ElearningSolr;
//import maia.client.RevisionFailedException;
import es.gsi.dit.upm.es.solr.maia.annotation.OnMessage;
import es.gsi.dit.upm.es.solr.maia.utils.JSONUtils;

import org.apache.solr.client.solrj.SolrServerException;
import org.apache.solr.core.CoreContainer;
import org.slf4j.Logger;

public class MaiaService extends MaiaClientAdapter{

	private Logger logger;
    private String username;
    private ElearningSolr solrServer;
    
    
    /**
     * @param serverURI
     * @throws URISyntaxException 
     * @throws RevisionFailedException 
     */
    public MaiaService(String serverURI, Logger logger, ElearningSolr solrServer) throws URISyntaxException {
        super(new URI(serverURI), logger);
        this.logger = logger;
        this.solrServer = solrServer;
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
        	String query = "label:" + kword;
			response = this.solrServer.search(query, 1)[0];
			
		} catch (SolrServerException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
        
        logger.info("[{}] Answering >> {}", userName, response);
        // TODO: Add the bot user to the reply
        sendMessage("[updatekb]"+response.replace("\"","\\\"")+ " " + userName);

    }
	
}
