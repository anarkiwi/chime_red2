#!/usr/bin/python3

import random
import time
import unittest

import rtmidi
from rtmidi.midiconstants import (ALL_NOTES_OFF, ALL_SOUND_OFF, CONTROL_CHANGE, RESET_ALL_CONTROLLERS, NOTE_ON, NOTE_OFF, PITCH_BEND)

MIDI_PORT = 1
MIDI_CHANNELS = set((1, 2, 3, 4, 5, 6, 7, 8, 10))
MAX_NOTES = 16
MAX_MIDI_NOTE = 96


class TestChimeRed(unittest.TestCase):

    def setUp(self):
        self.midiout = rtmidi.MidiOut()
        self.midiout.open_port(MIDI_PORT)
        self.panic()

    def tearDown(self):
        self.panic()
        del self.midiout

    def send_channel_message(self, channel, status, data1=None, data2=None):
        msg = [(status & 0xf0) | ((channel - 1) & 0xf)]
        if data1 is not None:
            msg.append(data1 & 0x7f)
            if data2 is not None:
                msg.append(data2 & 0x7)
        self.midiout.send_message(msg)

    def send_channel_cc(self, channel, cc, value):
        self.send_channel_message(channel, CONTROL_CHANGE, cc, value)

    def panic(self):
        for channel in MIDI_CHANNELS:
            for status in (ALL_NOTES_OFF, ALL_SOUND_OFF, RESET_ALL_CONTROLLERS):
                self.send_channel_cc(channel, status, 0)

    def note_on(self, channel, note, velocity):
        self.send_channel_message(channel, NOTE_ON, note, velocity)

    def note_off(self, channel, note):
        self.send_channel_message(channel, NOTE_OFF, note, 0)

    def send_pitch_bend(self, channel, bend):
        self.send_channel_message(channel, PITCH_BEND, bend & 0x7f, (bend >> 7) & 0x7f)

    def test_simple_chaos(self):
        playing_pairs = set()

        def remove_pair(midi_pair):
            midi_note, channel = midi_pair
            self.note_off(channel, midi_note)
            playing_pairs.remove(midi_pair)
            print('REMOVE %u %u (%u)' % (channel, midi_note, len(playing_pairs)))

        def add_pair(midi_pair):
            midi_note, channel = midi_pair
            velocity = random.randint(1, 127)
            self.note_on(channel, midi_note, velocity)
            playing_pairs.add(midi_pair)
            print('ADD %u %u %u (%u)' % (channel, midi_note, velocity, len(playing_pairs)))

        for i in range(1, 200):
            print(i)
            if len(playing_pairs) == MAX_NOTES:
                midi_pair = random.choice(list(playing_pairs))
                remove_pair(midi_pair)
            if random.random() > 0.5:
                while True:
                    midi_note = random.randint(0, MAX_MIDI_NOTE)
                    channel = random.choice(list(MIDI_CHANNELS))
                    midi_pair = (midi_note, channel)
                    if midi_pair not in playing_pairs:
                        add_pair(midi_pair)
                        self.send_pitch_bend(channel, random.randint(-8192, 8192))
                        break
            elif playing_pairs:
                midi_pair = random.choice(list(playing_pairs))
                remove_pair(midi_pair)
            else:
                continue
            time.sleep(random.random() * 0.1)

        for midi_pair in list(playing_pairs):
            remove_pair(midi_pair)


if __name__ == '__main__':
    unittest.main()
