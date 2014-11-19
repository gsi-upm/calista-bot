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

	/**
	 * The logger
	 * Is configured elsewhere, for unified logging
	 */
	private Logger logger;
	
	/**
	 * The username for the maia connection - I think. 
	 */
    private String username;
    
    /**
     * The solr server 
     * An connector to the solr server.
     */
    private ElearningSolr solrServer;
    
    
    /**
     *  The relevant tags for the Maia message.
     */
    // Maybe make them a config option?
    
    public static final String RETRIEVE = "retrieve";
    public static final String ORIGIN = "origin";
    public static final String USER = "user";
    public static final String START_DELIMITER = "[";
    public static final String END_DELIMITER = "]";
    public static final String UPDATEKB = "updatekb";
    
    /**
     * 
     * Creates the maia service.
     * 
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
    	
    	// Maia sends all if messges in json.
        String content = JSONUtils.getDataName(message);
        String origin = JSONUtils.find(message, ORIGIN);
        
        // Ignoring messages from myself.
        if (origin != null && origin.equals(this.username)) {
            return;
        }

        logger.debug("Message received >> " + content );
        
        
        //Discard the message if not asking to retrieve
        if (!content.contains(RETRIEVE)){
        	//TOFIX This is quite dirty and prone to failure
        	return;
        }
        
        //Sample accepted message: [retrieve dowhile]

        //Extract keyword to search from the Maia message
        String kwStart = content.replace(START_DELIMITER + RETRIEVE, "");
        String keyword = kwStart.substring(0, kwStart.indexOf(END_DELIMITER)).trim();
        
        // get the User
        // We have something like [user HqeCrvi68Ah7SEyzuJ5m]
        String userName = content.substring(content.lastIndexOf(START_DELIMITER + USER))
        		.replace(START_DELIMITER + USER,"").replace(END_DELIMITER,"").trim();
        
        logger.info("[{}] Searching keyword '{}", userName, keyword);
        
        String response = "";
        try {
        	String query = solrServer.getSearchTag() + ":" + keyword;
        	// I only want the first result.
			response = this.solrServer.search(query, 1).get(0);
			
		} catch (SolrServerException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
        
        //Escape some control chars. 
        //Holy cow this was annoying...
        response = response.replace("\\", "\\\\");
        response = response.replace("\"","\\\"");
        String full_response = START_DELIMITER + UPDATEKB + END_DELIMITER + " " + response + 
        		" "+START_DELIMITER + USER + " " + userName + END_DELIMITER;
        logger.info("[{}] Answering >> {}", userName, full_response);
        sendMessage(full_response);
    }
	
}
