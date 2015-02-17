#!/usr/bin/env python
# coding=utf-8
import os, sys
from setuptools.command.sdist import re_finder

sys.path.append(os.path.join(os.path.dirname(__file__), 'lib'))
sys.path.append(os.path.join(os.path.dirname(__file__), 'lib/ext'))

from bottle import route, run, request, response
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
response.content_type = 'application/json'


#ChatScript variables
cs_buffer_size = 1024
cs_tcp_ip = '127.0.0.1'
cs_tcp_port = 1024
facts_dir="facts/"
cs_facts_dir = this_dir+"/../ChatScript/"+facts_dir

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

# General config
host_name = 'localhost'
host_port = 8090


@route('/')
def rootURL():
    return TalkToBot()  
    
@route('/TalkToBot') 
def TalkToBot():
    
    feedback=request.query.feedback or '0'
    if (feedback != '0'):
        saveFeedback()
        return;
    
    
    #Collect URI parameters
    query_q = unidecode(request.query.q)
    query_user = request.query.user or 'anonymous'
    query_bot = request.query.bot or 'Duke'
    query_lang = request.query.lang or 'es'

    full_response = '0' #Default values
    
    #Send user input to ChatScript, get response
    cs_output = sendChatScript(query_q.lower(), query_bot, query_user) 
    
    #Split conversation response and out-of-band commands (example: Hello again [sendmaia assert returning])
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
    if '@fullresponse' in nl_response:
        full_response= '1'
        nl_response=re.sub("@fullresponse","",nl_response)


    #Render natural language response in JSON format
    return renderJson(query_q, nl_response, query_bot, query_user, full_response, oob_resource, oob_label)

def saveFeedback():

    log_text="feedback using bot '"+request.query.bot+"' in question '"+request.query.q+"' with answer given '"+request.query.response
    
    if (request.query.feedback=='2'):
        logger.error("[user: "+request.query.user+"] Negative "+log_text)
    else:
        logger.info("[user: "+request.query.user+"] Positive "+log_text)

    

#Produces an array with the natural language response as first element, and OOB tags the rest
def splitOOB(s):
    response_array = []
    response_array.append(re.sub('\[[^\]]*\]','',s))
    oob_array = re.findall('\[[^\]]*\]',s)
    
    for command in oob_array:
        response_array.append(command)
        
    return response_array;

#Executes the out-of-band commands and returns the natural language response produced   
def executeOOB(content,usr,bot):

    response = {}
    response['nl_response']=""

    
    while ("[" in content):
        
        #if (("[sendmaia" not in content) and ("[updatekb" not in content) and ("[sendcs" not in content)):
        #    logger.error("OOB with execution not found: "+content)
        #    content=""
            
        if "[sendmaia" in content:
            content=sendMaia(content,bot,usr)
        
        elif "[sendcs" in content:
            cs_output=sendChatScript(content,bot,usr) 
            cs_output_array=splitOOB(cs_output)
            response['nl_response'] =response['nl_response'] + cs_output_array[0]
            cs_output_array.pop(0);
            content=""
            for command in cs_output_array:
                content+=command;
        elif "[resource" in content:
            response['resource'] = content.replace("[resource", "")[0:content.index("]")]
            # Delete the "resource" content, and keep going.
            content = content[content.index("]")+1:]
        elif "[label" in content:
            response['label'] = content.replace("[label", "").replace("]", "")
            content = ""
        else:
            logger.error("OOB with execution not found: "+content)
            content=""


    return response

#Sends an input to ChatScript to process it
def sendChatScript(query, bot, user):

    logger.info("[user: {user}] ChatScript input: {input}".format(user=user, input=query))
    
    if "[sendcs" in query:
        query = re.sub('sendcs ','',query)
        query = re.sub('\[','',query)
        query = re.sub('\]','',query)   
        query = re.sub('\(','[',query)
        query = re.sub('\)',']',query)  


    #Remove special characters and lower case
    query = sub('["\'¿¡@#$]', '', query)
    query = query.lower()
    removeacc = ''.join((c for c in unicodedata.normalize('NFD',unicode(query)) if unicodedata.category(c) != 'Mn'))
    query=removeacc.decode()
    bot = bot.lower()


    #Message to server is 3 strings-   username, botname, message     
    socketmsg = user+"\0"+bot+"\0"+query 

    s = socket()
    try:
        s.connect((cs_tcp_ip, cs_tcp_port))
        s.send(socketmsg)
        data = s.recv(cs_buffer_size)
        s.close()
        
    #except socket.error, e:
    except Exception, e:
        logger.error("Conexion a %s on port %s failed: %s" % (cs_tcp_ip, cs_tcp_port, e))
        data = "ChatScript connection error"

    logger.info("[user: {user}] ChatScript output: {output}".format(user=user, output=data))
    return data

#Renders the response in Json
def renderJson(query, response, bot, user, fresponse, resource, label):
    # We shouold really make this a little less hardcoded, and more elegant.

    response = response.replace('"', '\\"')
    
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
    
    if "[sendmaia " in msg:
        msg = re.sub('sendmaia ','',msg)
    
    maia.send(msg + " [user "+usr+"]", usr)

    response = ""

    # Keeps waiting for new messages until a timeout
    response = maia.wait_for_message(1, usr)

    # Logs
    try:
        data = json.loads(response)
        logger.info("[user: {user}] Received response from maia about {label}".format(user=usr, label=data['label']))
    except:
        # If not json, probably a message form the Agent-system
        logger.info("[user: {user}] Received not-json message from maia.".format(user=usr))
    logger.debug("[user: {user}] Full response: {response}".format(user=usr, response=response))

    return response
    
if __name__ == '__main__':
    run(host=host_name, port=host_port, debug=True)
