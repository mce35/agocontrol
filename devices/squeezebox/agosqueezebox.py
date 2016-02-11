#! /usr/bin/python

# Squeezebox client
# copyright (c) 2013 tang
 
import sys
import agoclient
import pylmsserver
import pylmsplaylist
import pylmslibrary
import threading
import time
import base64
import logging

host = ''
server = None
playlist = None
library = None
client = None
states = {}

STATE_OFF = 0
STATE_ON = 255
STATE_STREAM = 50
STATE_PLAY = 100
STATE_STOP = 150
STATE_PAUSE = 200

MEDIASTATE_STREAM = 'streaming'
MEDIASTATE_PLAY = 'play'
MEDIASTATE_STOP = 'stopped'
MEDIASTATE_PAUSE = 'paused'

#logging.basicConfig(filename='agosqueezebox.log', level=logging.INFO, format="%(asctime)s %(levelname)s : %(message)s")
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s : %(message)s")

#utils
def getPlayer(player_id):
    """Return player according to player_id"""
    return playlist.get_server().get_player(player_id)

def getPlayers():
    """Return all players"""
    return playlist.get_server().get_players()

def quit():
    """Exit application"""
    global playlist
    global client
    if playlist:
        playlist.stop()
        playlist = None
    if client:
        del client
        client = None
    sys.exit(0)

#squeezebox callbacks
def play_callback(player_id, song_infos, playlist_index):
    #if player is off or player is already playing, kick this callback
    logging.debug('callback PLAY: %s' % player_id)
    logging.debug('callback PLAY: song %s' % song_infos)
    if states.has_key(player_id) and states[player_id]!=STATE_PLAY and states[player_id]!=STATE_OFF and states[player_id]!=STATE_STREAM:
        states[player_id] = STATE_PLAY
        emit_play(player_id, song_infos)
    #always emit media infos, even for streaming
    emit_media_infos(player_id, song_infos)

def stop_callback(player_id):
    #if player is off, kick this callback
    logging.debug('callback STOP: %s' % player_id)
    if states.has_key(player_id) and states[player_id]!=STATE_STOP and states[player_id]!=STATE_OFF and states[player_id]!=STATE_STREAM:
        states[player_id] = STATE_STOP
        emit_stop(player_id)

def pause_callback(player_id):
    #if player is off, kick this callback
    logging.debug('callback PAUSE: %s' % player_id)
    if states.has_key(player_id) and states[player_id]!=STATE_PAUSE and states[player_id]!=STATE_OFF and states[player_id]!=STATE_STREAM:
        states[player_id] = STATE_PAUSE
        emit_pause(player_id)

def on_callback(player_id):
    logging.debug('callback ON: %s' % player_id)
    if states.has_key(player_id) and states[player_id]!=STATE_ON and states[player_id]!=STATE_STREAM:
        states[player_id] = STATE_ON
        emit_on(player_id)

def off_callback(player_id):
    logging.debug('callback OFF: %s' % player_id)
    if states.has_key(player_id) and states[player_id]!=STATE_OFF and states[player_id]!=STATE_STREAM:
        states[player_id] = STATE_OFF
        emit_off(player_id)


#emit function
def emit_play(internalid, infos):
    logging.debug('emit PLAY')
    client.emit_event(internalid, "event.device.statechanged", str(STATE_PLAY), "")

def emit_stop(internalid):
    logging.debug('emit STOP')
    client.emit_event(internalid, "event.device.statechanged", str(STATE_STOP), "")

def emit_pause(internalid):
    logging.debug('emit PAUSE')
    client.emit_event(internalid, "event.device.statechanged", str(STATE_PAUSE), "")

def emit_on(internalid):
    logging.debug('emit ON')
    client.emit_event(internalid, "event.device.statechanged", str(STATE_ON), "")

def emit_off(internalid):
    logging.debug('emit OFF')
    client.emit_event(internalid, "event.device.statechanged", str(STATE_OFF), "")

def emit_stream(internalid):
    logging.debug('emit STREAM')
    client.emit_event(internalid, "event.device.statechanged", str(STATE_STREAM), "")

def emit_media_infos(internalid, infos):
    logging.debug('emit MEDIAINFOS')
    if not internalid:
        logging.error("Unable to emit mediainfos of not specified device")
        return False

    if not infos:
        #get player infos
        infos = playlist.get_current_song(internalid)

        if not infos:
            logging.error("Unable to find infos of internalid '%s'" % internalid)
            return False

    #prepare cover
    filename = 'cover_%s.jpg' % ''.join(x for x in internalid if x.isalnum())
    cover_b64 = None
    if 'album_id' in infos: #Local file
        cover_path = str(library.get_cover_path(infos['album_id'], infos['artwork_track_id'], '/tmp/', filename, (100,100)))
        if cover_path != 'None':
            with open(cover_path, "rb") as cover_bin:
                cover_b64 = buffer(base64.b64encode(cover_bin.read()))
    if 'url' in infos and infos['url'][:7]=='spotify':
        #TODO: Add cover art lookup via https://developer.spotify.com/web-api/search-item/
        #infos: {'album': 'The Mountain', 'artist': 'Haken', 'url': 'spotify:track:5fUGUcxc6Ocn89pv60AKhr', 'bitrate': '320k VBR', 'title': 'Pareidolia', 'tracknum': '8', 'year': '0', 'duration': '651', 'type': 'Ogg Vorbis (Spotify)', 'id': '-109058656'}
        cover_b64=None

    data = {'title':infos['title'], 'album':infos['album'], 'artist':infos['artist'], 'cover':cover_b64}

    client.emit_event_raw(internalid, "event.device.mediainfos", data)


#init
try:
    #connect agoclient
    client = agoclient.AgoConnection("squeezebox")

    #read configuration
    host = agoclient.get_config_option("squeezebox", "host", "127.0.0.1")
    cli_port = int(agoclient.get_config_option("squeezebox", "cliport", "9090"))
    html_port = int(agoclient.get_config_option("squeezebox", "htmlport", "9000"))
    login = agoclient.get_config_option("squeezebox", "login", "")
    passwd = agoclient.get_config_option("squeezebox", "password", "")
    logging.info("Config: %s@%s:%d" % (login, host, cli_port))
    
    #connect to squeezebox server
    logging.info('Creating LMSServer...')
    server = pylmsserver.LMSServer(host, cli_port, login, passwd)
    server.connect()
    
    #connect to notifications server
    logging.info('Creating LMSPlaylist...')
    library = pylmslibrary.LMSLibrary(host, cli_port, html_port, login, passwd)
    playlist = pylmsplaylist.LMSPlaylist(library, host, cli_port, login, passwd)
    #play, pause, stop, on, off, add, del, move, reload
    playlist.set_callbacks(play_callback, pause_callback, stop_callback, on_callback, off_callback, None, None, None, None)
    playlist.start()
    
except Exception as e:
    #init failed
    logging.exception('Init failed, exit now')
    quit()

    

#Set message handler
#state values:
# - 0 : OFF
# - 255 : ON
# - 25 : STREAM
# - 50 : PLAYING
# - 100 : STOPPED
# - 150 : PAUSED
def messageHandler(internalid, content):
    logging.info('messageHandler: %s, %s' % (internalid,content))

    #check parameters
    if not content.has_key("command"):
        logging.error('No command specified in content')
        return None

    if internalid==host:
        #server command
        if content["command"]=="allon":
            logging.info("Command ALLON: %s" % internalid)
            for player in getPlayers():
                player.on()
        elif content["command"]=="alloff":
            logging.info("Command ALLOFF: %s" % internalid)
            for player in getPlayers():
                player.off()
        elif content["command"]=="displaymessage":
            if content.has_key('line1') and content.has_key('line2') and content.has_key('duration'):
                logging.info("Command DISPLAYMESSAGE: %s" % internalid)
                for player in getPlayers():
                    player.display(content['line1'], content['line2'], content['duration'])
            else:
                logging.error('Missing parameters to command DISPLAYMESSAGE')
                return None
    else:
        #player command
        #get player
        player = getPlayer(internalid)
        logging.info(player)
        if not player:
            logging.error('Player %s not found!' % internalid)
            return None
    
        if content["command"] == "on":
            logging.info("Command ON: %s" % internalid)
            player.on()
        elif content["command"] == "off":
            logging.info("Command OFF: %s" % internalid)
            player.off()
        elif content["command"] == "play":
            logging.info("Command PLAY: %s" % internalid)
            player.play()
        elif content["command"] == "pause":
            logging.info("Command PAUSE: %s" % internalid)
            player.pause()
        elif content["command"] == "stop":
            logging.info("Command STOP: %s" % internalid)
            player.stop()
        elif content["command"] == "next":
            logging.info("Command NEXT: %s" % internalid)
            player.next()
        elif content["command"] == "previous":
            logging.info("Command PREVIOUS: %s" % internalid)
            player.prev()
        elif content["command"] == "displaymessage":
            if content.has_key('line1') and content.has_key('line2') and content.has_key('duration'):
                logging.info("Command DISPLAYMESSAGE: %s" % internalid)
                player.display(content['line1'], content['line2'], content['duration'])
            else:
                logging.error('Missing parameters to command DISPLAYMESSAGE')
                return None
        elif content["command"] == "mediainfos":
            emit_media_infos(internalid, None)

client.add_handler(messageHandler)

#add server
client.add_device(host, 'squeezeboxserver')

#add players
try:
    logging.info('Discovering players:')
    for p in server.get_players():
        logging.info("  Add player : %s[%s]" % (p.name, p.mac))
        client.add_device(p.mac, "squeezebox")
except Exception as e:
    logging.exception('Failed to discover players. Exit now')
    quit()

#get players state and media infos
logging.info('Get current players states:')
try:
    for p in server.get_players():
        if p.get_model()=='http':
            #it's a stream. No control on it
            logging.info('  Player %s[%s] is streaming' % (p.name, p.mac))
            states[p.mac] = STATE_STREAM
            emit_stream(p.mac)
        elif not p.get_is_on():
            #player is off
            logging.info('  Player %s[%s] is off' % (p.name, p.mac))
            states[p.mac] = STATE_STOP
            emit_off(p.mac)
        else:
            #player is on
            mode = p.get_mode()
            if mode=='stop':
                logging.info('  Player %s[%s] is stopped' % (p.name, p.mac))
                states[p.mac] = STATE_STOP
                emit_stop(p.mac)
            elif mode=='play':
                logging.info('  Player %s[%s] is playing' % (p.name, p.mac))
                states[p.mac] = STATE_PLAY
                emit_play(p.mac, None)
            elif mode=='pause':
                logging.info('  Player %s[%s] is paused' % (p.name, p.mac))
                states[p.mac] = STATE_PAUSE
                emit_pause(p.mac)

        #emit media infos
        emit_media_infos(p.mac, None)

except Exception as e:
    logging.exception('Failed to get player status. Exit now')
    quit()

#run agoclient
try:
    logging.info('Running agosqueezebox...')
    client.run()
except:
    logging.info('agosqueezebox stopped')
    quit()


