package es.upm.dit.gsi.solr;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collection;

import net.sf.json.JSONArray;
import net.sf.json.JSONObject;

import org.apache.solr.client.solrj.SolrQuery;
import org.apache.solr.client.solrj.SolrRequest;
import org.apache.solr.client.solrj.SolrServer;
import org.apache.solr.client.solrj.SolrServerException;
import org.apache.solr.client.solrj.impl.HttpSolrServer;
import org.apache.solr.client.solrj.response.QueryResponse;
import org.apache.solr.common.SolrDocument;
import org.apache.solr.common.SolrDocumentList;
import org.apache.solr.common.SolrInputDocument;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.slf4j.Logger;

/**
 * Wrapper for the solr server.
 * 
 * For the moment, embedded solr server.
 * 
 * @author amardomingo
 *
 */
public class ElearningSolr {

	private Logger logger;

	/**
	 * The solr server, either embedded or not
	 */
	private SolrServer server;

	// TODO: Make this a config option
	public static final String LABEL_FIELD = "label";
	public static final String ID_FIELD = "id";
	public static final String CONTENT_FIELD = "content";

	/**
	 * Creates the connector to the HTTP SolrServer
	 */
	public ElearningSolr(Logger logger, String serverURL) {
		this.logger = logger;
		this.server = new HttpSolrServer(serverURL);
	}
	
	/**
	 * Adds a doc to the server.
	 * 
	 * @param id
	 * @param json
	 * @throws IOException
	 */
    public void addDocument(String id, String json) throws SolrServerException, IOException{
    	SolrInputDocument newDoc = new SolrInputDocument();
    	
    	// I asume the json is directly the data we want to add
    	JSONObject jsonObject = JSONObject.fromObject(json);
    	
    	String label = jsonObject.getString(LABEL_FIELD);
    	String content = jsonObject.getString(CONTENT_FIELD);
		newDoc.addField(ID_FIELD, id);
		newDoc.addField(LABEL_FIELD, label);
		newDoc.addField(CONTENT_FIELD, content);
    	
    	this.server.add(newDoc);
    	this.server.commit();
    }
    

    /**
     * 
     * Given a String Query, performs a query to the server,
     * and return the relevant data. 
     * 
     * @param q - The query for the data
     * @param n - The max number of results
     * @return - An array with all the results
     */
    public String[] search(String query, int n) throws SolrServerException, IOException{
    	SolrQuery sQuery = new SolrQuery();
    	sQuery.set("q", query);
    	System.out.println("Query: " + query);
    	SolrDocumentList qResults = this.server.query(sQuery).getResults();
    	
    	String[] result = new String[qResults.size()];
    	for(int i = 0; i< result.length; i++) {
    		// I'm really, REALLY unsure about this.
    		// Basically, I want to get the Content field.
    		result[i] = (String)qResults.get(i).getFieldValue(CONTENT_FIELD);	
    	}
    	return result;
    }
}
