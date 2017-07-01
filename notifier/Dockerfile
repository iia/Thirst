FROM debian:stable

RUN apt-get update && apt-get install \
python-pip \
libio-socket-ssl-perl \
libnet-ssleay-perl \
sendemail -y --force-yes

RUN pip install cherrypy

ADD ./thirst-notifier.py /root/thirst-notifier.py

EXPOSE 4200

CMD ["sh", "-c", "/usr/bin/python /root/thirst-notifier.py"]
