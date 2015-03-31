# [ChatScript-Bot Calista](https://github.com/gsi-upm/calista-bot/ChatScript)


Chatbot module for Calista-Bot project. It makes use of [ChatScript](http://sourceforge.net/projects/chatscript/)  
The chatbot is able to maintain a conversation in natural language with the user. It applies the patterns specified in 
its corpus, which usually produce both a natural language response and out of band commands (hidden for the user) that
 are handled by the front-end controller. 

# Install

We no longer provide the ChatScript files with the bot. You can either download [ChatScript 5.1](http://sourceforge.net/projects/chatscript/files/ChatScript-5.1.zip/download)
from SourceForge, and uncompress the file in this folder, merging the RAWDATA folder, or run the getChatScript.sh script, which should download and extract everything as needed.

Once you have Chatscript, launch it in local mode, and build the bot:

    username@host:~/calista-bot/ChatScript>$ ./LinuxChatScript64 local

It will ask for an username, and give you a prompt. Execute ":build Duke"
    
    username: >:build Duke

This will produce a large output. You can now test the bot, or exit and launch it in server mode:
    
    username: >:quit
    Exiting ChatScript via Quit
    username@host:~/calista-bot/ChatScript$ ./LinuxChatScript64
    
If you are having trouble building the bot, try deleting the contents of the TMP, USERS and LIVEDATA folders.    

## License

```
Copyright 2013 UPM-GSI: Group of Intelligent Systems - Universidad Politécnica de Madrid (UPM)

Licensed under the Apache License, Version 2.0 (the "License"); 
You may not use this file except in compliance with the License. 
You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 Unless required by 
applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific 
language governing permissions and limitations under the License.
```
![GSI Logo](http://gsi.dit.upm.es/templates/jgsi/images/logo.png)

This project has been developed from the master thesis of [Javier Herrera](https://github.com/javiherrera), as part of the [Alberto Mardomingo](https://github.com/amardomingo) master thesis under the tutelage of [Miguel Coronado](https://github.com/miguelcb84) and the supervision of [Carlos A. Iglesias](https://github.com/cif2cif) at [gsi-upm](https://github.com/gsi-upm)


