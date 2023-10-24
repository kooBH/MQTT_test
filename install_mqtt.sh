#git clone https://github.com/eclipse/paho.mqtt.cpp
sudo apt-get install -y libsndfile1-dev libjsoncpp-dev libssl-dev
sudo apt-get install -y g++ cmake libpaho-mqtt-dev
git submodule init
git submodule update
cd paho.mqtt.cpp
mkdir build
cd build
cmake..
make
sudo make install
