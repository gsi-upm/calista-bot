package es.upm.dit.gsi.solr;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URISyntaxException;
import java.util.Properties;

import es.upm.dit.gsi.solr.maia.MaiaService;

import org.apache.commons.cli.BasicParser;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.io.FileUtils;
import org.apache.solr.client.solrj.SolrServer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Main class Obviuos WIP
 *
 */
public class SolrElearningApp {

	private final String indexDir;
	private static final InputStream DATA_PATH = SolrElearningApp.class
			.getClassLoader().getResourceAsStream("vademecum.json");
	private static Logger logger;
	private static final String ROOT_ELEMENT = "items";
	private static final String LABEL_DATA = "label";

	public SolrElearningApp(String indexDir, String coreName) {
		this.indexDir = indexDir;
		File iDir = new File(indexDir);
		if (iDir.exists()) {
			logger.error("Existing directory {} - aborting", indexDir);
			System.exit(1);
		}
		logger.info("[loading] Creating index directory {}", indexDir);
		iDir.mkdirs();

	}

	/**
	 * Parse the commandLine options and the config options, and return them as
	 * a hashMap.
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
				// LOOK: Check this is done properly.
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
		cliOptions.addOption("s", "serverURL", false, "Solr Server URL");

		Properties options = getOptions(cliOptions, args);

		logger = LoggerFactory.getLogger(options.getProperty("logger"));

		final File indexDir = new File(options.getProperty("indexDir"));
		if (Boolean.valueOf(options.getProperty("deleteIndexDir"))
				&& indexDir.exists()) {
			FileUtils.deleteDirectory(indexDir);
		}

		ElearningSolr solrServer = new ElearningSolr(logger, options.getProperty("serverURL"));

		String maiauri = options.getProperty("maiaURI");

		try {
			// Connect to the maia service.
			MaiaService mservice = new MaiaService(maiauri, logger, solrServer);
			mservice.connect();
			mservice.waitUntilConnected();
			mservice.subscribe("message");

		} catch (InterruptedException e) {
			logger.info("InterruptedException");
			e.printStackTrace();
		} catch (URISyntaxException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
}
