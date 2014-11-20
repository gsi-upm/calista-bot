# [SOLR Calista](https://github.com/gsi-upm/calista-bot/solr-elearning)

Linked Open Data Server module for Calista-Bot project. It makes use of the 4.10.2
 version of [Apache Solr](http://lucene.apache.org/solr/). This module contains 
 a connector between the Solr Server and the maia service, which has previously 
 been populated with the scrapped data (provided, as well as a solrconfig.xml, and a
 schema.xml for the solr in the src/main/resources and src/main/resources/solr folders).
   
 For example, in the personal assistance system for a e-learning platform, when
 the user sends a question and no information is found in the KB, this server
 is ready to provide the needed information. 



###  Launching the module

Assuming the solr server is already configured, edit the config file to provide with the server URL and the core,
and build the project:

```bash
:~/calista-bot/solr-elearning/$ mvn package
```

And just launch it:

 ```bash
:~/calista-bot/solr-elearning$ java -jar target/solr-elearning-jar-with-dependencies.jar -c solr-elearning.properties
```

## License

Copyright 2014 UPM-GSI: Group of Intelligent Systems - Universidad Politécnica de Madrid (UPM)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.