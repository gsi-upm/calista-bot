#!/usr/bin/env python
# coding=utf-8
import os, sys
from setuptools.command.sdist import re_finder

sys.path.append(os.path.join(os.path.dirname(__file__), 'lib'))
sys.path.append(os.path.join(os.path.dirname(__file__), 'lib/ext'))

import flask
from socket import socket
from re import sub

from unidecode import unidecode

import maia

import inspect, unicodedata
import json
import re
import logging
import logging.handlers

this_dir = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))    
#flask.response.content_type = 'application/json'

#ChatScript variables
cs_buffer_size = 1024
cs_tcp_ip = '127.0.0.1'
cs_tcp_port = 1024

# Special Strings to be replaced, in CS, as ";" or "{"
# to prevent conflicts, since they are considered reserved there.
cs_tokens = {u"SEMICOLON": u";", u"COLN": u":", u"PARENTESISO": u"(", 
             u"PARENTESISC": u")", u"CURLYO": u"{", u"CURLYC": u"}",
             u"BRACKETO": u"[", u"BRACKETC": u"]", u"WILDCARD": u"*"}
# Can't use a tag containing other, or the replace may mess it up
                

#Logging system
log_name='gsibot'
logger = logging.getLogger(log_name)

# Log to file
#hdlr = logging.FileHandler(this_dir+'/'+log_name+'.log')
##formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')

# Log to stdout
hdlr = logging.StreamHandler(sys.stdout)
formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')


# Log to Syslog
#hdlr = logging.handlers.SysLogHandler()
#formatter = logging.Formatter('calistabot: %(levelname)s %(name)s - %(message)s')

hdlr.setFormatter(formatter)
logger.addHandler(hdlr)
logger.setLevel(logging.INFO)


#Maia server variables
maia_uri = 'localhost'
maia_port = '1337'

maia = maia.Maia('ws://'+maia_uri+':'+maia_port, logger) 
maia.connect()

# Vadmecum url
vademecum_url = u'http://www.dit.upm.es/~pepe/libros/vademecum/topics/{url}.html'

# General config
host_name = 'demos.gsi.dit.upm.es'
host_port = 5005



app = flask.Flask(__name__)
app.debug = True

@app.route('/')
def rootURL():
    return TalkToBot()  
    
@app.route('/TalkToBot') 
def TalkToBot():
    
    query = flask.request.args
    if 'feedback' in query:
        saveFeedback()
        return;
    
    
    #Collect URI parameters
    query_q = unidecode(query['q'])
    query_user = query['user'] or 'anonymous'
    query_bot = query['bot'] or 'Duke'
    #query_lang = query['lang'] or 'es'
    query_lang = 'es'

    full_response = '0' #Default values
    
    #Send user input to ChatScript, get response
    cs_output = sendChatScript(query_q.lower(), query_bot, query_user) 
    
    #Split conversation response and out-of-band commands (example: Hello again [sendMaia assert returning])
    cs_output_array=splitOOB(cs_output)
    
    #Extract the natural language response from the array
    nl_response = cs_output_array[0]
    cs_output_array.pop(0);

    # The resource to direct the bot to, if exists.
    oob_resource = ""
    oob_label = ""
    
    #Proceed with further executions if needed, getting the natural language response produced
    for command in cs_output_array:
        oob_result = executeOOB(command,query_user,query_bot);
        nl_response += oob_result['nl_response']

        if 'resource' in oob_result:
            oob_resource = oob_result['resource']
        if 'label' in oob_result:
            oob_label = oob_result['label']


    #Detect the @full_response flag used to ask the user for feedback
    if '@fullResponse' in nl_response:
        full_response= '1'
        nl_response=re.sub(u"@fullResponse","",nl_response)


    #Render natural language response in JSON format
    json_response = renderJson(query_q, nl_response, query_bot, query_user, full_response, oob_resource, oob_label)
    print(json_response)
    return flask.Response(json_response, content_type="application/json")

def saveFeedback():

    log_text="feedback using bot '"+flask.request.args['bot']+"' in question '"+flask.request.args['q']+"' with answer given '"+flask.request.args['response']
    
    if (Flask.request.query.feedback=='2'):
        logger.error("[user: "+flask.request.args['user']+"] Negative "+log_text)
    else:
        logger.info("[user: "+flask.request.args['user']+"] Positive "+log_text)

    

#Produces an array with the natural language response as first element, and OOB tags the rest
def splitOOB(s):
    command_token = u"¬"
    # Find all ocurrences of "¬", which will indicate Out-of-Band commands
    # If there is no command, return array with just the user input
    
    if command_token not in s:
        return [s]
    
    oob_commands = []
    # Split by the token
    oob_commands.append(s[:s.index(command_token)])
    s = s[s.index(command_token):]
    while command_token in s:
        if command_token in s[s.index(command_token)+1:]:
            # Index of the NEXT oobcommand
            # Worth mentioning that, at this point, the first char is most likely the token.
            nindex = s[s.index(command_token)+1:].index(command_token)
            oob_commands.append(unicode(s[s.index(command_token):nindex]))
            s = s[nindex:]
        else:
            # This is the last command
            oob_commands.append(unicode(s))
            s = "" # be done

    return oob_commands
    

#Executes the out-of-band commands and returns the natural language response produced   
def executeOOB(content,usr,bot):

    response = {}
    response['nl_response']=""

    # Convert content to utf-8 if not already
    content = toUTF8(content)
    
    splitted = splitOOB(content)
    
    commands = [unicode(c) for c in splitted if u"¬" in c]
    
    while commands != []:
        # Get the first command
        current = commands.pop(0)
        
        if current == "":
            continue

        if u"¬sendMaia" in current:
            # Send the response to maia, and re-append the response commands.
            maia_response = sendMaia(current,bot,usr)
            commands +=splitOOB(maia_response)
        
        elif u"¬maiaResponse" in current:
            # Send the response to CS, and re-process
            cs_output=sendChatScript(current,bot,usr) 
            commands +=splitOOB(cs_output)
            print commands
            
        elif u"¬resource" in current:
            # The URL to show in the iframe
            response['resource'] = current.replace(u"¬resource", u"")
            
        elif u"¬label" in current:
            # The label we are using for the data
            response['label'] =  current.replace(u"¬label", u"")
        
        elif u'¬id' in current:
            # For some reason, chatscript is not reading properly the urls OoB.
            # This is a hacky way to work around it
            url_id = (current.replace(u"¬resource", u"")).strip()
            response['resource'] = vademecum_url.format(url=url_id)
        
        elif u"¬" not in current:
            #It's natural language, no OOB:
            response['nl_response']+=current
        else:
            logger.error("OOB with execution not found: "+current)

    return response

#Sends an input to ChatScript to process it
def sendChatScript(query, bot, user):
    
    # Convert the params to utf-8 if not already
    query = toUTF8(query)
    bot = toUTF8(bot)
    user = toUTF8(user)
    
    logger.info(u"[user: {user}] ChatScript input: {input}".format(user=user, input=query))
    
    #Remove special characters and lower case
    #query = sub(u'["\'¿¡@#$]', '', query)
    #query = query.lower()
    #removeacc = ''.join((c for c in unicodedata.normalize('NFD',query) if unicodedata.category(c) != 'Mn'))
    #query=removeacc.decode()
    bot = bot.lower()
    
    # Message to server is 3 strings-   username, botname, message     
    # Each part must be null-terminated with \0
    socketmsg = user+u"\0"+bot+u"\0"+query +u"\0"
    s = socket()
    try:

        s.connect((cs_tcp_ip, cs_tcp_port))
        s.send(socketmsg.encode("utf-8"))
        data = ""
        while 1:
            rcvd = s.recv(cs_buffer_size)
            if not rcvd:
                break
            data = data + rcvd

        data = toUTF8(data)
        s.close()
        
    #except socket.error, e:
    except Exception, e:
        logger.error("Conexion a %s on port %s failed: %s" % (cs_tcp_ip, cs_tcp_port, e))
        data = u"ChatScript connection error"

    logger.info(u"[user: {user}] ChatScript output: {output}".format(user=user, output=data))
    return unicode(data)

#Renders the response in Json
def renderJson(query, response, bot, user, fresponse, resource, label):
    # We shouold really make this a little less hardcoded, and more elegant.

    for special, replacement in cs_tokens.iteritems():
        response = response.replace(special, replacement)
    
    # TODO: I should probably remove this "hard-coded" tags, and place them in a config.
    response_dict = {}
    response_dict['dialog'] = {}
    response_dict['dialog']['sessionid'] = user
    response_dict['dialog']['user'] = user
    response_dict['dialog']['bot'] = bot
    response_dict['dialog']['q'] = query
    response_dict['dialog']['response'] = response
    response_dict['dialog']['full_response'] = fresponse
    print("URL: " + resource)
    if(resource != ""):
        response_dict['dialog']['url'] = resource
    response_dict['dialog']['topic'] = label

    # This is a bit... why on earth are these hardcoded???
    response_dict['dialog']['test'] = "General Saludo"
    response_dict['dialog']['flashsrc'] = "../flash/happy.swf"
    response_dict['dialog']['state'] = "happy"
    response_dict['dialog']['mood'] = "happy"

    return json.dumps(response_dict)

    
#Maia functions

#Sends a message to the Maia network
def sendMaia(msg,bot,usr):
    
    if u"¬sendMaia " in msg:
        msg = re.sub('¬sendMaia ','',msg)
    maia.send(msg + " [user "+usr+"]", usr)

    response = ""

    # Keeps waiting for new messages until a timeout
    response = unicode(maia.wait_for_message(2, usr))
    
    # Logs
    try:
        data = json.loads(response, ensure_ascii=False)
        logger.info(u"[user: {user}] Received response from maia about {label}".format(user=usr, label=data['title']))
    except:
        # If not json, probably a message form the Agent-system
	print(response.encode('utf-8'))
        logger.info(u"[user: {user}] Received not-json message from maia.".format(user=usr))
    logger.debug(u"[user: {user}] Full response: {response}".format(user=usr, response=response))
    
    return response

# convert given string to utf-8
def toUTF8(string):
    if type(string) == str:
        string = unicode(string, "utf-8")
    return string
    
if __name__ == '__main__':
    app.debug = True
    app.run(host=host_name)
