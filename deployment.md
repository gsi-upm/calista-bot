#Calista-bot deployment 

This document aims to provide a general walk-trough on how to deploy the calista bot on a fresh system. It asumes you have a basic knowledge on both the bot and the tools used, and the required tools (maven, javac, a webserver and so on) installed.

## Order of deployment

The bot is composed of several subsystems, and some (namely, Maia and Chatscript) need to be started before the others. So, the recommended course of action is:
1. Maia
2. Chatscript
3. Front-end controller
4. Front-end client
5. SirenDB
6. Agent System 

Keep in mind all these need to be running at the same time, so you need to launch them in different terminals. Screen or tmux are a good choice for this.

## Maia

The maia server is avaliable on [github](https://github.com/gsi-upm/maia). You should be able to just download it, and launch the nodeserver:
```bash
:~/Maia$ node nodeserver/bin/maia
```
Further details on how to deploy maia can be found in its repository.

## Chatscript

Before being able to launch chatscript, if you are using a 64 bit system, you need to install several 32-bit libraries:

```bash
:~/$ sudo dpkg --add-architecture i386
:~/$ sudo apt-get update
:~/$ sudo apt-get install lib32gcc1 lib32stdc++6
```

For the first launch of Chatscript, you need to compile the corpus. Therefore, you have to launch chatscript on localmode first, issue the build commands, and then launch it as a server. So:
```bash
:~/calista-bot/ChatScript$ ./LinuxChatSscript32 local
[...]
Enter user name: username
username: > :build 1
```

Once the process is complete (you may need to skip a few warnings), close chatscript using ctrl+c and launch it as a server:
```bash
:~/calista-bot/ChatScript$ ./LinuxChatSscript32
```

Keep in mind it will bind to the 1024 port.

## Front-end controller

We need the websocket-client library for the controller:
```bash
:~/$ pip install websocket-client
```



The talkbot controller binds, by default, to the 8090 port. You need to change the host at the last line of the FE-Controller/talkbot.py file. Once is set to the appropriate hostname, just launch it:
```bash
:~/calista-bot/FE-Controller$ python talkbot.py
```

Keep in mind the host and the port, you will need them for the nex step

## Front-end client

The client for the bot is a web application. You just need to place the Chat-client on a webserver (like Apache or Nginx) html folder, and edit the popup.html file, so the form action attribute points to the host and port from the previous step.

## SirenDB

For the time being, there is no pre-build jar provided, so you need to compile the sources into a .jar. First, we need to download and compile the sirendb libraries from [its repository](https://github.com/rdelbru/SIREn), and then build our package:
```bash
:~/SIREn/$ mvn package
:~/SIREn/$ mvn install:install-file -Dfile=siren-core/target/siren-core-1.0.jar -DgroupId=org.sindice.siren -DartifactId=siren-core -Dversion=1.0 -Dpackaging=jar
:~/SIREn/$ mvn install:install-file -Dfile=siren-qparser/target/siren-qparser-1.0.jar -DgroupId=org.sindice.siren -DartifactId=siren-qparser -Dversion=1.0 -Dpackaging=jar
:~/SIREn/$ mvn install:install-file -Dfile=siren-solr/target/siren-solr-1.0.jar -DgroupId=org.sindice.siren -DartifactId=siren-solr -Dversion=1.0 -Dpackaging=jar
```

Now, the rest of the dependencies should be handle by maven, so just run:

```bash
:~/calista-bot/siren-elearning$ mvn package
```

Once you have the jar, edit the siren-elearning.properties and set the maia uri to point to your maia server, and launch siren:
```bash
:~/calista-bot/siren-elearning$ java -jar target/siren-elearning-jar-with-dependencies.jar -c siren-elearning.properties
```

## Agent system

As with siren, no pre-compiled jar is provided, so you will also need to build the project. However, at this point there is no configuration file for the agent system, so you need to edit two sources to point to the maia server. Specifically, you need to edit Agent-system/src/java/maia/send.java and the Agent-system/src/java/maia/start.java, and change the 'uri' variable to the maia server uri.

After setting the server uri, build the sources using maven:
```bash
:~/calista-bot/Agent-system$ mvn package
```

Now, in order to launch the jason system, just launch the jason_elearning-jar-with-dependencies.jar. However, this will require an XServer, so, in order to launch it on a server, you need to use a X11 virtual server, like xvfb:
```bash
:~/calista-bot/Agent-system$ export DISPLAY=:1
:~/calista-bot/Agent-system$ Xvfb $DISPLAY -screen 0 1024x768x24+32 >>/dev/null 2>&1 &
:~/calista-bot/Agent-system$ java -jar target/jason_elearning-jar-with-dependencies jason_elearning.mas2j
```


