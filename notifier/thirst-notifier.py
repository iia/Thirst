import os
import sys
import cherrypy
import subprocess

# Service parameters
SERVICE_ON_IP = "0.0.0.0"
SERVICE_ON_PORT = 4200
SERVICE_ON_MOUNT_POINT = "/thirst/notifier"

# SMTP server parameters.
SMTP_SERVER   = os.environ['THIRST_NOTIFIER_SMTP_SERVER']
SMTP_PORT     = os.environ['THIRST_NOTIFIER_SMTP_PORT']
SMTP_USER     = os.environ['THIRST_NOTIFIER_SMTP_USER']
SMTP_PASSWORD = os.environ['THIRST_NOTIFIER_SMTP_PASSWORD']
SMTP_USE_TLS  = os.environ['THIRST_NOTIFIER_SMTP_USE_TLS']

if not SMTP_SERVER or \
   SMTP_SERVER == "" or \
   not SMTP_PORT or \
   SMTP_PORT == "" or \
   not SMTP_USER or \
   SMTP_USER == "" or \
   not SMTP_PASSWORD or \
   SMTP_PASSWORD == "" or \
   not SMTP_USE_TLS or \
   SMTP_USE_TLS == "":
      print "Error: Environment variables not properly set"
      exit(1)

class ThirstNotifier(object):
    def verify_request(self, request):
        def verify_email_address(email_address):
            at_split = email_address.split("@")

            if len(at_split) != 2:
                return False

            if at_split[1].count(".") < 1:
                return False

            return True

        message = ""
        status = False

        if len(request) != 4:
            return False, "Invalid request length"

        if 'from' not in request:
            return False, "From field missing"

        if not isinstance(request['from'], basestring):
            return False, "From field not a string"

        if request['from'] == "":
            return False, "From field is empty"

        if 'to' not in request:
            return False, "To field missing"

        if not isinstance(request['to'], basestring):
            return False, "To field not a string"

        if request['to'] == "":
            return False, "To field is empty"

        if 'subject' not in request:
            return False, "Subject field missing"

        if not isinstance(request['subject'], basestring):
            return False, "Subject field not a string"

        if request['subject'] == "":
            return False, "Subject field is empty"

        if 'message' not in request:
            return False, "Message field missing"

        if not isinstance(request['message'], basestring):
            return False, "Message field not a string"

        if request['message'] == "":
            return False, "Message field is empty"

        if not verify_email_address(request['from']):
            return False, "Malformed from address"

        if not verify_email_address(request['to']):
            return False, "Malformed to address"

        return True, None

    @cherrypy.expose
    @cherrypy.tools.json_out()
    @cherrypy.tools.json_in()
    def send_email(self):
        request_send_email = cherrypy.request.json
        request_ok, message = self.verify_request(request_send_email)
        print request_send_email

        if not request_ok:
            return {"ok": 0, "status": message}
        else:
            cmd = "sendemail -o tls=" + \
                  SMTP_USE_TLS + \
                  " -f \"" + \
                  request_send_email["from"] + \
                  "\" -t \"" + \
                  request_send_email["to"] + \
                  "\" -u \"" + \
                  request_send_email["subject"] + \
                  "\" -m \"" + \
                  request_send_email["message"] + \
                  "\" -s " + \
                  SMTP_SERVER + \
                  ":" + \
                  str(SMTP_PORT) + \
                  " -xu \"" + \
                  SMTP_USER + \
                  "\" -xp \"" + \
                  SMTP_PASSWORD + \
                  "\""

            p = subprocess.Popen([cmd], stdout=subprocess.PIPE, shell=True)

            status = ""
            sout, serr = p.communicate()
            exit_code = p.returncode

            if sout and serr:
                status =  sout + " " + serr
            elif sout and not serr:
                status = sout
            elif not sout and serr:
                status = serr

            if exit_code != 0:
                return {"ok": 0, "status": unicode(status)}
            else:
                return {"ok": 1, "status": unicode(status)}

if __name__ == '__main__':
    # Cherrypy configuration.
    cherrypy.server.socket_host = SERVICE_ON_IP
    cherrypy.server.socket_port = SERVICE_ON_PORT

    # Start.
    cherrypy.quickstart(ThirstNotifier(), SERVICE_ON_MOUNT_POINT)
