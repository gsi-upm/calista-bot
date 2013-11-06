# [SIREn-Bot Calista](https://github.com/gsi-upm/calista-bot/SIREn)


Linked Open Data Server module for Calista-Bot project. It makes use of the 1.0 commercial version of [SIREn](http://siren.sindice.com/). This module contains indexed information, which has been previously added by the system administrator. For example, in the personal assistance system for a e-learning platform, when the user sends a question and no information is found in the KB, this server is ready to provide the needed information. 



###Launching the Linked Open Data Server module


The Linked Open Data Server module makes use of the 1.0 commercial version of [SIREn](http://siren.sindice.com/). This commercial version can be requested to [Sindice](http://www.sindice.com/). Also, a development version of SIREn can be found in the [SIREn repository](https://github.com/rdelbru/SIREn/). 


The first using SIREn, we need to compile the source code. We can do it by running (needs [Maven](http://maven.apache.org) installed):

	mvn clean package assembly:single 


Once the project is compiled, we should be able run any of its sample classes:

	java -cp ./target/siren-jar-with-dependencies.jar org.sindice.siren.demo.bnb.BNBDemo


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
