import time
import logging
import simplejson
import errno
from config import get_config_option, get_config_path

from qpid.messaging import *
from qpid.util import URL
from qpid.log import enable, DEBUG, WARN

__all__ = ["AgoConnection"]

class AgoResponse:
    """This class represents a response as obtained from AgoConnection.send_request"""
    def __init__(self, response):
        self.response = response
        if self.is_ok():
            self.root = self.response["result"]
        elif self.is_error():
            self.root = self.response["error"]
        else:
            raise Exception("Invalid response, neither result or error present")

    def is_error(self):
        return "error" in self.response

    def is_ok(self):
        return "result" in self.response

    def identifier(self):
        return self.root["identifier"];

    def message(self):
        if "message" in self.root:
            return self.root["message"];

        return None

    def data(self):
        if "data" in self.root:
            return self.root["data"];

        return None

class ResponseError(Exception):
    def __init__(self, response):
        if not response.is_error():
            raise Exception("Not an error response")

        self.response = response

        super(ResponseError, self).__init__(self.message())
    
    def identifier(self):
        return self.response.identifier()

    def message(self):
        return self.response.message()

    def data(self):
        return self.response.data()

class AgoConnection:
    """This is class will handle the connection to ago control."""
    def __init__(self, instance):
        """The constructor."""
        self.instance = instance
        self.uuidmap_file = get_config_path('uuidmap/' + self.instance + '.json')
        self.log = logging.getLogger('AgoConnection')

        broker = str(get_config_option("system", "broker", "localhost"))
        username = str(get_config_option("system", "username", "agocontrol"))
        password = str(get_config_option("system", "password", "letmein"))

        self.log.debug("Connecting to broker %s", broker)
        self.connection = Connection(broker,
            username=username, password=password, reconnect=True)
        self.connection.open()
        self.session = self.connection.session()
        self.receiver = self.session.receiver(
            "agocontrol; {create: always, node: {type: topic}}")
        self.sender = self.session.sender(
            "agocontrol; {create: always, node: {type: topic}}")
        self.devices = {}
        self.uuids = {}
        self.handler = None
        self.eventhandler = None
        self.agocontroller = None
        self.load_uuid_map()

    def __del__(self):
        self.shutdown()

    def shutdown(self):
        if self.session:
            self.session.acknowledge()
            self.session.close()
            self.session = None

        if self.connection:
            self.connection.close()
            self.connection = None

    def add_handler(self, handler):
        """Add a command handler to be called when
        a command for a local device arrives."""
        self.handler = handler

    def add_event_handler(self, eventhandler):
        """Add an event handler to be called when an event arrives."""
        self.eventhandler = eventhandler

    def internal_id_to_uuid(self, internalid):
        """Convert a local (internal) id to an agocontrol UUID."""
        for uuid in self.uuids:
            if (self.uuids[uuid] == internalid):
                return uuid

    def uuid_to_internal_id(self, uuid):
        """Convert an agocontrol UUID to a local (internal) id."""
        try:
            return self.uuids[uuid]
        except KeyError:
            self.log.warning("Cannot translate uuid %s to internal id", uuid)
            return None

    def store_uuid_map(self):
        """Store the mapping (dict) of UUIDs to
        internal ids into a JSON file."""
        try:
            with open(self.uuidmap_file, 'w') as outfile:
                simplejson.dump(self.uuids, outfile)
        except (OSError, IOError) as exception:
            self.log.error("Cannot write uuid map file: %s", exception)
        except ValueError, exception:  # includes simplejson error
            self.log.error("Cannot encode uuid map: %s", exception)

    def load_uuid_map(self):
        """Read the mapping (dict) of UUIDs to
        internal ids from a JSON file."""
        try:
            with open(self.uuidmap_file, 'r') as infile:
                self.uuids = simplejson.load(infile)
        except (OSError, IOError) as exception:
            if exception.errno == errno.ENOENT:
                # This is not fatal, it just haven't been created yet
                self.log.debug("Cannot find uuid map file: %s", exception)
            else:
                self.log.error("Cannot load uuid map file: %s", exception)
        except ValueError, exception:  # includes simplejson error
            self.log.error("Cannot decode uuid map from file: %s", exception)

    def emit_device_announce(self, uuid, device):
        """Send a device announce event, this will
        be honored by the resolver component.
        You can find more information regarding the resolver
        here: http://wiki.agocontrol.com/index.php/Resolver """
        content = {}
        content["devicetype"] = device["devicetype"]
        content["uuid"] = uuid
        content["internalid"] = device["internalid"]
        content["handled-by"] = self.instance
        self.send_message("event.device.announce", content)

    def emit_device_remove(self, uuid):
        """Send a device remove event to the resolver"""
        content = {}
        content["uuid"] = uuid
        self.send_message("event.device.remove", content)

    def emit_device_stale(self, uuid, stale):
        """Send a device stale event to the resolver"""
        content = {}
        content["uuid"] = uuid
        content["stale"] = stale
        self.send_message("event.device.stale", content)

    def add_device(self, internalid, devicetype):
        """Add a device. Announcement to ago control will happen
        automatically. Commands to this device will be dispatched
        to the command handler.
        The devicetype corresponds to an entry in the schema."""
        if (self.internal_id_to_uuid(internalid) is None):
            self.uuids[str(uuid4())] = internalid
            self.store_uuid_map()
        device = {}
        device["devicetype"] = devicetype
        device["internalid"] = internalid
        device["stale"] = 0
        self.devices[self.internal_id_to_uuid(internalid)] = device
        self.emit_device_announce(self.internal_id_to_uuid(internalid), device)

    def remove_device(self, internalid):
        """Remove a device."""
        if (self.internal_id_to_uuid(internalid) is not None):
            self.emit_device_remove(self.internal_id_to_uuid(internalid))
            del self.devices[self.internal_id_to_uuid(internalid)]

    def suspend_device(self, internalid):
        """suspend a device"""
        uuid = self.internal_id_to_uuid(internalid)
        if uuid:
            self.devices[uuid]["stale"] = 1
            self.emit_device_stale(uuid, 1)

    def resume_device(self, internalid):
        """resume a device"""
        uuid = self.internal_id_to_uuid(internalid)
        if uuid:
            self.devices[uuid]["stale"] = 0
            self.emit_device_stale(uuid, 0)

    def is_device_stale(self, internalid):
        """return True if a device is stale"""
        uuid = self.internal_id_to_uuid(internalid)
        if uuid:
            if self.devices[uuid]["stale"]==0:
                return False
            else:
                return True
        else:
            return False

    def send_message(self, content):
        """Send message without subject."""
        return self.send_message(None, content)

    def send_message(self, subject, content):
        """Method to send an agocontrol message with a subject."""
        _content = content
        _content["instance"] = self.instance
        if self.log.isEnabledFor(logging.TRACE):
            self.log.trace("Sending message [sub=%s]: %s", subject, content)

        try:
            message = Message(content=_content, subject=subject)
            self.sender.send(message)
            return True
        except SendError, exception:
            self.log.error("Failed to send message: %s", exception)
            return False

    def send_request(self, content):
        """Send message and fetch reply.
        
        Response will be an AgoResponse object
        """
        _content = content
        _content["instance"] = self.instance
        try:
            replyuuid = str(uuid4())
            replyreceiver = self.session.receiver(
                "reply-%s; {create: always, delete: always}" % replyuuid)
            message = Message(content=_content)
            message.reply_to = 'reply-%s' % replyuuid

            if self.log.isEnabledFor(logging.TRACE):
                self.log.trace("Sending message [reply-to=%s]: %s", message.reply_to, content)

            self.sender.send(message)
            replymessage = replyreceiver.fetch(timeout=3)
            self.session.acknowledge()

            response = AgoResponse(replymessage.content)

            if self.log.isEnabledFor(logging.TRACE):
                self.log.trace("Received response: %s", response.response)

            return response

        except Empty:
            self.log.warn("Timeout waiting for reply to %s", content)
            return AgoResponse({"error": {"message":"no.reply"}})
        except ReceiverError:
            self.log.warn("ReceiverError waiting for reply to %s", content)
            return AgoResponse({"error": {"message":"receiver.error"}})
        except SendError:
            self.log.warn("SendError when sending %s", content)
            return AgoResponse({"error": {"message":"send.error"}})
        finally:
            replyreceiver.close()

    def send_message_reply(self, content):
        """Send message and fetch reply.
        This is deprecated! Use send_request instead.
        """
        _content = content
        _content["instance"] = self.instance
        try:
            replyuuid = str(uuid4())
            replyreceiver = self.session.receiver(
                "reply-%s; {create: always, delete: always}" % replyuuid)
            message = Message(content=_content)
            message.reply_to = 'reply-%s' % replyuuid

            if self.log.isEnabledFor(logging.TRACE):
                self.log.trace("Sending message [reply-to=%s]: %s", message.reply_to, content)

            self.sender.send(message)
            replymessage = replyreceiver.fetch(timeout=3)
            self.session.acknowledge()
        except Empty:
            replymessage = None
        except ReceiverError:
            replymessage = None
        except SendError:
            replymessage = False
        finally:
            replyreceiver.close()

        return replymessage

    def get_inventory(self):
        """Returns the inventory from the resolver. Return value is a dict"""
        content = {}
        content["command"] = "inventory"
        response = self.send_request(content)
        if response.is_ok():
            return response.data()
        else:
            # TODO: Report errors properly?
            return {}
            #raise ResponseError(response)

    def get_agocontroller(self, inventory=None):
        """Returns the uuid of the agocontroller device"""
        if self.agocontroller:
            return self.agocontroller

        retry = 10
        while retry > 0:
            try:
                if inventory == None:
                    # May be null if not passed in first time
                    inventory = self.get_inventory()

                if inventory != None and "devices" in inventory:
                    devices = inventory['devices']
                    for uuid in devices.keys():
                        d = devices[uuid]
                        if d == None:
                            continue

                        if d['devicetype'] == 'agocontroller':
                            self.log.debug("agoController found: %s", uuid)
                            self.agocontroller = uuid
                            return uuid
            except ResponseError as e:
                self.log.warning("Unable to resolve agocontroller (%s), retrying", e)
            else:
#                self.log.warning("Unable to resolve agocontroller, not in inventory response? retrying")
                self.log.warning("Unable to resolve agocontroller, retrying")

            inventory = None
            time.sleep(1)
            retry -= 1

        self.log.warning("Failed to resolve agocontroller, giving up")
        return None

    def emit_event(self, internal_id, event_type, level, unit):
        """This will send an event. Ensure level is of correct data type for the event!"""
        content = {}
        content["uuid"] = self.internal_id_to_uuid(internal_id)
        # XXX: C++ version will call Variant.parse on any string values,
        # which may convert it to other type i.e. int if it is
        # a plain int. There does not seem to be any similar qpid
        # function in Python.
        content["level"] = level
        content["unit"] = unit
        return self.send_message(event_type, content)

    def emit_event_raw(self, internal_id, event_type, content):
        """This will send content as event"""
        _content = content
        _content["uuid"] = self.internal_id_to_uuid(internal_id)
        return self.send_message(event_type, content)

    def report_devices(self):
        """Report all our devices."""
        self.log.debug("Reporting child devices")
        for device in self.devices:
            #only report not stale device
            if not self.devices[device].has_key("stale"):
                self.devices[device]["stale"] = 0
            if self.devices[device]["stale"]==0:
                self.emit_device_announce(device, self.devices[device])

    def _sendreply(self, addr, content):
        """Internal used to send a reply."""
        if self.log.isEnabledFor(logging.TRACE):
            self.log.trace("Sending reply to %s: %s", addr, content)

        try:
            replysession = self.connection.session()
            replysender = replysession.sender(addr)
            response = Message(content)
            replysender.send(response)
        except SendError, exception:
            self.log.error("Failed to send reply: %s", exception)
        except AttributeError, exception:
            self.log.error("Failed to encode reply: %s", exception)
        except MessagingError, exception:
            self.log.error("Failed to send reply message: %s", exception)
        finally:
            replysession.close()

    def run(self):
        """This will start command and event handling.
        Be aware that this is blocking."""
        self.log.debug("Startup complete, waiting for messages")
        while self.connection:
            try:
                message = self.receiver.fetch()
                self.session.acknowledge()
                if self.log.isEnabledFor(logging.TRACE):
                    self.log.trace("Processing message [src=%s, sub=%s]: %s",
                            message.reply_to, message.subject, message.content)

                if (message.content and 'command' in message.content):
                    if (message.content['command'] == 'discover'):
                        self.report_devices()
                    else:
                        if ('uuid' in message.content and
                            message.content['uuid'] in self.devices):
                            #this is for one of our children
                            myid = self.uuid_to_internal_id(
                                message.content["uuid"])
                            if (myid is not None and self.handler):
                                returnval = self.handler(myid, message.content)
                                if (message.reply_to):
                                    replydata = {}
                                    if (isinstance(returnval, dict)):
                                        replydata = returnval
                                    else:
                                        replydata["result"] = returnval
                                    self._sendreply(
                                        message.reply_to, replydata)
                if (message.subject):
                    if ('event' in message.subject and self.eventhandler):
                        self.eventhandler(message.subject, message.content)
            except Empty, exception:
                pass

            except ReceiverError, exception:
                self.log.error("Error whlie receiving message: %s", exception)
                time.sleep(0.05)

            except LinkClosed:
                # connection explicitly closed
                break

