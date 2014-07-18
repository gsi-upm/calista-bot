package maia;

import jason.RevisionFailedException;
import jason.asSemantics.ConcurrentInternalAction;
import jason.asSemantics.TransitionSystem;
import jason.asSemantics.Unifier;
import jason.asSyntax.Term;

import java.net.URISyntaxException;
import java.util.logging.Logger;

import maia.client.MaiaRxClient;
//import javax.swing.JOptionPane;



public class start extends ConcurrentInternalAction {

    private Logger logger = Logger.getLogger("gui."+start.class.getName());

    
    @Override
    public Object execute(final TransitionSystem ts, Unifier un, final Term[] args) throws Exception, URISyntaxException, InterruptedException  {
        try {
        	final String key = suspendInt(ts, "bayes", 500000); 
        	
            startInternalAction(ts, new Runnable() {
                public void run() {
                	
                	

                	
                	

                	

                    String uri = "http://localhost:1337";
                    
                    MaiaRxClient client;
					try {
						client = new MaiaRxClient (uri, ts);
						//ts.getLogger().info("Conectando a Maia...");  
						 client.connect();
		                 client.waitUntilConnected();    
		                 //logger.info("Suscribiendose...");  
		                 client.subscribe("message");
						
		              //   logger.info("Enviando hola...");  
		              //   client.sendMessage("hola");
		                        
		              //   logger.info("Enviando holo...");
		              //   client.sendMessage("holo");  
		                        
		                        
		                    
		                    
		                    
		                				
						
					} catch (URISyntaxException e) {
						// TODO Auto-generated catch block
						logger.info("URISyntaxException");
						e.printStackTrace();
					} catch (InterruptedException e) {
						logger.info("InterruptedException");
						e.printStackTrace();
					} catch (RevisionFailedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
                   
					   //logger.info("Listo");


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
