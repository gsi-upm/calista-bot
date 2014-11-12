package es.gsi.dit.upm.es.solr;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Collection;

import net.sf.json.JSONArray;
import net.sf.json.JSONObject;

import org.apache.solr.client.solrj.SolrServer;
import org.apache.solr.client.solrj.SolrServerException;
import org.apache.solr.client.solrj.embedded.EmbeddedSolrServer;
import org.apache.solr.common.SolrInputDocument;
import org.apache.solr.core.CoreContainer;
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
	private static final String ROOT_ELEMENT = "items";
	private static final String LABEL_DATA = "label";
	public static final String ID_FIELD = "id";
	// public static final String SIREN_FIELD = "siren-field";
	public static final String CONTENT_FIELD = "content";

	/**
	 * Creates the embedded solr server, or connects to a remote one (TODO!)
	 */
	public ElearningSolr(Logger logger, CoreContainer coreCont, String coreName) {
		this.logger = logger;
		this.server = new EmbeddedSolrServer(coreCont, coreName);
	}

	/**
	 * Initializes the server with the given data.
	 * 
	 * @param jsonInput
	 */
	public void index(InputStream jsonInput) {
		// Read json file (one or more lines)
		BufferedReader br = new BufferedReader(new InputStreamReader(jsonInput));

		try {
			String json = "";
			while (br.ready()) {
				json += br.readLine();
			}
			br.close();

			// Extract array of items, they will be the documents
			JSONObject jsonObject = JSONObject.fromObject(json);
			JSONArray objects = jsonObject.getJSONArray(ROOT_ELEMENT);
			logger.debug("[loading] Found {} objects in the file",
					objects.size());

			int counter = 0;

			Collection<SolrInputDocument> docInput = new ArrayList<SolrInputDocument>();
			// Add documents
			for (Object obj : objects) {
				if (!(obj instanceof JSONObject)) {
					logger.warn(
							"[loading] JSON format error. Found a not JSONObject when one was expected {}",
							obj);
				}
				final String id = Integer.toString(counter++);
				JSONObject app = (JSONObject) obj;
				final String content = app.toString();
				String label = app.getString(LABEL_DATA);

				SolrInputDocument doc = new SolrInputDocument();
				doc.addField(ID_FIELD, id);
				doc.addField(LABEL_DATA, label);
				doc.addField(CONTENT_FIELD, content);
				logger.info("[loading] Indexing document #{}: {}", id, label);
				docInput.add(doc);
			}
			this.server.add(docInput);
			logger.info("[loading] Commiting all pending documents");
			this.server.commit();
		} catch (Exception e) {
			// TODO: Handle the exception
		}
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
    	
    	String label = jsonObject.getString(LABEL_DATA);
    	String content = jsonObject.getString(CONTENT_FIELD);
		newDoc.addField(ID_FIELD, id);
		newDoc.addField(LABEL_DATA, label);
		newDoc.addField(CONTENT_FIELD, content);
    	
    	this.server.add(newDoc);
    	this.server.commit();
    }
    

}
