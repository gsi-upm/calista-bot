package es.upm.dit.gsi.solr.maia;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

import es.upm.dit.gsi.solr.ElearningSolr;
import es.upm.dit.gsi.solr.maia.annotation.OnMessage;
import es.upm.dit.gsi.solr.maia.utils.JSONUtils;

import org.apache.solr.client.solrj.SolrServerException;
import org.apache.solr.common.params.CommonParams;
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
    public static final String COMMAND_DELIMITER = "¬";
    public static final String USER_DELIMITER_START = "[";
    public static final String USER_DELIMITER_END = "]";
    // These three are string instead of chars to simplify deleting them from other strings  
    public static final String GAMBIT = "gambit";
    public static final String UNKNOWN = "unknown";
    public static final String RESPONSE_TAG = "maiaResponse";
    
    // TODO: Get this from the config
    public static final String CONTENT = "content";
    
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
        // ¬sendMaia field field_label [user userid]
        // For example: ¬sendMaia content for [user arhj]
        //              ¬sendMaia example for [user afhf]
        // Additionally, we can get a "gambit" message, where we will look for
        // the closest topic we can get:
        //								¬sendMaia gambit for [user afhf]
        
        //Sample accepted message: ¬sendMaia content dowhile [user dghg]
        
        // This seems... inappropriate, but, splitting by space, we should be able to get
        // an array with the labels.
        
        String[] contentArray = content.split(" ");
        
        //Extract keyword to search from the Maia message
        
        if (contentArray.length != 5) {
        	throw new IllegalArgumentException("Invalid maia message");
        }
        
        // The tags are separated by an space
        String field = contentArray[1].replace(COMMAND_DELIMITER, "");
        String keyword = contentArray[2];
        String userName = contentArray[4].replace(USER_DELIMITER_END, "");
        
        logger.info("[{}] Searching keyword '{}", userName, keyword);
        
        String response = "";
        try {
        	String query;
        	String[] fieldList = new String[1];
        	
        	// There may be a better way of doing this?
        	if (field.equals(GAMBIT)) {
        		// In this case, we search by the text contents fields, and want the label
        		query = CONTENT + ":" + keyword;
        		// The label is the search tag we will use if the "gambit"is accepted. 
        		fieldList[0] = this.solrServer.getSearchTag();
        	} else {
        		// We search by the provided label, and get the asked field:
        		query = this.solrServer.getSearchTag() + ":" + keyword;
        		fieldList[0] = field;
        	}
        	// In any case, I only want/ need the first result
        	// I only want/need the first result.
        	
        	ArrayList<String> searchResult = this.solrServer.search(query, fieldList, 1); 
        	
        	System.out.println(searchResult.size());
        	if (searchResult.size() !=0) {
        		// We have found data, return the first value
        		// the expected format is something like:
        		//  [maiaResponse resource google.com]
        		// So we take the json response and get only the data
        		String searchResponse = searchResult.get(0);
        		
        		// If nothing was found, the response would be and empty json ("{}"), so...
        		if (searchResponse.equals("{}")) {
        			response = UNKNOWN;
        		} else {
        			response = field + " " + JSONUtils.find(searchResponse, fieldList[0]);
        		}
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
        //					¬maiaResponse label data
        //					¬maiaResponse gambit label
        // For example:
        //					¬maiaResponse resource google.com
        // 					¬maiaResponse gambit for
        
        String full_response = COMMAND_DELIMITER + RESPONSE_TAG + " " + response + 
        		" "+USER_DELIMITER_START + USER + " " + userName + USER_DELIMITER_END;
        logger.info("[{}] Answering >> {}", userName, full_response);
        
        // We return the data we've found, if any.
        sendMessage(full_response);
    }
	
}
