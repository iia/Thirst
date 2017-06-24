import cherrypy

class Notifier(object):
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
            return {"ok": request_ok, "status": message}
        else:
            cmd = "sendemail -o tls=yes -f \"" + \
                  request_send_email["from"] + \
                  "\" -t \"" + \
                  request_send_email["to"] + \
                  "\" -u \"" + \
                  request_send_email["subject"] + \
                  "\" -m \"" + \
                  request_send_email["message"] + \
                  "\" -s smtp.gmail.com:587 -xu \"thirst.the.project@gmail.com\" -xp \"Qqqq-1111\""

            return {"ok": True, "status": "Email sent"}

cherrypy.server.socket_host = '0.0.0.0'
cherrypy.quickstart(Notifier())
