package es.upm.dit.gsi.solr.maia;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;

import es.upm.dit.gsi.solr.ElearningSolr;
import es.upm.dit.gsi.solr.maia.annotation.OnMessage;
import es.upm.dit.gsi.solr.maia.utils.JSONUtils;

import org.apache.solr.client.solrj.SolrServerException;
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
        
        //TODO: change strings to constants
        
        //Extract keyword to search from the Maia message
        String kStart = content.replace("[retrieve", "");
        kword = kStart.substring(0, kStart.indexOf(']')).trim();
        
        // get the User
        // We have something like [user HqeCrvi68Ah7SEyzuJ5m]
        String userName = content.substring(content.lastIndexOf("[user")).replace("[user","").replace("]","").trim();
        
        logger.info("[{}] Searching keyword '{}", userName, kword);
        
        try {
        	String query = "label:" + kword;
        	// I only want the first result.
			response = this.solrServer.search(query, 1).get(0);
			
		} catch (SolrServerException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
        
        logger.info("[{}] Answering >> {}", userName, response);
        // TODO: Add the bot user to the reply
        sendMessage("[updatekb] "+response.replace("\"","\\\"")+ " [user " + userName + "]");

    }
	
}
