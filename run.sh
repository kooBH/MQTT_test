#export MQTT_C_CLIENT_TRACE=ON
#export MQTT_C_CLIENT_TRACE_LEVEL=MEDIUM

g++ -o stream_audio stream_audio.cpp -lsndfile -lpaho-mqtt3c -lcrypto -lssl -ljsoncpp -lpaho-mqttpp3 -I /usr/include/jsoncpp
./stream_audio
