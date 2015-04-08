#!/usr/bin/python
# coding=utf-8

import os, sys

import flask
import requests

app = flask.Flask(__name__)
# Flask settings 
host='localhost'
port=4242

# Solr settings
solr = {'host': 'localhost', 'port':8080, 
        'core': 'elearning'}

@app.route('/')
def rootURL():
    return "Hola mundo"

@app.route('/ask')
def ask():
    return flask.jsonify(sendSolrEDisMax("que es un for"))


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
    print(response)
    return {'answer':response['response']['docs'][0], 'response':response}

def sendChatScript(question):
    """
    Using a websocket, send the question to ChatScript
    """
    return "TODO"

if __name__ == '__main__':
    app.debug = True
    app.run(host=host, port=port)
    