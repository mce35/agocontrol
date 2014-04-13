#!/usr/bin/python

# copyright (c) 2013 Harald Klein <hari+ago@vt100.at>
#

import agoclient
import smtplib
import string

client = agoclient.AgoConnection("smtp")
smtpserver = agoclient.get_config_option("smtp", "server", "mx.mail.com")
smtpport = agoclient.get_config_option("smtp", "port", "25")
smtpfrom = agoclient.get_config_option("smtp", "from", "agoman@agocontrol.com")
smtpauthrequired = agoclient.get_config_option("smtp", "authrequired", "0")
smtpuser = agoclient.get_config_option("smtp", "user", "")
smtppassword = agoclient.get_config_option("smtp", "password", "")

def messageHandler(internalid, content):
	if "command" in content:
		if content["command"] == "sendmail" and "to" in content:
			print "sending email"
			subject = "mail from agoman"
			if "subject" in content:
				subject = content["subject"]
			body = "no text"
			if "body" in content:
				body = content["body"]
			body = string.join((
				"From: %s" % smtpfrom,
				"To: %s" % content["to"],
				"Subject: %s" % subject ,
				"",
				body
				), "\r\n")
			try:
				server = smtplib.SMTP(smtpserver, smtpport)
				server.set_debuglevel(1)
				server.ehlo()
				server.starttls()
				server.ehlo()
				if smtpauthrequired == "1":
					server.login(smtpuser, smtppassword)
				server.sendmail(smtpfrom,[content["to"]], body)
				return 0
			except:
				print "error sending email, check your config"
				return -1

client.add_handler(messageHandler)

client.add_device("smtp", "smtpgateway")

client.run()

