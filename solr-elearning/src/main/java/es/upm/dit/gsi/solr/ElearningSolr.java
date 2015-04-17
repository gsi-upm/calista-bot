package es.upm.dit.gsi.solr;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Map;

import net.sf.json.JSONObject;

import org.apache.solr.client.solrj.SolrQuery;
import org.apache.solr.client.solrj.SolrServer;
import org.apache.solr.client.solrj.SolrServerException;
import org.apache.solr.client.solrj.impl.HttpSolrServer;
import org.apache.solr.common.SolrDocumentList;
import org.apache.solr.common.SolrInputDocument;
import org.apache.solr.common.params.CommonParams;
import org.slf4j.Logger;

/**
 * Wrapper for the solr server.
 * Connects to a "remote" solr server, and interacts with it as demanded. 
 * 
 * @author Alberto Mardomingo
 * @version 2014-11-20
 */
public class ElearningSolr {

	/**
	 * The logger - configured elsewhere
	 */
	private Logger logger;

	/**
	 * The solr server
	 */
	private SolrServer server;
	
	/**
	 * The basic solr document fields
	 * 
	 */
	private String[] docFields;
	
	/**
	 * The Fields Filter for the solr query.
	 * 
	 */
	private String[] fieldsFilter;
	
	/**
	 * The "default" search tag
	 * Usually, we use the "label" tag
	 */
	private String searchTag;

	/**
	 * Creates the connector to the HTTP SolrServer,
	 * without default values.
	 * 
	 * It needs to be further configured!
	 * 
	 * @param logger
	 * @param serverUrl - The solr server url, including the core.
	 */
	public ElearningSolr(Logger logger, String serverURL) {
		this.logger = logger;
		this.server = new HttpSolrServer(serverURL);
	}
	
	/**
	 * Creates the connector to the HTTP SolrServer
	 * Fully configured
	 * 
	 * @param logger
	 * @param serverUrl - The solr server url, including the core.
	 * @param docFields - The fields to return in a solr search
	 * @param searchTag - The expected tag for searching the solr database 
	 */
	public ElearningSolr(Logger logger, String serverURL, String[] docFields, String[] fieldsFilter, String searchTag) {
		this.logger = logger;
		this.server = new HttpSolrServer(serverURL);
		this.docFields = docFields;
		this.fieldsFilter = fieldsFilter;
		this.searchTag = searchTag;
	}
	
	
	/**
	 * Adds a doc to the server.
	 * 
	 * @param id
	 * @param json
	 * @throws IOException
	 */
    public void addDocument(String id, String json) throws SolrServerException, IOException{
    	// Empry solr document
    	SolrInputDocument newDoc = new SolrInputDocument();
    	
    	// I asume the json is directly the data we want to add
    	JSONObject jsonObject = JSONObject.fromObject(json);
    	
    	// Converts the json to the doc, but only with the required fields.
    	for(String fieldName : docFields) {
    		if(jsonObject.has(fieldName)) 
    			newDoc.addField(fieldName, jsonObject.get(fieldName));
    	}
    	
    	//FIXME: We *may* be missing the id here.
    	// Our best bet may be to configure solr, so the id field is autoincremented.
    	
    	this.server.add(newDoc);
    	this.server.commit();
    }
    
    /**
     * Returns the "default" search tag
     * 
     * @return
     */
    public String getSearchTag() {
    	return this.searchTag;
    }
    
    /**
     * Sets the tag to use in the filter.
     */
    public void setSearchTag(String searchTag) {
    	this.searchTag = searchTag;
    }
    
    /**
     * If we want to have a "default" fields filter,
     * sets them.
     * 
     * @param fields - the fieds to show in the result.
     */
    public void setFieldsFilter(String[] fields){
    	this.fieldsFilter = fields;
    }
    
    /**
     * Given a String Query, performs a query to the server,
     * and return the relevant data. 
     * It utilices the field general field filter, set with
     * the "setFieldsFilter" method.
     * 
     * @param q - The query for the data
     * @param n - The max number of results
     * @return - An array with all the results
     */
    public ArrayList<String> search(String query, int n) throws SolrServerException, IOException{
    	return search(query, n, this.fieldsFilter);
    }
    
    /**
     * Given a String Query and a fieldFilter list,
     *  performs a query to the server, and return the relevant data. 
     * 
     * @param q - The query for the data
     * @param fl - The list of fields to return
     * @param n - The max number of results
     * @return - An array with all the results
     */
    public ArrayList<String> search(String query, String[] fieldsList, int n) throws SolrServerException, IOException{
    	return search(query, n, fieldsList);
    }
        

    /**
     * 
     * Given a String Query, performs a query to the server,
     * and return the relevant data. 
     * 
     * @param q - The query for the data
     * @param n - The max number of results
     * @param fields - The fields I want to return.
     * @return - An array with all the results
     */
    public ArrayList<String> search(String query, int n, String[] fields) throws SolrServerException, IOException{
    	SolrQuery sQuery = new SolrQuery();
    	// Sets the query
    	sQuery.set(CommonParams.Q, query);
    	
    	// If I want the search to return all the fields stored,
    	// the fields variable will be null.
    	if(fields != null){
    		sQuery.set(CommonParams.FL, String.join(",",fields));
    	}
    	SolrDocumentList qResults = this.server.query(sQuery).getResults();
    	
    	ArrayList<String> result = new ArrayList<String>();
    	for(Map docResult: qResults) {
    		
    		JSONObject jsonResult = new JSONObject();
    		jsonResult.putAll((docResult));
    		
    		result.add(jsonResult.toString());

    	}
    	return result;
    }
}
