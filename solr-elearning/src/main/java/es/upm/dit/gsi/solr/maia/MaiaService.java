package es.upm.dit.gsi.solr.maia;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

import es.upm.dit.gsi.solr.ElearningSolr;
import es.upm.dit.gsi.solr.maia.annotation.OnMessage;
import es.upm.dit.gsi.solr.maia.utils.JSONUtils;

import org.apache.solr.client.solrj.SolrServerException;
import org.slf4j.Logger;
/**
 * Maia service
 * The connector for the maia server
 * 
 * @author Alberto Mardomingo
 * @version 2014-11-20
 */
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
    public static final String ORIGIN = "origin";
    public static final String USER = "user";
    public static final String START_DELIMITER = "[";
    public static final String END_DELIMITER = "]";
    public static final String GAMBIT = "gambit";
    public static final String UNKNOWN = "unknown";
    public static final String RESPONSE_TAG = "maiaResponse";
    
    // Maybe make them a config option?
    
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
    
    /**
     * Receives a message from maia, and processes it.
     * 
     * @param message - The message received. 
     */
    @OnMessage("message")
    public void message(String message) {
    	
    	// Maia sends all if messages in json.
        String content = JSONUtils.getDataName(message);
        String origin = JSONUtils.find(message, ORIGIN);
        
        // Ignoring messages from myself.
        if (origin != null && origin.equals(this.username)) {
            return;
        }

        logger.debug("Message received >> " + content );
        
        // The messages I should accept now:
        // [sendMaia field field_label]
        // For example: [content for] [user arhj]
        //              [example for] [user afhf]
        // Additionally, we can get a "gambit" message, where we will look for
        // the closest topic we can get:
        //								[gambit for] [user afhf]
        
        //Sample accepted message: [content dowhile] [user dghg]
        
        // This seems... inapropiate, but, splitting by space, we should be able to get
        // an array with the labels.
        
        String[] contentArray = content.split(" ");
        
        //Extract keyword to search from the Maia message
        
        if (contentArray.length != 4) {
        	throw new IllegalArgumentException("Invalid maia message");
        }
        
        // The tags are separated by an space
        String field = contentArray[0].replace(START_DELIMITER, "");
        String keyword = contentArray[1].replace(END_DELIMITER, "");
        String userName = contentArray[3].replace(END_DELIMITER, "");
        
        logger.info("[{}] Searching keyword '{}", userName, keyword);
        
        String response = "";
        try {
        	String query;
        	if (field.equals(GAMBIT)) {
        		// In this case, we search by the default field
        		query = this.solrServer.getSearchTag() + ":" + keyword;
        	} else {
        		// We search by the provided label.
        		query = field + ":" + keyword;
        	}
        	// In any case, I only want/ need the first result
        	// I only want/need the first result.
        	
        	ArrayList<String> searchResult = this.solrServer.search(query, 1); 
        	
        	if (searchResult.size() !=0) {
        		// We have found data, return the first value
        		response = searchResult.get(0);
        	} else {
        		// There is no relevant data in solr, return unknown
        		response = UNKNOWN;
        	}
			
		} catch (SolrServerException e) { 
			logger.error("Server Exception whilst connecting to SOLR");
			e.printStackTrace();
		} catch (IOException e) {
			logger.error("IO Error whilst trying to connect with SOLR");
			e.printStackTrace();
		}
        
        //Escape some control chars. 
        //Holy cow this was annoying...
        response = response.replace("\\", "\\\\");
        response = response.replace("\"","\\\"");
        
        // The response needs to be something like:
        //					[maiaResponse label data]
        //					[maiaResponse gambit label]
        // For example:
        //					[maiaResponse resource google.com]
        // 					[maiaResponse gambit for]

        String full_response = START_DELIMITER + RESPONSE_TAG + " " + response + END_DELIMITER + 
        		" "+START_DELIMITER + USER + " " + userName + END_DELIMITER;
        logger.info("[{}] Answering >> {}", userName, full_response);
        
        // We return the data we've found, if any.
        sendMessage(full_response);
    }
	
}
