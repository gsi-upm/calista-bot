#!/usr/bin/python
# coding=utf-8

from __future__ import print_function

import os, sys

from unidecode import unidecode

import flask
import requests, socket
import ast

app = flask.Flask(__name__)
app.debug = True
# Flask settings 
host='localhost'
port=4242

# Solr settings
solr = {'host': 'localhost', 'port':8990, 
        'core': 'elearning'}

# ChatScript settings:
# The agent should be randomly set for each user
# Also, I should probably make another bot, say "Arthur" to answer the questions
cs = {'bot': u'Dent', 'host':'localhost',
      'port': 1025, 'agent': u'ErrorAgent'}

default_response='Lo siento, no he podido encontrar nada sobre eso'

@app.route('/')
def rootURL():
    return qa()

@app.route('/ask')
def qa():
    """
    Receives a question and returns an answer
    """
    req = flask.request.args
    
    agent = req['username']
    response = {}
    question = unidecode(req['question'])
    response = runQuestion(question, agent)
    
    # If there is no answer after the processing, send a default one.
    if not 'answer' in response:
        response['answer'] = default_response
    
    print(u"[ASK-BOT]Answering to user: {user} question: {q} with {resp}".format(user=agent,
                                                         resp=unicode(response),
                                                         q=question))
    
    return flask.jsonify(response)


def runQuestion(question, user):
    """
    Send the question to CS and process
    the response to return the actual response
    """

    cs_response = sendChatScript(question, user)

    bot_response = runCommands(cs_response, question, user)
    
    return bot_response

def runCommands(cs_response, question, user):
    """
    Takes the different answers and split the commands
    
    """
    # The natural language response
    response = {'answer': []}

    # There is only the CS response, no commands
    if u"¬" not in cs_response:
        return {'answer' : [cs_response]}
    
    commands, nl_response = splitCommands(cs_response)
    if nl_response != "":
        response['answer'].append(nl_response)
    while commands != []:
        # Get the first element
        command = commands.pop(0)
        
        if u"¬sendSolr" in command:
            elements = command.split(' ')
            requested = elements[1]
            query = {'q': 'title:{label}'.format(label=elements[2])}
            solr_response = sendSolr(query)
            if len(solr_response) != 0:
                if not requested in response:
                    response[requested] = solr_response[0][requested]
                r_response = solr_response[0][requested]
                commands.append(u"¬solrResponse {command} {label}".format(label=r_response,
                                                                     command=requested))
            else:
                response['answer'] = [u"Lo siento, no tengo información sobre "+ elements[2]]

        elif u'¬solrLinks' in command:
            link_list = command.replace(u'¬solrLinks', '')
            link_list = link_list.replace(" ", "")
            link_list = ast.literal_eval(link_list.strip())
            links_names = []
            for link in link_list:
                l_q = {'q': 'resource:"{link}"'.format(link=link),
                       'fl': 'title'}
                l_response = sendSolr(l_q)
                if len(l_response) != 0:
                    title = l_response[0]['title']
                    links_names.append(title)
            response['related'] = links_names
        elif u"¬solrResponse" in command:
            current_response = sendChatScript(command, user)
            new_commands, new_nl = splitCommands(current_response)
            if new_nl != "":
                response['answer'].append(new_nl)
            commands += new_commands
        elif u"¬resource" in command:
            if 'resource' not in response:
                response['resource'] = command.replace(u'¬resource ', '')
        elif u"¬gambit" in command:
            sendSolr(question)
        elif u"¬label" in command:
            print("label found")
        elif u"¬links" in command:
            print(u"Links {co}".format(co=command))
        else:
            print(unidecode(u"unkown command {command}".format(command=command)))
    return response
    
def splitCommands(cs_response):
    """
    Given a string, split it into commands
    """
    commands = []
    nl_response = ""
    if u"¬" not in cs_response:
        return ([], cs_response)
    cs_response = cs_response.strip()
    if cs_response.index(u"¬") != 0:
        first_index = cs_response.index(u"¬")
        nl_response = cs_response[:first_index]
        commands = [u'¬' +command for command in cs_response[first_index:].split(u'¬')[1:]]
    else:
        l_commands = cs_response.split(u"¬")
        commands = [u'¬' + command for command in l_commands[1:]]
        
    return (commands, nl_response)

def sendChatScript(query, agent=cs['agent']):
    """
    Send the question to chatscript
    """
    
    
    # Convert the params to utf-8 if not already
    query = toUTF8(query)
    
    # Message to server is 3 strings-   username, botname, message     
    # Each part must be null-terminated with \0
    socketmsg = agent+u"\0"+cs['bot']+u"\0"+query +u"\0"
    s = socket.socket()
    try:

        s.connect((cs['host'], cs['port']))
        s.send(socketmsg.encode('utf-8'))
        data = ""
        
        rcvd = s.recv(1024)
        while len(rcvd) != 0:
            data = data + rcvd
            rcvd = s.recv(1024)
    
        data = toUTF8(data)
        s.close()
        
    #except socket.error, e:
    except Exception, e:
        data = u"ChatScript connection error"

    return data


def sendSolrDefault(question, fl=None):
    """
    Send the question to solr, using a default q:question query
    Returns a json with the answer an the response json
    {'answer': 'solr_response', 'response':{the_full_response}}
    """
    question = question.replace(' ', '+')
    
    payload = {'q':question}
    
    if fl:
        payload['fl'] = fl
    return sendSolr(payload)
    
def sendSolrEDisMax(question, weights={'title':'10', 'description':'2'}, fl=None):
    """
    Sends the question to solr, using a eDisMax query.
    Accept specifying the weights, or uses a default.
    Returns a dict with the answer and the full response.
    """
    # Scape spaces
    question = question.replace(' ', '+')
    
    qf = ' '.join([key+'^'+value for key,value in weights.iteritems()])
        
    payload = {'q':question, 'defType':'edismax',
               'lowercaseOperators':'true', 'stopwords':'true',
               'qf':qf}   
    
    if fl:
        payload['fl'] = fl
    
    solr_response = sendSolr(payload)
    
    links = solr_response['answer']['links']
    links_names = []
    links.pop
    for link in links:
        # Ignore the general "vademecum root" link
        if "http://www.dit.upm.es/~pepe/libros/vademecum/topics/3.html" not in link:
            query = {'q':'resource: "{url}"'.format(url=link)}
            q_response = sendSolr(query)
            links_names.append({'title':q_response['answer']['title'],
                                'definition':q_response['answer']['definition'],
                                'resource':link})
    
    solr_response['answer']['links'] = links_names
    return  solr_response
    
def sendSolr(payload):
    """
    Send the given query to solr
    By default. it takes the payload and adds the 'wt': 'json and 'rows':'1' params
    if they aren't present already
    """
    
    solr_url = 'http://{host}:{port}/solr/{core}/select'.format(host=solr['host'],
                                                         port=str(solr['port']),
                                                         core=solr['core'])
    
    if 'wt' not in payload:
        payload['wt'] = 'json'
    if 'rows' not in payload:
        payload['rows'] = '1'
    print('Query: {payload}'.format(payload=str(payload)))
    response = requests.get(solr_url, params=payload).json()
    
    
    return response['response']['docs']

def toUTF8(string):
    """
    Convert given string to utf-8
    """
    if type(string) == str:
        string = unicode(string, "utf-8")
    return string
    

if __name__ == '__main__':
    app.debug = True
    app.run(host=host, port=port)
    
