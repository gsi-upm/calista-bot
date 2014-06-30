from __future__ import print_function
import sys

import websocket
import time
import json

import os, threading, Queue

class Maia:

    def __init__ (self, uri, logger):
        """
        Creates the maia object, but does NOT create the websocket
        """

        self.uri = uri
        self.logger = logger

        # Creates the queues for the messages

        self.rcvd_msgs = Queue.Queue()
        self.lock = threading.Lock()


    def connect(self):
        """
        Connects to the maia network
        """
        self.logger.info("Subscribing to the maia network...")

        # I don't really like this, I should probably
        # just make them private methods or something...
            

        websocket.enableTrace(False)
        self.ws = websocket.WebSocketApp(self.uri, on_message = self.on_message,
                                                   on_error = self.on_error,
                                                   on_close = self.on_close,
                                                   on_open = self.on_open)
        self.ws.send_type = 'message'
        #self.ws.on_open = Maia.on_open
        self.ws.subscribe = 'message' #lista a suscribirse
        self.ws.response = ''

        self.maia_thread = maiaListener(self.ws)

        self.maia_thread.start()
            
        #ws.run_forever()

    def wait_for_message(self, timeout, user):
        """
        Waits for a message from the maia network, 
        or until the timeout triggers
        """
        
        # Dirty, pass the current query user so I could use the message

        # I get a message from the queue, with timeout.
        # If the timeout triggers, I return an empty string
        try:
            msg = self.rcvd_msgs.get(True, timeout)
            return msg
        except Queue.Empty:
            return '' 

    def send(self, data, user):
        """
        Sends a message to the maia network, without waiting for response
        """

        with self.lock:
            self.user = user
        self.logger.info('Sending to Maia {"name":"message","data": {"name" : "%s"}}' % data)
        self.ws.send('{"name":"message","data": {"name" : "%s"}}' % data)
        # Bassically, I just add it to the queue.
        #self.send_msgs.put(message)

    def on_open(self, ws):
        """
        When creating the socket, subscribe to the maia network
        """

        ws.send('{"name":"username","data": {"name" : "%s"}}' % "python_client");
        time.sleep(1);
    
        ws.send('{"name":"subscribe","data": {"name" : "%s"}}' % 'message');
        time.sleep(2)
                
    def on_message(self, ws, message):
        """
        Receives a message from the maia network.
        """
        mymsg = json.loads(message)

        #Accept only messages with an [actuator]
        if (('[' in mymsg['data']['name']) and 
             ('[assert' not in mymsg['data']['name']) and ('[retrieve' not in mymsg['data']['name']) ):
            
             # TODO:
             # The way I should probably do this: Have a dict with queues, one for each user
             # Also, do a clean-up of the  queues periodically, to prevent mem-leaks.
             user = ''
             msg = mymsg['data']['name']

             with self.lock:
                user = self.user

             # Ok, for some reason, here I used to check for an user in the message. However, the message from
             # maia doesn't return an user the way I expect (i.e., the bot client user), so I have remove the
             # check temporarily. I'll come back to this.
                
             # if user in msg:
            
             self.rcvd_msgs.put(msg)
             print(">> Received: %s" % msg)
                
    def on_error(self, ws, error):
        """
        Just print the error for the time being
        """
        
        # i should use a proper logger
        print(error, file=sys.stderr)

    def on_close(self, ws):
        """
        Unsubscribe from maia
        """
        print('I still need to figure out how to properly do this...')
        
        # Unsubscribe
        # Should I really do this?
        #self.ws.send('{"name":"unsubscribe","data": {"name" : "%s"}}' % 'message')
        #self.ws.close()

        self.maia_thread.join()

class maiaListener(threading.Thread):
    """
    Thread to listen /send messages to the maia network
    """

    def __init__(self, ws):
        super(maiaListener, self).__init__()
        
        # I want the thread to die when CTRL+C is issued, so I make it a "dameon" thread
        self.daemon = True

        self.ws = ws

    def run(self):
        """
        Keeps running
        If I need to send a message, sent it
        """
        self.ws.run_forever()

    def join(self, timeout=None):
        super(maiaListener, self).join(timeout)

