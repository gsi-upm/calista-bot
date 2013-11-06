# [Front-end controller-Bot Calista](https://github.com/gsi-upm/calista-bot/FE-Controller)


Front-end controller module for Calista-Bot project. It makes use of the Python web framework [Bottle](http://bottlepy.org) 0.11.2. The Front-end controller manages the process followed to generate a reply for the user's query. It triggers the other modules to understand the user query, execute external actions if needed, and finally formulate the reply that is sent back to the user. 
The knowledge base (KB) contains information ready to be provided by the Chatbot when the user sends a question about a subject. It is dynamically updated when new information is fetched from the Linked Open Data Server.




###Launching the Front-end controller

For implementing our Front-end controller module, we have used [Bottle](http://bottlepy.org) 0.11.2. It can be found together with the script for our Front-end controller in this folder of the project repository.


We can launch the Front-end controller module both on Windows or Linux by running the following command inside the module folder (needs [Python](http://www.python.org) 2.7 installed):

	python talkbot.py
	


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
