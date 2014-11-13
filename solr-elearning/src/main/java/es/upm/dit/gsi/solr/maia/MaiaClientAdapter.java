/*
 * Copyright 2013 miguel Grupo de Sistemas Inteligentes (GSI UPM) 
 *                                         <http://gsi.dit.upm.es>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package es.upm.dit.gsi.solr.maia;

import java.lang.reflect.Method;
import java.net.URI;
import java.util.HashSet;
import java.util.Set;

import es.upm.dit.gsi.solr.maia.annotation.OnMessage;
import es.upm.dit.gsi.solr.maia.utils.JSONUtils;

import org.java_websocket.client.WebSocketClient;
import org.java_websocket.exceptions.WebsocketNotConnectedException;
import org.java_websocket.handshake.ServerHandshake;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * <p>This class provides a few functionalities built upon the WebSocket 
 * protocol implementation to support Maia Pub/Sub Protocol. These features 
 * are:</p>
 * 
 * <ul>
 *   <li>Subscription handling</li>
 *   <li>Reconnection handling</li>
 *   <li>Message dispatching</li>
 *   <li>Standard logging (which is not part of the maia's protocol, but it 
 *   is provided here)</p>
 * </ul>
 * 
 * <h3 id="impl">About the implementation (and callbacks)</h3>
 * 
 * <p>From the point of view of internal implementation we give here a few 
 * notes. <code>{@link WebSocketClient}</code> is an abstract class so the 
 * attribute it cannot be instantiated when assigning a value to attribute 
 * named <code>{@link #client}</code>. Instead of creating a 
 * <i>named class</i> that inherits from <code>WebSocketClient</code>, we 
 * create an anonymous class when instantiating the <code>client</code> 
 * attribute. This way, we get the benefit of having access to 
 * <code>MaiaClientAdaptator</code> methods from the overridden methods of 
 * the <code>WebSocketClient</code> with no need of storing a reference to 
 * the <code>MaiaClientAdaptator</code> in the <code>WebSocketClient</code> 
 * class. Particularly,</p>
 * 
 * <ul>
 *   <li>whenever {@link WebSocketClient#onMessage(String)} is called, it 
 *       calls {@link #onMessage(String)};</li>
 *   <li>whenever {@link WebSocketClient#onClose(int, String, boolean)} is 
 *       called, it calls {@link #onClose(int, String, boolean)};</li>
 *   <li>whenever {@link WebSocketClient#onError(Exception)} method is called, 
 *       it calls {@link #onError(Exception)};</li>
 *   <li>and whenever {@link WebSocketClient#onOpen(ServerHandshake)} is 
 *       called, it calls {@link #onOpen(ServerHandshake)}.</li>
 * </ul>
 *
 * 
 * 
 * <h3>Subscription handling</h3>
 * 
 * <p>Mainly, the class manages subscriptions for re-connection purposes, 
 * and it  offers the method <code>{@link #getSubscriptions()}</code> 
 * that returns the names of the events that the client is currently 
 * subscribed to.</p>
 * 
 * <p>Subscription and unsubscription messages are sent to the server, but 
 * the client cannot assure it is correctly (un)subscribed until the 
 * acknowledge is received. Thus, we handle pending request as well as 
 * confirmed (un)subscriptions.</p>
 * 
 * <p>This class automatically update the state of the (un)subscription 
 * as soon as the {@link #subscribe(String)} or {@link #unsubscribe(String)}
 * methods are executed -the corresponding subscription is marked as pending.
 * Just after the acknowledge message is received, the class adds the event 
 * to the subscription list or removes it from the list (depending on the 
 * nature of the request sent).</p>
 *
 * <p>Therefore, {@link #subscribe(String)} or {@link #unsubscribe(String)}
 * should not be <code>overridden</code>, or at least, is overridden 
 * implementation should call to the parent implementation that handles 
 * subscriptions. If the inherited class sends (un)subcriptions messages, it 
 * will interfere on the <i>subcription handling</i> process, hence it will
 * be corrupted.</p>
 * 
 * <p><b style="color:red">We need to plan what to when no request cannot be 
 * sent: does it concern this this adaptator?</b></p>
 *
 * <p>As part of the reconnection handling process, when the client 
 * successfully reconnects to the server, it assumes all subscriptions are 
 * lost (nothing is already specified in Maia protocol) and resubmits all 
 * of them, so that the state of the client is the same once the subscription 
 * process is completed.</p>
 *
 * 
 * 
 * <h3>Reconnection handling</h3>
 * 
 * <p><code>MaiaClientAdaptator</code> implements reconnection  
 * to assure the client will remain connected until a <i>Client-initiated 
 * closure process</i> is performed. Current description of <i>maia protocol
 * </i> states client will remain connected permanently, server will never 
 * send a disconnect request. Therefore, this class will try to reconnect
 * whichever the reason of disconnection, but a client initiated closure.</p>
 * 
 *
 *  
 * <h3>Message dispatching</h3>
 * 
 * <p>MaiaClientAdaptator offers a enhanced way to process messages when 
 * they are received. As explained before, when new messages are received
 * though the WebSocket connection <code>{@link #onMessage(String)}</code> 
 * method is executed. 
 * 
 * <p>It performs some internal managements (including subscription 
 * acknowledge handling), and then dispatches the message to the associated 
 * <code>method</code> (including assertion <i>acks</i> messages). 
 * The selection of the method to execute is done by means of 
 * <code>Reflection</code> and the <code>{@link maia.client.annotation.OnMessage}</code> 
 * annotation. We use <code>OnMessage</code> annotation to associate a method 
 * to <i>reception of messages of a certain type</i>. Thus, any time a message 
 * of that is received the associated method will be executed. 
 * 
 * <p>The type of the message if given by the value of the element 
 * <i>"name"</i>. In the example below, &lt;type&gt; is the type of the 
 * message.</p>
 * 
 *     <pre>{"name":"&lt;type&gt;", "time":"1370210358496", "data":{...}}</pre>
 *  
 * <p>{@link #onMessage(String)} method should not be overridden.
 * To override it will cause <i>message dispatching</i> method to stop working.</p>
 *  
 * <p>This process <b>will only work</b> with classes that inhere from 
 * <code>MaiaClientAdaptator</code> class.</p>
 * 
 * 
 * <b>Project:</b> maia-client<br />
 * <b>Package:</b> maia.client<br />
 * <b>Class:</b> MaiaClientAdaptator.java<br />
 * <br />
 * <i>GSI 2013</i><br />
 *
 * @author Miguel Coronado (miguelcb@gsi.dit.upm.es)
 * @version	May 31, 2013
 *
 */
public abstract class MaiaClientAdapter {

    /** The logger from slf4j library */
    private Logger logger;
    
    /** Set that contains the events that the client is subscribe to */
    private Set<String> eventsSubscribed;

    /** 
     * Set that contains the pending subscription requests.
     * we do this to prevent blocking while the server responds to 
     * subscriptions, so that we can send several subscription requests 
     * subsequently.
     **/
    private Set<String> subscriptionsRequest;
    
    /** 
     * Set that contains the pending unsubscription requests.
     * we do this to prevent blocking while the server responds to 
     * unsubscriptions, so that we can send several unsubscription requests 
     * subsequently.
     **/
    private Set<String> unsubscriptionsRequest;

    /** The web-socket client to use. */
    private WebSocketClient client;
    
    /** The URI of the server */
    private URI serverURI;

    /** 
     * It provides information about whether the client is connected 
     * (since the WebSocketClient implementation does not provide any 
     * interface to get the state of the connection)
     */
    private boolean connected = false;

    /**
     * It points out whether that the client has initiated a closure process 
     */
    private boolean closing = false;;
    
    /** 
     * The number of current reconnection attempts. When the connection 
     * is established this is reset to 0 
     */
    private int connectionAttempts = 0;

    /** 
     * Array that contains increasing times for different connection attempts 
     */ 
    // they should be random times according to rdf6455
    public final int[] RECONNECTION_WAITING_TIMES = {1000,5000,15000,15000,30000,45000,60000,90000};

    public final static String SUBSCRIBED_REPLAY = "subscribed";
    
    public final static String UNSUBSCRIBED_REPLAY = "unsubscribed";
    
    /**
     * <p>Constructor: instantiate all attribute collections and 
     * the client by means of creating an anonymous class. See 
     * {@link MaiaClientAdapter} documentation for further details.</p>
     *
     * <p>It does not connect to the web socket server until 
     * <code>{@link MaiaClientAdapter#connect()}</code> is explicitly
     * called.</p>
     * 
     * @param serverURI     The URI of the MAIA server used with the
     *                      <code>{@link WebSocketClient}</code> constructor.
     * @param logger		The logger to use with the object
     */
    public MaiaClientAdapter (URI serverURI, Logger logger) {
    	
    	this.logger = logger;
    	
        // Instantiate sets
        this.eventsSubscribed = new HashSet<String>();
        this.subscriptionsRequest = new HashSet<String>();
        this.unsubscriptionsRequest = new HashSet<String>();
        
        this.serverURI = serverURI; // store for reconnection purposes
        this.client = new WebSocketClientImp (serverURI);
    }
    
    /* 
     * The inherit class is defined nested so that we can call 
     * MaiaClientAdaptator methods from the overridden methods of the 
     * client with no need of a MaiaClientAdaptator attribute on the 
     * client.
     */
    protected class WebSocketClientImp extends WebSocketClient {
        
        public WebSocketClientImp(URI serverURI) {
            super(serverURI);
        }

        @Override
        public void onMessage( String message ) {
            MaiaClientAdapter.this.onMessage(message); // shadowing
        }

        @Override
        public void onOpen( ServerHandshake handshake ) {
            MaiaClientAdapter.this.onOpen(handshake); // shadowing
        }

        @Override
        public void onClose( int code, String reason, boolean remote ) {
            MaiaClientAdapter.this.onClose(code, reason, remote); // shadowing
        }

        @Override
        public void onError( Exception ex ) {
            MaiaClientAdapter.this.onError(ex); // shadowing
        }
    }
    
    /**
     * <p>Initially connect to the server.</p> 
     * 
     * <p>This method can only be called once for a single 
     * <code>{@link WebSocketClient}</code>. Otherwise, it will throw an 
     * <code>java.lang.IllegalStateException</code>.</p>
     * 
     * <p>The connection process is asynchronous, any error will be notified 
     * through the <code>{@link #onError(Exception)}</code> callback; 
     * therefore, surrounding it with a <tt>throw catch block</tt> will 
     * result useless to treat those exceptions.</p>
     */
    public void connect() {
        logger.debug("Connecting WebSocketClient...");
//        this.connecting = true;
        client.connect();
    }
    
    public void waitUntilConnected() throws InterruptedException {
        logger.debug("Wait until connected...");
        while(!this.connected) {
            Thread.sleep(500);
        }
        logger.debug("Connected, so lets carry on.");
    }
    

    /**
     * <p>Implementation of recommendations of RFC6455 for 
     * <a href="http://tools.ietf.org/html/rfc6455#section-7.2.3">Recovering 
     * from Abnormal Closure</a>. The process is as follows:</p>
     * 
     * <ol>
     *   <li>If the maximum number of connection attempts is reached, it 
     *   gives up.</li>
     *   <li>Other wise it waits a certain time to let the server wake 
     *   up.</li>
     *   <li>If unexpectedly, after this time connection has already been 
     *   established the goal has already been met, so it does nothing.</li>
     *   <li>Finally, a new connection attempt is made.</li>
     * </ol>
     *  
     * <p>Current implementation of {@link WebSocketClient} does not support
     * connecting twice using the same object, so a new <tt>client</tt> is 
     * created every time this method tries to reconnect.</p>
     * 
     */
    protected void reconnect() {
        if(this.connectionAttempts >= this.RECONNECTION_WAITING_TIMES.length) {
            logger.warn("Reached max number of connection attempt. We give it up!");
            return;
        }
        
        // wait some time until next reconnection attempt 
        try{
            logger.info("Next connection attempt wil occur in {} second(s).", RECONNECTION_WAITING_TIMES[connectionAttempts]/1000);
            Thread.sleep(RECONNECTION_WAITING_TIMES[connectionAttempts]);
        }
        catch(InterruptedException e){
            // Ok, carry on then
            logger.warn("Interrupted: {}", e);
        }
        
        if(this.connected){
            // there is no need to keep reconnecting. Maybe some programming is going wrong...
            logger.debug("Already connected, no need to reconnect.");
            return;
        }
        
        // create a new WebSocketClient (WebSocketClient objects are not reuseable)
        this.client = new WebSocketClientImp(this.serverURI);
        // connect!
//        this.connecting = true;
        this.client.connect();
        
        this.connectionAttempts++;
    }
    
    /**
     * <p>Current implementation of MaisServer does not support secured 
     * authentication, so user-name is sent.</p>
     */
    protected void authenticate() {
        logger.debug("Send authentication");
        this.client.send( JSONUtils.buildMessage("username", "user" + System.currentTimeMillis()) );
    }
    
    /**
     * <p>Perform a <i>Client-Initiated Closure</i>. It also sets the
     * <tt>closing</tt> flag to indicate the closing process has been initiated 
     * by the client. Otherwise, as long as the library used does not follow
     * RFC recommendation for closing codes there is not reliable way to 
     * determine, inside the <code>onClose</code> method whether we need to
     * reconnect or not.</p>.
     */
    public void close() {
        logger.debug("Disconnecting WebSocketClient...");
        this.closing = true;
        client.close();
    }

    /**
     * <p>This sends a message, with the content given, to the server using 
     * the <code>WebSocketClient</code>.</p>
     * 
     * <p>The sent process is asynchronous, any error will be notified 
     * through the <code>{@link #onError(Exception)}</code> callback; 
     * therefore, surrounding it with a <tt>throw catch block</tt> will 
     * result useless to treat those exceptions. The asynchronous nature 
     * of the process makes harder to properly treat the exceptions, since the
     * information provided by the <code>WebSocketClient</code> (which 
     * receives it from the server) is not comprehensive, and not always 
     * identifies the message that caused the error.</p>
     * 
     * <p>Message querying and resubmitting is not implemented by this class.
     * In order to provide a <tt>send method</tt> that is <i>shield against 
     * error</i> a send queue need to be implemented by the inherited classes.
     * </p>
     * 
     * @param json
     *              The content of the message to be sent
     */
    public void sendRaw (String json) {
       logger.debug("Sending message: \"{}\"", json);
       this.client.send(json); 
    }
    
    /**
     * <p>This sends a message to the server whose content is build using the
     * maia message format. Messages are built using 
     * {@link JSONUtils#buildMessage(String, String)}.</p>
     * 
     *     <pre>JSONUtils.buildMessage(event, message)</pre>
     * 
     * <p>This method internally uses <code>sendRaw</code> method to send the 
     * message. See {@link #sendRaw(String)} for further information about 
     * performance and error handling.</p>
     * 
     * @param event
     *              the type of the message to send (according to maia's 
     *              message format).
     * @param message
     *              the content of the message (according to maia's 
     *              message format).
     * 
     * @see #sendRaw(String)
     */
    public void send (String event, String message){
        this.client.send(JSONUtils.buildMessage(event, message));
    }
    
    /**
     * <p>Alias for <tt>send("message", message)</tt>.</p>
     *  
     * @param message
     *              the content of the message to sent according to the 
     *              normal Maia Message format
     * 
     * @see #send(String, String)
     * 
     */
    public void sendMessage (String message) {
        send("message", message);
    }
    
    /**
     * <p>Returns a <code>Set</code> that contains all the event names that 
     * this client is subscribed to. The <code>Set</code> is immutable, so 
     * changes in the returning set will not affect to current subscriptions.
     * </p>
     * 
     * @return Set with the event subscription names
     *      
     */
    public Set<String> getSubscriptions() {
        return new HashSet<String>(eventsSubscribed);
    }
    
    /**
     * <p>This sends a subscription request message. 
     * Until the ACK message is received, the subscription is not confirmed, 
     * so the message remains marked as <i>pending</i>.</p>
     * 
     * @param event
     *              The name of the event to subscribe to.
     *              
     * @return <tt>true</tt> if the subscription was correctly sent, or the 
     *         client is already subscribed to the given event. <tt>false</tt> 
     *         otherwise.
     */
    public final boolean subscribe(String event) {

        logger.debug("Request subscription to <{}> events.", event);        
        
        if (eventsSubscribed.contains(event)) { // already registered
            logger.warn("You already are registered to \"{}\" events.", event);
            return true; // throw new Exception(); //??
        }
        
        if (subscriptionsRequest.contains(event)) { // subscription pending
            logger.warn("Subscription to \"{}\" events is still pending.", event);
            return true; // TODO:Resend not considered
        }
        
        try{
            // send subscription message
            String subscribeMsg = JSONUtils.buildMessage("subscribe", event);
            logger.debug("Subscription message {}.", subscribeMsg);
            this.client.send(subscribeMsg);
        }
        catch (WebsocketNotConnectedException e){
            logger.error("You tried to send a message while you were not connected.");
            return false;
        }
        
        // annotate as subscription pending
        logger.debug("Write down subscription to \"{}\" is pending.", event);
        subscriptionsRequest.add(event);        
        return true;
    }
    
    
    
    /**
     * <p>This sends a unsubscription request message. 
     * Until the ACK message is received, the unsubscription is not confirmed, 
     * so the message remains marked as <i>pending</i>.</p>
     * 
     * @param event
     *              The name of the event to subscribe to.
     *              
     * @return <tt>true</tt> if the subscription was correctly sent, or the 
     *         client is already subscribed to the given event. <tt>false</tt> 
     *         otherwise.
     */
    public final boolean unsubscribe(String event) throws Exception {
        if (unsubscriptionsRequest.contains(event)) {
            // already registered
            // throw new Exception(); //??            
            return true;
        }
        
        try{
            // send unsubscription message
            String unsubscribeMsg = JSONUtils.buildMessage("unsubscribe", event);
            logger.debug("Unsubscription message {}.", unsubscribeMsg);
            this.client.send(unsubscribeMsg);
        }
        catch (WebsocketNotConnectedException e){
            logger.error("You tried to send a message while you were not connected.");
            return false;
        }
        
        // annotate as unsubscription pending
        unsubscriptionsRequest.add(event);
        return true;
    }
    
    /**
     * <p>This resubmits all the subscriptions that were previously asserted.
     * This method is used to restore subscriptions after a reconnection to 
     * the server.</p>
     *  
     */
    protected void renewSubscriptions() {
        for(String event : this.eventsSubscribed){
            subscribe(event);
        }
    }
    
    /* (non-Javadoc)
     * @see org.java_websocket.client.WebSocketClient#onOpen(org.java_websocket.handshake.ServerHandshake)
     */
    public void onOpen( ServerHandshake handshake ) {
        logger.info("Connection open!");
        this.connected = true;
//        this.connecting = false;
        authenticate();
        renewSubscriptions();
    }


    /**
     * <p>This method is a callback method executed whenever the connection 
     * to the server is closed. Further explanations about this can be found 
     * in documentation of method
     * {@link org.java_websocket.client.WebSocketClient#onClose(int, java.lang.String, boolean)}
     * </p>
     * 
     * <p>When the connection is closed, thus this method is executed, 
     * <code>MaiaClientAdapter</code> tries to reconnect when the closure is a 
     * <i>Server-Initiated Closure</i>. Reconnection is performed through the 
     * method <code>{@link #reconnect()}</code> according to
     * <a href="http://tools.ietf.org/html/rfc6455#section-7.2.3">RFC6455</a>.
     * </p>
     * 
     * <p>The reconnection process is skipped when the closure process was
     * initiated by the client, this is the <tt>closing</tt> flag is set. The
     * <tt>remote</tt> flag and the <tt>code</tt> value could be used to 
     * determine the reason of the disconnection. However, the web-socket 
     * library used  does not make a proper use of those parameters.</p>
     * 
     * @param code
     *              The status code as defined in 
     *              <a href="http://tools.ietf.org/html/rfc6455">RDF6455</a>, 
     *              <a href="http://tools.ietf.org/html/rfc6455#section-7.4.1">
     *              section 7.4.1</a>.
     * @param reason
     *              The reason for the closure. As stated in the specification, 
     *              <i>the interpretation of the data is left to the endpoint
     *              </i>, so current implementation omit this parameter.  
     *              
     * @param remote
     *              boolean that indicates whether the closure was 
     *              <a href="http://tools.ietf.org/html/rfc6455#section-7.2.2">
     *              Server-Initiated Closure</a> (<tt>true</tt>) or a 
     *              <a href="http://tools.ietf.org/html/rfc6455#section-7.2.1">
     *              Client-Initiated Closure</a> (<tt>false</tt>).
     * 
     * @see org.java_websocket.client.WebSocketClient#onClose(int, java.lang.String, boolean)
     */
    public void onClose( int code, String reason, boolean remote ) {
        this.connected = false;
        logger.debug("Connection Closed ({}): {} || Remote:{}", code, reason, remote);
        
        if(closing) {
            logger.info("Connection closed.");
            return;
        }
        logger.info("Connection closed remotely. Trying to reconnect...");
        reconnect();
    }

    /* (non-Javadoc)
     * @see org.java_websocket.client.WebSocketClient#onError(java.lang.Exception)
     */
    public void onError( Exception ex ) {
        logger.warn("Some error happened: {}:{}", ex.getClass(), ex.getMessage());
        ex.printStackTrace();
//        if(ex instanceof java.net.ConnectException){
//            reconnect();
//        }
    }


    /**
     * <p>Overridden method that handles received messages though a two step 
     * process:</p>
     * <ul>
     *   <li>First, it checks whether the message is the formal response to 
     *   a pending request. If so, processes it.</li>
     *   <li>Then, the message is derived to the <tt>execution-resolver</tt> 
     *   according to the type of the message.</li>
     * </ul>
     * 
     * <p>A thorough explanation of Message dispatching is performed in the 
     * class documentation</p>
     * 
     * @param message
     *                  The full message received from the server.
     * 
     * @see org.java_websocket.client.WebSocketClient#onMessage(java.lang.String)
     */
    public void onMessage (String message) {

        logger.debug("Message received <<{}>> ", message);
        
        // do some particular stuff
        String name = JSONUtils.getName(message);
        String dataName = JSONUtils.getDataName(message);
        if(name != null && name.equals(SUBSCRIBED_REPLAY) && subscriptionsRequest.contains(dataName)) {
            subscriptionsRequest.remove(dataName);
            eventsSubscribed.add(dataName);
            logger.debug("Current subscriptions: {}", this.eventsSubscribed);
        }
        
        else if(name != null && name.equals(UNSUBSCRIBED_REPLAY) && unsubscriptionsRequest.contains(dataName)) {
            unsubscriptionsRequest.remove(dataName);
            eventsSubscribed.remove(dataName);
            logger.debug("Current subscriptions: {}", this.eventsSubscribed);            
        }
        
        // Delegate
        executionResolve(message);
    }
    
    /**
     * <p>Helper method that executes the appropriate method associated to 
     * the event message received.</p> 
     * <p>It uses java's introspection capabilities by means of the Reflection 
     * API and annotated methods.</p>
     * 
     * @param message   The message
     */
    private void executionResolve ( String message ) {
        Method[] methods = getClass().getMethods();

        /* message type may vary for the same subscription, so we use *
         * the field forSubscription                                  */            
        String name = JSONUtils.getForSubscription (message);
        if (name == null || name.equals("")){
            name = JSONUtils.getName(message);
        }
        
        for (Method method : methods) {
            OnMessage annot = method.getAnnotation(OnMessage.class);
                     
            if (annot != null && annot.value().equals(name)) {
                try {
                    logger.debug("Events <{}> are associated to method: {}", name, method.getName());
                    method.invoke(this, message);
                } catch (Exception e) {
                    logger.error("Error while trying to invoke method: {}", method);
                    logger.error(e.getMessage());
                    e.printStackTrace();
                }
            }
        }
    }
 
}

