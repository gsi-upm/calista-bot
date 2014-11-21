## [Calista Bot](https://github.com/gsi-upm/calista-bot)


## Introduction

This project aims to integrate the advantages of intelligent agent systems, information retrieval and Natural Language Processing in a personal assistance system.

Thanks to this integration, we have been able to provide the personal assistant the ability to solve questions and support the learning process in an e-learning platform. This opens the opportunity for the student to be able to find help in any moment, not having to resort to the teacher. At the same time, thanks to the agents system the learning process is checked and supported to be the correct one. Furthermore, for solving the student questions, our personal assistant has been given the capability to access external sources of data using information retrieval techniques, so he can also improve his existing knowledge base.

During the development, we dealt with one of the main limitations that the natural language processors nowadays have: as there is a wide range of applications for them, they usually are unable to execute orders from the user that require using external modules. We overcome this limitation by implementing over an event-based network several execution modules, as the agent system that is able to support the learning process, or the information retrieval module. 



##Architecture

In this diagram, you can see the Calista Bot arquitecture at a glance

![Calista Bot architecture](http://img849.imageshack.us/img849/9230/w8nl.png)

For a detailed explanation of each module, you can visit each subproject's repository by clicking in each folder of this repo or by clicking in the headings of the Submodules section.



## Submodules
Calista Bot consists in several modules that can be found under the subtrees of this project.
 

### [Chat client](https://github.com/gsi-upm/calista-bot/tree/master/Chat-client) 
An user interface that allows the user to communicate with the system, by chatting with the personal assistant in natural language.

![Chat client](http://img14.imageshack.us/img14/8480/x6ex.png)


### [Front-end controller and KB](https://github.com/gsi-upm/calista-bot/tree/master/FE-Controller) 
The Front-end controller manages the process followed to generate a reply for the user's query. It triggers the other modules to understand the user query, execute external actions if needed, and finally formulate the reply that is sent back to the user. 

The knowledge base (KB) contains information ready to be provided by the Chatbot when the user sends a question about a subject. It is dynamically updated when new information is fetched from the Linked Open Data Server.


### [Grammar engine (Unitex)](https://github.com/gsi-upm/calista-bot/tree/master/Unitex) 
The grammar engine is used for detecting specific patterns by applying grammars to the user query. These grammars are usually applied to match phrases containing a set of concepts specified in a list (for example, 'do-while', a concept of the Java programming language). This list of concepts can also be updated dynamically during the conversation. 

### [Chatbot (ChatScript)](https://github.com/gsi-upm/calista-bot/tree/master/ChatScript) 
The chatbot is able to maintain a conversation in natural language with the user. It applies the patterns specified in its corpus, which usually produce both a natural language response and out of band commands (hidden for the user) that are handled by the front-end controller. 

### [Event network (Maia)](https://github.com/gsi-upm/Maia) 
The event network allows the communication between the front-end controller and the external services, such as the Agent system or the Linked Open Data Server. This network is based on event messages that contain the information exchanged by these modules. 

### [Agent system (Jason)](https://github.com/gsi-upm/calista-bot/tree/master/Agent-system) 
An intelligent agent system that receives assertions collected during the conversation and provides them as beliefs to the agent(s) running on it. It provides our personal assistance system an extra interaction with the user, by triggering plans that automatically produce replies that are sent to the user.

### [Linked Open Data Server (Apache Solr)](https://github.com/gsi-upm/calista-bot/tree/master/solr-elearning) 
The Linked Open Data Server contains indexed information, which has been previously added by the system administrator. When the user sends a question and no information is found in the KB, this service is ready to provide the needed information. 




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
