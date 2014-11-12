package es.gsi.dit.upm.es.solr;

import java.io.File;
import java.io.InputStream;

import org.slf4j.Logger;

/**
 * Main class Obviuos WIP
 *
 */
public class SolrElearningApp {
	
	private final File indexDir;
	private static final InputStream DATA_PATH = SolrElearningApp.class
			.getClassLoader().getResourceAsStream("vademecum.json");
	private static Logger logger;
	private static final String ROOT_ELEMENT = "items";
	private static final String LABEL_DATA = "label";

	public SolrElearningApp(final File indexDir){
		this.indexDir = indexDir;
        if (indexDir.exists()) {
            logger.error("Existing directory {} - aborting", indexDir);
            System.exit(1);
        }
        logger.info("[loading] Creating index directory {}", indexDir);
        indexDir.mkdirs();
	}
	
	public static void main(String[] args) {
		
		System.out.println("Nothing to see here yet...");
	}
}
