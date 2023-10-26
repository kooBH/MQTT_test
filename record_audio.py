import argparse
import logging
import wave
import time

import paho.mqtt.client as mqtt
#from messages.IncrementalUnits.Audio.AudioInputIU import AudioInputIU

logging.basicConfig(level = logging.NOTSET)
logger = logging.getLogger(__name__)

class Recorder:
    def __init__(self, args):
        self.channels = args.num_channels
        self.rate = 16000
        self.sample_width = 2
        self.audio_topic = args.topic
        self.is_recording = False

        self.client = mqtt.Client()
        self.client.on_message = self.on_message
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.filename = args.filename
        self.wavfile = None

        # Continually try to connect until we succeed
        # This is necessary especially in docker where the hub can take a while
        # to fully initialize
        while True:
            try:
                self.client.connect(args.mqtt_ip, args.mqtt_port, 60)
                self.client.loop_start()
                break
            except:  # noqa: E722
                logger.warning("RabbitMQ is unavailable, retrying...")
                time.sleep(5)

    def start_recording(self, filename):
        self.wavfiles = [self.open(filename.format(i), mode = "wb") for i in range(self.channels)]
        self.is_recording = True

    def open(self, fname, mode="wb"):
        return RecordingFile(
            fname,
            mode,
            1,
            self.rate,
            self.sample_width,
        )

    def on_connect(self, client, userdata, flags, rc):
        """Once connected, subscribe (will also trigger on reconnect)"""
        logger.info(f"Connected with result code {str(rc)}")
        self.client.subscribe(self.audio_topic)

    def on_disconnect(self, client, userdata, rc):
        logger.error(f"Unexpected MQTT disconnection {rc}")

    def on_message(self, client, userdata, msg):
        """Switch based on the topic to the correct handler"""
        if msg.topic == self.audio_topic:
            self.handle_audio(msg.payload)

    def handle_audio(self, msg):
        #msg = AudioInputIU.from_json(msg)
        print(msg)
        return

        if self.is_recording:
            data = msg.all_channels
            #interlace the channels to write to disk
            for i,ch in enumerate(range(self.channels)):
                self.wavfiles[i].write(data[ch])

    def stop_recording(self):
        self.is_recording = False
        for w in self.wavfiles:
            w.close()


class RecordingFile(object):
    def __init__(self, fname, mode, channels,
                 rate, sample_width):
        self.fname = fname
        self.mode = mode
        self.channels = channels
        self.rate = rate
        self.sample_width = sample_width
        self.wavefile = self._prepare_file(self.fname, self.mode)

    def __enter__(self):
        return self

    def __exit__(self, exception, value, traceback):
        self.close()

    def write(self, in_data):
        self.wavefile.writeframes(in_data)

    def close(self):
        self.wavefile.close()

    def _prepare_file(self, fname, mode='wb'):
        wavefile = wave.open(fname, mode)
        wavefile.setnchannels(self.channels)
        wavefile.setsampwidth(self.sample_width)
        wavefile.setframerate(self.rate)
        return wavefile


if __name__ == "__main__":
    
    parser = argparse.ArgumentParser("MQTT audio recorder")
    parser.add_argument("--topic", default = "/audio")
    parser.add_argument("--num_channels", type = int, default = 4)
    parser.add_argument("--mqtt_ip", default = "localhost")
    parser.add_argument("--mqtt_port", default = 1883)
    parser.add_argument("--filename", default = "audio_dump-{}.wav")

    args = parser.parse_args()
    rec = Recorder(args)

    logger.info("Starting recording...")
    rec.start_recording(args.filename)
    time.sleep(10)      #Record for 10s
    rec.stop_recording()
    logger.info("Finished recording...")
