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
package maia.utils;

import net.sf.json.JSONException;
import net.sf.json.JSONObject;

/**
 * <b>Project:</b> maia-client<br />
 * <b>Package:</b> maia.utils<br />
 * <b>Class:</b> JSONUtils.java<br />
 * <br />
 * <i>GSI 2013</i><br />
 *
 * @author Miguel Coronado (miguelcb@gsi.dit.upm.es)
 * @version	Jun 5, 2013
 *
 */
public abstract class JSONUtils {

    /**
     * <p>It performs a deep search through the json object structure 
     * to retrieve the string selected. The object to retrieve is given 
     * by the <code>selectors</code> parameter.</p>
     * 
     *  <p><code>Selectors</code> is a ordered list of selectors. 
     *  Each selector is considered as key to extract the value associated 
     *  to that key in the current object. While there are more selectors, 
     *  the resulting object is converted into a new json object and a new 
     *  search is performed on it. The next example better shows how it 
     *  works. Lets consider the following json string:</p>
     *  
     *     <pre>
     *     {"name":"subscribed", "data":{"name":"message::chat"}}
     *     </pre>
     *  
     *  <p>It is a two level json object because the <i>key</i> named 
     *  <i>data</i> has another json object as associated value. 
     *  Here is the outcome of some executions performed over the json object 
     *  given:</p>
     *  
     *      <pre>
     *      find(json, "name");           // subscribed
     *      find(json, "data");           // {"name":"message::chat"}
     *      find(json, "data", "name");   // message::chat
     *      </pre>
     * 
     * @param json
     *              The json string to use
     * @param selectors
     *              Selectors to traverse the json object
     *              
     * @return the string representation of the selected object
     */
    public static String find (String json, Object...selectors) {
        
        JSONObject jsonObject = JSONObject.fromObject(json);
        int last = selectors.length-1;
        for (int i=0; i<last; i++) {
            if (!(selectors[i] instanceof String)){
                throw new IllegalArgumentException();
            }
            try{
                jsonObject = jsonObject.getJSONObject((String) selectors[i]);
            }
            catch (JSONException e){ // Enhance exception message
                JSONException jsonE = new JSONException(e.getMessage() + " Trying to execute find with selectors:" + selectors +" on json object: " + json);
                jsonE.setStackTrace(e.getStackTrace());
                throw jsonE;
            }
        }
        
        // get the string
        if (!(selectors[last] instanceof String)){
            throw new IllegalArgumentException();
        }
        
        try{
            Object result = jsonObject.get((String)selectors[last]);
            return result == null ? null : result.toString();
        }
        catch(Exception e){
            return null;
        }
        
    }

    @Deprecated
    public static String sfind (String json, Object...selectors) {

        Object result = json;
        for(Object selector : selectors){
            if (!(selector instanceof String)){
                throw new IllegalArgumentException();
            }
            JSONObject jsonObject = JSONObject.fromObject(result);
            result = jsonObject.get(selector);
        }
        
        return result == null ? null : result.toString();
    }

    /**
     * <p>Extract the value of the name attribute from the 
     * <code>JSONMessage</code> according to the <i>simple event format</i>.
     * This is the &lt;name&gt; element in the example below:</p>
     * 
     * <pre>{"name":"&lt;name&gt;", "data":{"name":"&lt;value&gt;"}}</pre>
     * 
     * <p>Alias for <code>find(json, "name")</code></p>
     * 
     * <p>Implementation of this method may vary according to the last version 
     * of maia protocol, so it is safer to use this method instead of the 
     * <code>find</code> alternative.</p>
     * 
     * @param json
     *              the message
     *              
     * @return  the value of the name attribute
     */
    public static String getName (String json){
        return find(json, "name");
    }
    
    /**
     * <p>Extract the value of the name attribute from the element data of the  
     * <code>JSONMessage</code> according to the <i>simple event format</i>.
     * This is the &lt;value&gt; element in the example below:</p>
     * 
     * <pre>{"name":"&lt;name&gt;", "data":{"name":"&lt;value&gt;"}}</pre>
     * 
     * <p>Alias for <code>find(json, "name", "data")</code></p>
     * 
     * <p>Implementation of this method may vary according to the last version 
     * of maia protocol, so it is safer to use this method instead of the 
     * <code>find</code> alternative.</p>
     * 
     * @param json
     *              the message
     *              
     * @return  the value of the name attribute from the data object
     */
    public static String getDataName (String json){
        return find(json, "data", "name");
    }
    
    /**
     * <p>Extract the value of the origin element from the   
     * <code>JSONMessage</code> according to the <i>simple event format</i>.
     * This is the &lt;origin&gt; element in the example below:</p>
     * 
     * <pre>{"name":"&lt;name&gt;", "data":{"name":"&lt;value&gt;"}, "origin":"&lt;origin&gt;"}</pre>
     * 
     * <p>Alias for <code>find(json, "origin")</code></p>
     * 
     * <p>Implementation of this method may vary according to the last version 
     * of maia protocol, so it is safer to use this method instead of the 
     * <code>find</code> alternative.</p>
     * 
     * @param json
     *              the message
     *              
     * @return  the value of the origin element from the data object
     */
    public static String getOrigin (String json){
        return find(json, "origin");
    }
    
    /**
     * <p>Extract the value of the forSubscription element from the   
     * <code>JSONMessage</code> according to the <i>simple event format</i>.
     * This is the &lt;subscription&gt; element in the example below:</p>
     * 
     * <pre>{"name":"&lt;name&gt;", "data":{"name":"&lt;value&gt;"}, "forSubscription":"&lt;subscription&gt;"}</pre>
     * 
     * <p>Alias for <code>find(json, "forSubscription")</code></p>
     * 
     * <p>Implementation of this method may vary according to the last version 
     * of maia protocol, so it is safer to use this method instead of the 
     * <code>find</code> alternative.</p>
     * 
     * @param json
     *              the message
     *              
     * @return  the value of the origin element from the data object
     */
    public static String getForSubscription (String json){
        return find(json, "forSubscription");
    }
    
    /**
     * <p>This helper method produces an outcome according to the <i>simple
     * event format</i>:</p>
     * 
     * <pre>{"name":"&lt;name&gt;", "data":{"name":"&lt;value&gt;"}}</pre>
     * 
     * <p>Where:</p>
     * <ul>
     *   <li>&lt;name&gt is the value of the <tt>name</tt> parameter.</li>
     *   <li>&lt;value&gt is the value of the value <tt>value</tt> parameter.</li>
     * </ul>
     * 
     * <p>If the value attribute is <tt>null</tt> or an empty string, it 
     * sends the message without content.</p>
     * 
     * @param name
     *          The message type.
     * @param value
     *          The value.
     *          
     * @return Json string built according to the maia message format.
     */
    public static String buildMessage (String name, String value) {
        if (value == null || value.equals("")) {
            return "{\"name\":\"" + name + "\"}";
        }
        return "{\"name\":\"" + name + "\", \"data\":{\"name\":\"" + value + "\"}}";
    }
    
    /**
     * <p>Specifically creates a message with no value, just the type of the 
     * message.</p>
     * 
     * @param name
     *              the type of the message
     * @return Json string built according to the maia message format.
     */
    public static String buildEmptyMessage( String name ){
        return buildMessage (name, null);
    }
    
}
