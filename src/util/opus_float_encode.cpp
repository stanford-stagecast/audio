
#include <memory>
#include <iostream>
#include <vector>
#include <opus/opus.h>
#include <typed_ring_buffer.hh>
extern "C"{
	#include <libavformat/avformat.h>
}

#include "media_formats.hh"

using opus_frame_t = std::pair<size_t, std::array<uint8_t, MAX_COMPRESSED_FRAME_SIZE>>;
using namespace std; 


//size of the frame in milliseconds as a 2.5 ms as a parameter
const unsigned int NUM_CHANNELS = 2; 
const unsigned int NUM_SAMPLES = 180; 
const unsigned int BYTES_IN_PAYLOAD = 40;
const unsigned int MAX_COMPRESSED_FRAME_SIZE = 131072; 
//size of the buffer 

class OpusEncoderDecoderWrapper
{
	unique_ptr<OpusEncoder> encoder_ {};
	unique_ptr<OpusDecoder> decoder_ {}; 
	public:
		OpusEncoderDecoderWrapper(const int bit_rate, const int frame_ms, const int sample_rate){ 
			int out_enc, out_dec; 
			encoder_.reset(opus_encoder_create(sample_rate, NUM_CHANNELS, OPUS_APPLICATION_AUDIO, &out_enc)); 
			opus_encoder_ctl(encoder_.get(), OPUS_SET_BITRATE(bit_rate)); 
			decoder_.reset(opus_decoder_create(sample_rate, NUM_CHANNELS, &out_dec));
			opus_decoder_ctl(decoder_.get(), OPUS_SET_BITRATE(bit_rate)); 
		}
		void encode(const float * pcm, opus_frame_t &opus_frame){
			opus_frame.first = opus_check( opus_encode_float(encoder_.get(), 
							pcm, 
							NUM_SAMPLES, 
							opus_frame.second.data(), 
							opus_frame.second.size()));
		}
		void decode(char * input, float * pcm){
			opus_decode_float(decoder_.get(), 
					input,  
					BYTES_IN_PAYLOAD, 
					pcm, 
					opus_frame.second.size(), 
					0); 
		}
}

vector<float> decode(OpusEncodeDecodeWrapper encoder_decoder, vector<opus_frame_t> opus_frames){
	vector<float> decoded; 
	for (int x = 0; x < opus_frames.size(); x++){
		float * temp; 
		encoder_decoder.decode(opus_frames[x], temp); 
		for (int i = 0; i < NUM_SAMPLES; i++)
			decoded.push_back(temp[i]); 
	}
	return decoded; 
}

vector<opus_frame_t>  encode(OpusEncoderDecoderWrapper encoder_decoder, simple_span<float> pcm, int sample_rate, int frame_ms) {
	vector<opus_frame_t> opus_frames; 
	int window = sample_rate  * frame_ms; 
	for (int chunk_no = 0; chunk_no < pcm.size()- window; chunk_no += window) {
		opus_frame_t opus_frame; 
		encoder_decoder.encode(pcm.substr(chunk_no, window), opus_frame); 
		opus_frames.push_back(opus_frame); 
	} 
	return opus_frames; 
}

int main(int argc, char *argv[]){

	const int bit_rate = argv[1]; 
	const int frame_ms = argv[2]; 
	const int sample_rate = argv[3]; 
	simple_span<float> pcm = argv[4]; 
	try{
		OpusEncoderDecoderWrapper encoder_decoder{bit_rate, frame_ms, sample_rate};
	} catch (const exception &e){
		return EXIT_FAILURE; 
	}
	return EXIT_SUCCESS; 

}
