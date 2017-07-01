The Notifier Service
====================

The notifier service is provided as a Docker image. The following instructions
assume that you have a working Docker installation.

Building the Image
------------------

A Docker image of the notifier service can be built with the following command,

    sudo docker build -t thirst-notifier:latest .

As an alternative to building and deploying the notifier service you can pull the
Docker image from Docker Hub and execute the image on any machine with a working
Docker installation. In this case use the following command to pull the image,

        sudo docker pull iia86/thirst-notifier:latest

Executing the Docker Image
--------------------------
If you built the Docker image locally then the generated Docker image can be
executed using the following command,

        sudo docker run -i -t -d -p 4200:4200 --env-file thirst-notifier.env thirst-notifier:latest

If the image was pulled from the registry then use the following command to
execute the Docker image,

        sudo docker run -i -t -d -p 4200:4200 --env-file thirst-notifier.env iia86/thirst-notifier:latest

Variables must be provided in the **thirst-notifier.env** file which are essential
for the Docker container.

Note that if you want to deploy the notifier service like this to a machine of
your choice you have to put a custom firmware on the devices that you are using.
In this case you have to modify **#define FMT_NOTIFIER_HTTP_HEADER** in the file
**firmare/src/include/main.h** according to your notifier service deployment.
Then compile and re-flash the firmware.
