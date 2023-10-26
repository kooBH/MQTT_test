#include <iostream>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>
#include <sndfile.h>
#include <json/json.h>
#include <mqtt/async_client.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/buffer.h>

const std::string MQTT_BROKER_IP = "localhost:1883";
//const std::string MQTT_BROKER_IP = "localhost:5672";
const std::string MQTT_TOPIC = "/audio";
const int sample_width = 2;

char *base64(const unsigned char *input, int length) {
	const auto pl = 4*((length+2)/3);
	auto output = reinterpret_cast<char *>(calloc(pl+1, 1)); //+1 for the terminating null that EVP_EncodeBlock adds on
	const auto ol = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(output), input, length);
	if (pl != ol) { std::cerr << "Whoops, encode predicted " << pl << " but we got " << ol << "\n"; }
	return output;
}

unsigned char *decode64(const char *input, int length) {
	const auto pl = 3*length/4;
	auto output = reinterpret_cast<unsigned char *>(calloc(pl+1, 1));
	const auto ol = EVP_DecodeBlock(output, reinterpret_cast<const unsigned char *>(input), length);
	if (pl != ol) { std::cerr << "Whoops, decode predicted " << pl << " but we got " << ol << "\n"; }
	return output;
}

/**
 *  * A callback class for use with the main MQTT client.
 *   */
using namespace std;
class callback : public virtual mqtt::callback
{
	public:
		void connection_lost(const string& cause) override {
			cout << "\nConnection lost" << endl;
			if (!cause.empty())
				cout << "\tcause: " << cause << endl;
		}

		void delivery_complete(mqtt::delivery_token_ptr tok) override {
			cout << "\tDelivery complete for token: "
				<< (tok ? tok->get_message_id() : -1) << endl;
		}
};

int main() {

	// Initialize libsndfile
	SF_INFO sfInfo;
	int total_packets = 0;
	memset(&sfInfo, 0, sizeof(SF_INFO));
	SNDFILE* audioFile = sf_open("multichannel_audio.wav", SFM_READ, &sfInfo);
	if (!audioFile) {
		std::cerr << "Failed to open audio file." << std::endl;
		return 1;
	}

	// Create audio buffer for 30ms of audio
	int channels = sfInfo.channels;
	int frames = sfInfo.frames;

	printf("Channel : %d | Frame : %d\n",channels, frames);
	int buffer_length = sfInfo.samplerate * 0.03 * channels;
	sf_count_t n = 1; // stores  number of frames read


	std::cout<<"Conneecting to : "<<MQTT_BROKER_IP<<std::endl;
	mqtt::async_client client(MQTT_BROKER_IP, "mpBeamforming");

	callback cb;
	client.set_callback(cb);

	try{
		mqtt::connect_options connOpts;
		connOpts.set_keep_alive_interval(20);
		connOpts.set_clean_session(true);

		//client.connect(connOpts)->wait();

		cout << "\nConnecting..." << endl;
		mqtt::token_ptr conntok = client.connect(connOpts);
		cout << "Waiting for the connection..." << endl;
		conntok->wait();
		cout << "  ...OK" << endl;

		// Main loop to read and publish audio
		while (n>0){
			int frames_in_packet = buffer_length / channels;
			short* audioPacket = new short[frames_in_packet * channels];
			n = sf_readf_short(audioFile, audioPacket, frames_in_packet);

			//De-interleave audio to separate channels
			std::vector<short*> channelData(channels);
			char* base64Audio;
			Json::Value all_channels(Json::arrayValue);

			for(int i = 0; i< channels; i++){
				channelData[i] = new short[frames_in_packet];
				for (int j = 0; j< frames_in_packet; j++)
					channelData[i][j] = audioPacket[j*channels + i];
				auto x = reinterpret_cast<unsigned char*>(channelData[i]);

				// Encode audio packet to Base64
				base64Audio = base64(x, frames_in_packet * sample_width);
				all_channels.append(base64Audio);
			}

			// Create JSON object
			Json::Value jsonPacket;
			jsonPacket["creator"] = "mpBeamforming";
			jsonPacket["audio"] = all_channels[0];
			jsonPacket["all_channels"] = all_channels;
			jsonPacket["sample_width"] = sample_width;
			jsonPacket["buffer_duration"] = 30;
			jsonPacket["num_channels"] = 4;
			jsonPacket["sample_rate"] = 16000;

			//Create these other fields to match AudioInputIU specifications
			jsonPacket["rms"] = 0;  //TBD
			jsonPacket["all_rms"] = Json::arrayValue;
			auto time_now = std::chrono::duration_cast<std::chrono::duration<double>>( \
					std::chrono::system_clock::now().time_since_epoch()).count();   //fractional seconds since epoch
			jsonPacket["created_at"] = time_now;
			//Derive a uuid from time
			std::string time_now_str = std::to_string(time_now);
			unsigned char iuid[20];
			SHA1(reinterpret_cast<const unsigned char*>(time_now_str.c_str()), time_now_str.length(), iuid);
			jsonPacket["iuid"] = base64(iuid,20);
			jsonPacket["previous_iu"] = Json::Value::null;
			jsonPacket["grounded_in"] = Json::arrayValue;
			jsonPacket["vad"] = Json::Value::null;
			jsonPacket["doa"] = Json::Value::null;

			// Publish JSON to MQTT topic
			mqtt::message_ptr msg = mqtt::make_message(MQTT_TOPIC, jsonPacket.toStyledString());
			msg->set_qos(0);
			client.publish(msg)->wait();
			std::cout<<"Just published.."<<n<<"frames\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
			total_packets +=1;
		}
	} catch (const mqtt::exception& exc) {
		std::cerr << "Error: " << exc.what() << std::endl;
		return 1;
	}

	std::cout<<"Sample width"<<(sfInfo.format & SF_FORMAT_SUBMASK)<<std::endl;
	std::cout<<"Sent "<<total_packets<<" packets\n";


	// Cleanup and close resources
	sf_close(audioFile);
	client.disconnect()->wait();

	return 0;
}
