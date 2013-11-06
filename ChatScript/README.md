# [ChatScript-Bot Calista](https://github.com/gsi-upm/calista-bot/ChatScript)


Chatbot module for Calista-Bot project. It makes use of [ChatScript](http://sourceforge.net/projects/chatscript/) 2.0. The chatbot is able to maintain a conversation in natural language with the user. It applies the patterns specified in its corpus, which usually produce both a natural language response and out of band commands (hidden for the user) that are handled by the front-end controller. 



###Launching the Chatbot module

ChatScript 2.0 together with the corpus of the chatbot of our project in this folder of the project repository.

To launch the service ChatScript on Windows, the following command should be run inside the ChatScript folder:
	chatscript


On Linux, the following command should be run inside the ChatScript folder:
	./LinuxChatScript32
	
Once the service is launched, it will start listening to incoming queries.

	
	
###Compiling the chatbot corpus

The chatbot corpus files are usually found in the /RAWDATA/ folder. They can be compiled and loaded by running [ChatScript](http://sourceforge.net/projects/chatscript/) locally:
To launch ChatScript locally on Windows, the following command should be run inside the ChatScript folder:
	chatscript local

On Linux, the following command should be run inside the ChatScript folder:
	./LinuxChatScript32 local

Once ChatScript is locally launched, we can use the following command to build and load the corpus:
	:build 1 
	
	
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

This project has been developed as the master thesis of [Javier Herrera](https://github.com/javiherrera) under the tutelage of [Miguel Coronado](https://github.com/miguelcb84) and the supervision of [Carlos A. Iglesias](https://github.com/cif2cif) at [gsi-upm](https://github.com/gsi-upm)