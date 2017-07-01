The Notifier Service
====================

The notifier service is provided as a Docker image. The following instructions
assume that you have a working Docker installation.

Building the Image
------------------

A Docker image of the notifier service can be built with the following command,

    sudo docker build -t thirst-notifier:latest .

Executing the Docker Image
--------------------------
The generated Docker image can be executed using the following command,

        sudo docker run -i -t -d -p 4200:4200 --env-file thirst-notifier.env thirst-notifier:latest

Variables must be provided in the **thirst-notifier.env** file which are essential
for the Docker container.

As an alternative to building and deploying the notifier service you can pull the
Docker image from Docker hub and execute the image on any machine with a working
Docker installation.

Note that if you want to deploy the notifier service like this to a machine of
your choice you have to put a custom firmware on the devices that you are using.
In this case you have to modify **#define FMT_NOTIFIER_HTTP_HEADER** in the file
**firmare/src/include/main.h** according to your notifier service deployment.
Then compile and re-flash the firmware.
