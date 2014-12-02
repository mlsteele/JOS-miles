import os, re, threading, socket, time, shutil, struct, difflib

def ascii_to_bytes(s):
    if bytes == str:
        # Python 2
        return s
    else:
        # Python 3
        return bytes(s, "ascii")

def send_packet(payload):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect(("127.0.0.1", 26001))
    sock.send(ascii_to_bytes(payload))

# send_packet("do not go gently")
# send_packet("into that good night")

send_packet("")
send_packet("do not go gently into the good night. Rage, rage against the dying of the light")
