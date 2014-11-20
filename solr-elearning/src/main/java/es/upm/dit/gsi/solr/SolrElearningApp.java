package es.upm.dit.gsi.solr;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URISyntaxException;
import java.util.Properties;

import es.upm.dit.gsi.solr.maia.MaiaService;

import org.apache.commons.cli.BasicParser;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Main class
 * Reads the config and launchs the Maia-solr connector
 * 
 * @author Alberto Mardomingo
 * @version 2014-11-20
 */
public class SolrElearningApp {
	
	/**
	 * The logger
	 * This logger will be used through all the classes.
	 */
	private static Logger logger;

	/**
	 * Parse the commandLine options and the config options,
	 * and return them as a hashMap.
	 * 
	 * @param - The cli avaliable options
	 * @param - String[] with the cli arguments
	 * @return HashMap with the options.
	 */
	private static Properties getOptions(Options cliOptions, String[] args) {
		try {
			// First, get the options from the cli.
			CommandLineParser parser = new BasicParser();
			CommandLine cmd = parser.parse(cliOptions, args);

			if (cmd.hasOption('c')) {

				// Read config file
				String configPath = cmd.getOptionValue('c');

				Properties prop = new Properties();
				prop.load(new FileInputStream(configPath));

				// The cli arguments take precedence over the the properties
				for (Option option : cmd.getOptions()) {
					prop.setProperty(option.getLongOpt(), option.getValue());
				}

				return prop;

			} else {
				// TODO: Check this is done properly.
				System.out.println("Specify a config file with -c CONFIGPATH");
				System.exit(0);
			}

		} catch (Exception e) {
			logger.error("[loading] Problem reading the options: "
					+ e.getMessage());
			e.printStackTrace();
			System.exit(0);
		}
		return null;
	}

	/**
	 * Creates the solr server connector, and launches the maia websocket listener
	 * 
	 * @param args - The cli params.
	 * @throws IOException
	 */
	public static void main(String[] args) throws IOException {

		Options cliOptions = new Options();

		// TODO: Make this static constants.
		cliOptions.addOption("c", "configFile", true,
				"Config file - Absolute path");
		cliOptions.addOption("m", "maiaURI", false, "URI for the maia service");
		cliOptions.addOption("i", "indexDir", false,
				"Index directory for the siren files - Deprecated!!");
		cliOptions.addOption("d", "deleteIndexDir", false,
				"Delete the indexDir if present");
		cliOptions.addOption("l", "logger", false,
				"Logger to use: ToSTDOUT or ToSysLog");
		cliOptions.addOption("n", "coreName", false, "Solr Core");
		cliOptions.addOption("s", "solrURL", false, "Solr Server URL");
		cliOptions.addOption("t", "searchTag", false, "The tag for the solr search");
		cliOptions.addOption("l", "fl", false, "Solr fields to return, sepparated by commas");
		cliOptions.addOption("f", "solrFields", false, "The list of solr fields when adding a document");

		Properties options = getOptions(cliOptions, args);

		logger = LoggerFactory.getLogger(options.getProperty("logger"));

		ElearningSolr solrServer = new ElearningSolr(logger, options.getProperty("solrURL")+"/" + options.getProperty("coreName"),
													options.getProperty("solrFields").split(","), options.getProperty("fl").split(","), 
													options.getProperty("searchTag"));

		String maiauri = options.getProperty("maiaURI");

		try {
			// Connect to the maia service.
			MaiaService mservice = new MaiaService(maiauri, logger, solrServer);
			mservice.connect();
			mservice.waitUntilConnected();
			mservice.subscribe("message");

		} catch (InterruptedException e) {
			logger.error("InterruptedException trying to connect to maia:");
			e.printStackTrace();
		} catch (URISyntaxException e) {
			logger.error("URISyntaxException trying to connect to maia:");
			e.printStackTrace();
		}
	}
}
