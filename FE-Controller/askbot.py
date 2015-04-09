#!/usr/bin/python
# coding=utf-8

import os, sys

from unidecode import unidecode

import flask
import requests, socket

app = flask.Flask(__name__)
app.debug = True
# Flask settings 
host='localhost'
port=4242

# Solr settings
solr = {'host': 'localhost', 'port':8080, 
        'core': 'elearning'}

# ChatScript settings:
# The agent should be randomly set for each user
# Also, I should probably make another bot, say "Arthur" to answer the questions
cs = {'bot': 'Duke', 'host':'localhost',
      'port': 1024, 'agent':'ErrorAgent'}


@app.route('/')
def rootURL():
    return ask()

@app.route('/ask')
def ask():
    """
    Receives a question and returns an answer
    """
    req = flask.request.args
    
    agent = req['username'] or cs['agent']
    response = {}
    response['solr'] = sendSolrEDisMax(req['question'].replace('?', ''))['answer']
    #TODO: Change the agent for one sent from the client
    response['cs'] = sendChatScript(req['question'], cs['agent'])
    
    return flask.jsonify(response)


# TODO: Take the solr functionality to a "lib" module

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
    
    return sendSolr(payload)
    
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
    response = requests.get(solr_url, params=payload).json()
    
    return {'answer':response['response']['docs'][0], 'response':response}

def sendChatScript(question, agent):
    """
    Using a websocket, send the question to ChatScript
    """
    query = agent + '\0' + 'Duke\0' + unidecode(question) +'\0'
    
    s = socket.socket()

    s.connect((cs['host'],cs['port']))
    s.send(query)
    # Read response
    data = s.recv(1024)
    response = ""
    while data:
        response += data
        data = s.recv(1024) 
    
    return unicode(response, encoding="utf-8")

if __name__ == '__main__':
    app.debug = True
    app.run(host=host, port=port)
    