package maia;

import jason.asSemantics.ConcurrentInternalAction;
import jason.asSemantics.TransitionSystem;
import jason.asSemantics.Unifier;
import jason.asSyntax.Term;

import java.net.URISyntaxException;
import java.util.logging.Level;
import java.util.logging.Logger;

import maia.client.MaiaTxClient;


//import javax.swing.JOptionPane;



public class send extends ConcurrentInternalAction {

    // TODO: Add config option instead... (Running out of time!)
    private Logger logger = Logger.getLogger("STDOUT");
    //private Logger logger = Logger.getLogger("SYSLOG");
    
    @Override
    public Object execute(final TransitionSystem ts, Unifier un, final Term[] args) throws Exception, URISyntaxException, InterruptedException  {
        try {
        	final String key = suspendInt(ts, "bayes", 500000); 
        	
            startInternalAction(ts, new Runnable() {
                public void run() {
                	
                	String msg = "";
                	
                	for(int i = 0; i < args.length; i++) {
                        msg += args[i].toString().replace("\"", "").replace("concept_","");
                    }
                	

                    String uri = "http://localhost:1337";
                    
                    MaiaTxClient client;
					try {
						 client = new MaiaTxClient (uri);
						 client.connect();
		                 client.waitUntilConnected();    
		                 client.subscribe("message");
						
		                 logger.fine("Enviando el mensaje "+msg+" a traves de Maia");
		                 String msga = msg;
		                 
		                 client.sendMessage(msga);

		                 client.unsubscribe("message");
						
					} catch (URISyntaxException e) {
						// TODO Auto-generated catch block
						logger.fine("URISyntaxException");
						e.printStackTrace();
					} catch (InterruptedException e) {
						logger.fine("InterruptedException");
						e.printStackTrace();
					} catch (Exception e) {
						logger.fine("Unsubscription fail");
						e.printStackTrace();
					}
                   
					   logger.fine("Enviado");
					  

                		resumeInt(ts, key);
                }
            });
            
            return true;
        } catch (Exception e) {
            logger.warning("Error in internal action 'bayes'! "+e);
        }
        return false;
    }
    
    /** called back when some intention should be resumed/failed by timeout */
    @Override
    public void timeout(TransitionSystem ts, String intentionKey) {
        // this method have to decide what to do with actions finished by timeout
        // 1: resume
        //resumeInt(ts,intentionKey);
        
        // 2: fail
        failInt(ts, intentionKey);
    }
}
