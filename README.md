# About
This script reads a multichannel audio files, and streams it over 30ms packets using a serialized json structure to MQTT.

# Install other dependencies
```bash
sudo apt-get install libsndfile1-dev libjsoncpp-dev libssl-dev
```

# Installing paho mqtt for C++
```
git clone https://github.com/eclipse/paho.mqtt.cpp
sudo apt-get install g++ cmake libpaho-mqtt-dev
cd paho.mqtt
mkdir build
cd build
cmake..
make
sudo make install
```

# Compile

```bash
g++ -o stream_audio stream_audio.cpp -lsndfile -lpaho-mqtt3c -lcrypto -lssl -ljsoncpp -lpaho-mqttpp3 -I /usr/include/jsoncpp/
```

# Run rabbitmq server

Save this to `docker-compose.yml`

```yaml
version: "3.9"
services:

  rabbitmq:
    image: "rabbitmq:3.11"
    ports:
      - 1883:1883
      - 15672:15672
      - 15675:15675
```
Then launch the server as follows:
```bash
docker compose up -d
```

# Run MQTT subscription example

Depends on `paho-mqtt` library and the shared `messages` library in python. 
Run the following script after streaming has started. It will record 10 seconds of audio with one wave file for each channel.

```bash
python record_audio.py
```
