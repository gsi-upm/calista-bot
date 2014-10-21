# coding=utf-8
import os, sys
sys.path.append(os.path.join(os.path.dirname(__file__), 'lib'))
sys.path.append(os.path.join(os.path.dirname(__file__), 'lib/ext'))

from bottle import route, run, request, response
from socket import socket
from re import sub

import maia

import inspect, unicodedata
import json
import re
import logging
import logging.handlers
import pyunitex_emb

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
#formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')

# Log to Syslog
hdlr = logging.handlers.SysLogHandler()
formatter = logging.Formatter('calistabot: %(levelname)s %(name)s %(message)s')

hdlr.setFormatter(formatter)
logger.addHandler(hdlr) 
logger.setLevel(logging.INFO)
logging.basicConfig(level=logging.DEBUG, format='%(message)s')


#Maia server variables
maia_uri = 'localhost'
maia_port = '1337'

maia = maia.Maia('ws://'+maia_uri+':'+maia_port, logger) 
maia.connect()


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
    query_q = request.query.q
    query_user = request.query.user or 'anonymous'
    query_bot = request.query.bot or 'Duke'
    query_lang = request.query.lang or 'es'

    full_response = '0' #Default values
    
    
    #Send user input to Unitex, get response    
    unitex_output = sendUnitex(query_user, query_q, query_bot, query_lang)   

    #Split Unitex out-of-band commands and not identified contents
    unitex_output_array=splitOOB(unitex_output)
    cs_input = unitex_output_array[-1]  

    
    #Send user input to ChatScript, get response
    cs_output = sendChatScript(cs_input, query_bot, query_user) 
    
    
    #Split conversation response and out-of-band commands (example: Hello again [sendmaia assert returning])
    cs_output_array=splitOOB(cs_output)
    
    #Extract the natural language response from the array
    nl_response = cs_output_array[0]
    cs_output_array.pop(0);
    
    #Proceed with further executions if needed, getting the natural language response produced
    for command in cs_output_array:
        nl_response+=executeOOB(command,query_user,query_bot);  
    
    #Detect the @full_response flag used to ask the user for feedback
    if '@fullresponse' in nl_response:
        full_response= '1'
        nl_response=re.sub("@fullresponse","",nl_response)


    #Render natural language response in JSON format
    return renderJson(query_q, nl_response, query_bot, query_user, full_response)

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
    
    nl_response=""
    
    while ("[" in content):
        
        if (("[sendmaia" not in content) and ("[updatekb" not in content) and ("[sendcs" not in content)):
            logger.error("OOB with execution not found: "+content)
            content=""
            
        if "[sendmaia" in content:
            content=sendMaia(content,bot,usr)
           
        if "[updatekb" in content:
            content=updateCsKb(content,bot,usr)
        
        if "[sendcs" in content:
            cs_output=sendChatScript(content,bot,usr) 
            cs_output_array=splitOOB(cs_output)
            nl_response+=cs_output_array[0] 
            cs_output_array.pop(0);
            content=""
            for command in cs_output_array:
                content+=command;   

    return nl_response
    
#Sends an input to Unitex to process it
def sendUnitex(user, query, bot, lang):

    logger.info("[user: {user}] Unitex input: {input}".format(user=user, input=query))
    
    #Remove special characters, lower case letters
    query = sub('["\'¿¡!?@#$]', '', query)
    query = query.lower()
    removeacc = ''.join((c for c in unicodedata.normalize('NFD',unicode(query)) if unicodedata.category(c) != 'Mn'))
    query=removeacc.decode()
    
    
    #Process the input using Unitex
    u = pyunitex_emb.Unitex()
    response_string=""

    bufferDir =this_dir+"/../Unitex/unitex_buffer"
    commonDir= this_dir +"/../Unitex/common_resources"
    botDir = this_dir + "/../Unitex/bot_resources/" + bot
    #delaDir=commonDir+"/lang_dictionary/es/delaf.bin" 
    #dicoDir=commonDir+"/lang_dictionary"
    botDir_dics = botDir+"/dictionary"

    txt_name=user
    txt=bufferDir+"/"+txt_name+".txt"
    snt=bufferDir+"/"+txt_name+".snt"
    snt_dir=bufferDir+"/"+txt_name+"_snt"
    txt_output=bufferDir+"/"+txt_name+"_o.txt"
    
    text_file = open(txt, "w")
    text_file.write(query)
    text_file.close()
    
    if not os.path.exists(snt_dir): os.makedirs(snt_dir)
    u.Convert("-s","UTF8",txt,"-o",txt+"2")
    u.Normalize(txt+"2","-r",commonDir+"/Norm.txt","-qutf8-no-bom")
    u.Convert("-s","UTF8",snt,"-o",snt+"2")
    u.Tokenize(snt+"2","-a",commonDir+"/alphabet/"+lang+"/Alphabet.txt")
    bot_dicos = []

    os.chdir(botDir_dics)
    for files in os.listdir("."):
        if files.endswith(".bin"):
            bot_dicos=bot_dicos+[botDir_dics+"/"+files]
            

    paramsDico= ["-t",snt,"-a",commonDir+"/alphabet/"+lang+"/Alphabet.txt"] + bot_dicos
    u.Dico(*paramsDico)
    paramsLocate=["-t",snt,botDir+"/graph/main_graph.fst2","-a",commonDir+"/alphabet/"+lang+"/Alphabet.txt","-L","-R","--all","-b","-Y","-m",";".join(bot_dicos)]
    u.Locate(*paramsLocate)
    u.Concord(snt_dir+'/concord.ind','-m',txt_output,"-qutf8-no-bom")
    
    with open(txt_output, 'r') as f:
        response_string=response_string+f.next()
            

    logger.info("[user: {user}] Unitex output: {output}".format(user=user, output=response_string))
    return response_string

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

#Updates the ChatScript knowledge base
def updateCsKb(content,bot,usr):    

    if "[updatekb]" in content:
        content=re.sub("\[updatekb\]","",content)

    if bot=="Duke":
        kbase="java"
    else:
        kbase="global"
    
    logger.info("[user: {user}] Updating ChatScript knowledge base kb- {kb}".format(user=usr, kb=kbase))
    
    cskbfile = cs_facts_dir+"kb-"+kbase
    
    newcontent="" 

    content_array = json.loads(content)

    #Remove special characters and lower case label
    content_array["label"]=re.sub("-","",content_array["label"])
    content_array["label"]=content_array["label"].lower()
    
    #Add the new contents to the ChatScript kb file
    for ritem in content_array:
        if ritem != "label" and ritem != "links_to":
            content=re.sub(" ","_",content_array[ritem])
            newcontent+= "\n( "+content_array["label"]+" "+ritem+" "+content+" )"
        
        if ritem == "links_to":
                for sritem in content_array["links_to"]:
                    label=re.sub("-","",sritem["label"])
                    label=label.lower()
                    newcontent+= "\n( "+content_array["label"]+" links_to "+label+" )"
    
    with open(cskbfile, 'a') as f:
        f.write(newcontent) 
    
    return "[sendcs [ import "+facts_dir+"kb-"+kbase+" "+content_array["label"]+" ] ]"
    
#Renders the response in Json
def renderJson(query, response, bot, user, fresponse):
    response = response.replace('"', '\\"')
    return "{\"dialog\": {\"sessionid\": \""+user+"\", \"user\": \""+user+"\", \"bot\": \""+bot+"\", \"q\": \""+query+"\", \"response\": \""+response+"\", \"test\": \"General Saludo\", \"url\": \"null\", \"flashsrc\": \"../flash/"+"happy"+".swf\", \"full_response\": \""+fresponse+"\", \"state\": \""+"happy"+"\", \"mood\": \""+"happy"+"\"}}"

    
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
    logger.debug("[{user: user}] Full response: {response}".format(user=usr, response=response))

     
    return response
    
if __name__ == '__main__':
    run(host='alpha.gsi.dit.upm.es', port=8090, debug=True)
