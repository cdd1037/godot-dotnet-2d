/**************************************************************************/
/*  test_audio_stream_wav.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_audio_stream_wav)

#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "scene/resources/audio_stream_wav.h"
#include "tests/test_utils.h"

namespace TestAudioStreamWAV {


static Vector<uint8_t> make_regression_wav(int p_bits, bool p_float, bool p_constant = false, bool p_silence_edges = false) {
	constexpr int frames = 32;
	constexpr int channels = 2;
	const int bytes = p_bits / 8;
	Vector<uint8_t> result;
	result.resize(44 + frames * channels * bytes);
	uint8_t *w = result.ptrw();
	memcpy(w, "RIFF", 4);
	encode_uint32(result.size() - 8, w + 4);
	memcpy(w + 8, "WAVEfmt ", 8);
	encode_uint32(16, w + 16);
	encode_uint16(p_float ? 3 : 1, w + 20);
	encode_uint16(channels, w + 22);
	encode_uint32(48000, w + 24);
	encode_uint32(48000 * channels * bytes, w + 28);
	encode_uint16(channels * bytes, w + 32);
	encode_uint16(p_bits, w + 34);
	memcpy(w + 36, "data", 4);
	encode_uint32(frames * channels * bytes, w + 40);
	const double pattern[] = { -0.5, 0.0, 0.5, 0.25 };
	for (int i = 0; i < frames * channels; i++) {
		double sample = p_constant ? (i % 2 == 0 ? 0.25 : 0.5) : pattern[i % 4];
		if (p_silence_edges && (i / channels < 4 || i / channels >= frames - 4)) {
			sample = 0;
		}
		uint8_t *dst = w + 44 + i * bytes;
		if (p_float) {
			if (p_bits == 32) {
				encode_float(sample, dst);
			} else {
				encode_double(sample, dst);
			}
		} else if (p_bits == 8) {
			*dst = int(sample * 128) + 128;
		} else {
			const uint32_t encoded = uint32_t(int64_t(sample * double(uint64_t(1) << (p_bits - 1))));
			for (int byte = 0; byte < bytes; byte++) {
				dst[byte] = (encoded >> (8 * byte)) & 0xFF;
			}
		}
	}
	return result;
}

TEST_CASE("[Audio][AudioStreamWAV][EighthFixBatch] Buffered WAV parsing preserves PCM and float samples") {
	for (int encoding : { 8, 16, 24, 32, -32, -64 }) {
		const int bits = Math::abs(encoding);
		Vector<uint8_t> input = make_regression_wav(bits, encoding < 0);
		const Vector<uint8_t> original = input;
		Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(input, Dictionary());
		REQUIRE(stream.is_valid());
		CHECK(input == original);
		CHECK(stream->is_stereo());
		CHECK(stream->get_mix_rate() == 48000);
		const Vector<uint8_t> output = stream->get_data();
		CHECK(output.size() == 64 * (bits == 8 ? 1 : 2));
		const int16_t expected[] = { -16384, 0, 16384, 8192 };
		for (int i = 0; i < 64; i++) {
			if (bits == 8) {
				CHECK(int8_t(output[i]) == expected[i % 4] / 256);
			} else {
				CHECK(int16_t(decode_uint16(output.ptr() + i * 2)) == expected[i % 4]);
			}
		}
	}
}

TEST_CASE("[Audio][AudioStreamWAV][EighthFixBatch] WAV transforms preserve resample normalize mono and trim") {
	Dictionary options;
	options["force/max_rate"] = true;
	options["force/max_rate_hz"] = 24000;
	options["edit/normalize"] = true;
	options["force/mono"] = true;
	Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_buffer(make_regression_wav(32, true, true), options);
	REQUIRE(stream.is_valid());
	CHECK_FALSE(stream->is_stereo());
	CHECK(stream->get_mix_rate() == 24000);
	Vector<uint8_t> output = stream->get_data();
	REQUIRE(output.size() == 32);
	for (int i = 0; i < 16; i++) {
		CHECK(int16_t(decode_uint16(output.ptr() + i * 2)) == 24576);
	}
	options.clear();
	options["edit/trim"] = true;
	stream = AudioStreamWAV::load_from_buffer(make_regression_wav(16, false, true, true), options);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_data().size() > 0);
	CHECK(stream->get_data().size() < 32 * 2 * 2);
	options.clear();
	options["compress/mode"] = 1;
	stream = AudioStreamWAV::load_from_buffer(make_regression_wav(16, false, true), options);
	REQUIRE(stream.is_valid());
	CHECK(stream->get_format() == AudioStreamWAV::FORMAT_IMA_ADPCM);
	CHECK_FALSE(stream->get_data().is_empty());
}

// Default wav rate for test cases.
constexpr float WAV_RATE = 44100;
/* Default wav count for test cases. 1 second of audio is used so that the file can be listened
to manually if needed. */
constexpr int WAV_COUNT = WAV_RATE;

float gen_wav(float frequency, float wav_rate, int wav_number) {
	// formula for generating a sin wave with given frequency.
	return Math::sin((Math::TAU * frequency / wav_rate) * wav_number);
}

/* Generates a 440Hz sin wave in channel 0 (mono channel or left stereo channel)
 * and a 261.63Hz wave in channel 1 (right stereo channel).
 * These waves correspond to the music notes A4 and C4 respectively.
 */
Vector<uint8_t> gen_pcm8_test(float wav_rate, int wav_count, bool stereo) {
	Vector<uint8_t> buffer;
	buffer.resize(stereo ? wav_count * 2 : wav_count);

	uint8_t *write_ptr = buffer.ptrw();
	for (int i = 0; i < buffer.size(); i++) {
		float wav;
		if (stereo) {
			if (i % 2 == 0) {
				wav = gen_wav(440, wav_rate, i / 2);
			} else {
				wav = gen_wav(261.63, wav_rate, i / 2);
			}
		} else {
			wav = gen_wav(440, wav_rate, i);
		}

		// Map sin wave to full range of 8-bit values.
		uint8_t wav_8bit = Math::fast_ftoi(((wav + 1) / 2) * UINT8_MAX);
		// Unlike the .wav format, AudioStreamWAV expects signed 8-bit wavs.
		uint8_t wav_8bit_signed = wav_8bit - (INT8_MAX + 1);
		write_ptr[i] = wav_8bit_signed;
	}

	return buffer;
}

// Same as gen_pcm8_test but with 16-bit wavs.
Vector<uint8_t> gen_pcm16_test(float wav_rate, int wav_count, bool stereo) {
	Vector<uint8_t> buffer;
	buffer.resize(stereo ? wav_count * 4 : wav_count * 2);

	uint8_t *write_ptr = buffer.ptrw();
	for (int i = 0; i < buffer.size() / 2; i++) {
		float wav;
		if (stereo) {
			if (i % 2 == 0) {
				wav = gen_wav(440, wav_rate, i / 2);
			} else {
				wav = gen_wav(261.63, wav_rate, i / 2);
			}
		} else {
			wav = gen_wav(440, wav_rate, i);
		}

		// Map sin wave to full range of 16-bit values.
		uint16_t wav_16bit = Math::fast_ftoi(((wav + 1) / 2) * UINT16_MAX);
		// The .wav format expects wavs larger than 8 bits to be signed.
		uint16_t wav_16bit_signed = wav_16bit - (INT16_MAX + 1);
		encode_uint16(wav_16bit_signed, write_ptr + (i * 2));
	}

	return buffer;
}

void run_test(String file_name, AudioStreamWAV::Format data_format, bool stereo, float wav_rate, float wav_count) {
	String save_path = TestUtils::get_temp_path(file_name);

	Vector<uint8_t> test_data;
	if (data_format == AudioStreamWAV::FORMAT_8_BITS) {
		test_data = gen_pcm8_test(wav_rate, wav_count, stereo);
	} else {
		test_data = gen_pcm16_test(wav_rate, wav_count, stereo);
	}

	Ref<AudioStreamWAV> stream = memnew(AudioStreamWAV);
	stream->set_mix_rate(wav_rate);
	CHECK(stream->get_mix_rate() == wav_rate);

	stream->set_format(data_format);
	CHECK(stream->get_format() == data_format);

	stream->set_stereo(stereo);
	CHECK(stream->is_stereo() == stereo);

	stream->set_data(test_data);
	CHECK(stream->get_data() == test_data);

	SUBCASE("Stream length is computed properly") {
		CHECK(stream->get_length() == doctest::Approx(double(wav_count / wav_rate)));
	}

	SUBCASE("Stream can be saved as .wav") {
		REQUIRE(stream->save_to_wav(save_path) == OK);

		Error error;
		Ref<FileAccess> wav_file = FileAccess::open(save_path, FileAccess::READ, &error);
		REQUIRE(error == OK);

		Dictionary options;
		Ref<AudioStreamWAV> loaded_stream = AudioStreamWAV::load_from_file(save_path, options);

		CHECK(loaded_stream->get_format() == stream->get_format());
		CHECK(loaded_stream->get_loop_mode() == stream->get_loop_mode());
		CHECK(loaded_stream->get_loop_begin() == stream->get_loop_begin());
		CHECK(loaded_stream->get_loop_end() == stream->get_loop_end());
		CHECK(loaded_stream->get_mix_rate() == stream->get_mix_rate());
		CHECK(loaded_stream->is_stereo() == stream->is_stereo());
		CHECK(loaded_stream->get_length() == stream->get_length());
		CHECK(loaded_stream->is_monophonic() == stream->is_monophonic());
		CHECK(loaded_stream->get_data() == stream->get_data());
	}
}

TEST_CASE("[Audio][AudioStreamWAV] Mono PCM8 format") {
	run_test("test_pcm8_mono.wav", AudioStreamWAV::FORMAT_8_BITS, false, WAV_RATE, WAV_COUNT);
}

TEST_CASE("[Audio][AudioStreamWAV] Mono PCM16 format") {
	run_test("test_pcm16_mono.wav", AudioStreamWAV::FORMAT_16_BITS, false, WAV_RATE, WAV_COUNT);
}

TEST_CASE("[Audio][AudioStreamWAV] Stereo PCM8 format") {
	run_test("test_pcm8_stereo.wav", AudioStreamWAV::FORMAT_8_BITS, true, WAV_RATE, WAV_COUNT);
}

TEST_CASE("[Audio][AudioStreamWAV] Stereo PCM16 format") {
	run_test("test_pcm16_stereo.wav", AudioStreamWAV::FORMAT_16_BITS, true, WAV_RATE, WAV_COUNT);
}

TEST_CASE("[Audio][AudioStreamWAV] Alternate mix rate") {
	run_test("test_pcm16_stereo_38000Hz.wav", AudioStreamWAV::FORMAT_16_BITS, true, 38000, 38000);
}

TEST_CASE("[Audio][AudioStreamWAV] save_to_wav() adds '.wav' file extension automatically") {
	String save_path = TestUtils::get_temp_path("test_wav_extension");
	Vector<uint8_t> test_data = gen_pcm8_test(WAV_RATE, WAV_COUNT, false);
	Ref<AudioStreamWAV> stream = memnew(AudioStreamWAV);
	stream->set_data(test_data);

	REQUIRE(stream->save_to_wav(save_path) == OK);
	Error error;
	Ref<FileAccess> wav_file = FileAccess::open(save_path + ".wav", FileAccess::READ, &error);
	CHECK(error == OK);
}

TEST_CASE("[Audio][AudioStreamWAV] Default values") {
	Ref<AudioStreamWAV> stream = memnew(AudioStreamWAV);
	CHECK(stream->get_format() == AudioStreamWAV::FORMAT_8_BITS);
	CHECK(stream->get_loop_mode() == AudioStreamWAV::LOOP_DISABLED);
	CHECK(stream->get_loop_begin() == 0);
	CHECK(stream->get_loop_end() == 0);
	CHECK(stream->get_mix_rate() == 44100);
	CHECK(stream->is_stereo() == false);
	CHECK(stream->get_length() == 0);
	CHECK(stream->is_monophonic() == false);
	CHECK(stream->get_data() == Vector<uint8_t>{});
	CHECK(stream->get_stream_name() == "");
}

TEST_CASE("[Audio][AudioStreamWAV] Save empty file") {
	run_test("test_empty.wav", AudioStreamWAV::FORMAT_8_BITS, false, WAV_RATE, 0);
}

TEST_CASE("[Audio][AudioStreamWAV] Saving IMA ADPCM is not supported") {
	String save_path = TestUtils::get_temp_path("test_adpcm.wav");
	Ref<AudioStreamWAV> stream = memnew(AudioStreamWAV);
	stream->set_format(AudioStreamWAV::FORMAT_IMA_ADPCM);
	ERR_PRINT_OFF;
	CHECK(stream->save_to_wav(save_path) == ERR_UNAVAILABLE);
	ERR_PRINT_ON;
}

} // namespace TestAudioStreamWAV
