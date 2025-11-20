#include "../stdafx.h"

#ifdef USE_LUA

#include "lua_audio.h"
#include "lua_helpers.h"
#include "fceulua.h"
#include "fceu.h"
#include "types.h"
#include "sound.h"

#include <map>
#include <vector>
#include <string>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "../xbox/lua/src/lua.h"
#include "../xbox/lua/src/lauxlib.h"
#include "../xbox/lua/src/lualib.h"
}

// External audio system variables and functions
extern FCEUS FSettings;
extern uint8 EnabledChannels;
extern int32 lengthcount[4];
extern int32 DMCSize;
extern uint8 PSG[0x10];
extern uint8 DMCFormat;
extern uint8 DMCAddressLatch;
extern uint8 DMCSizeLatch;
extern uint8 RawDALatch;
extern int32 ChannelLastSample[5];
extern int32 ChannelSampleBuffer[5][512];
extern int ChannelSampleBufferIndex[5];

// Forward declarations for sound.h functions
int GetSoundBuffer(int32 **W);
void SetAudioOutputFilter(bool enabled, int filterType, double cutoff, double q);
void GetAudioOutputFilter(bool* enabled, int* filterType, double* cutoff, double* q);

// ============================================================================
// Helper Functions
// ============================================================================

// Helper function: Check if number is power of 2
static bool IsPowerOf2(int n) {
	return (n > 0) && ((n & (n - 1)) == 0);
}

// Helper function: Reverse bits for FFT
static int ReverseBits(int x, int bits) {
	int result = 0;
	for (int i = 0; i < bits; i++) {
		result = (result << 1) | (x & 1);
		x >>= 1;
	}
	return result;
}

// PI constant for FFT calculations
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Real-time frequency filtering - filter state structures
// Biquad filter (second-order IIR) for efficient real-time filtering
struct AudioFilterState {
	double x1, x2;  // Previous input samples
	double y1, y2;  // Previous output samples
	double b0, b1, b2;  // Numerator coefficients
	double a1, a2;  // Denominator coefficients
	bool initialized;
};

static AudioFilterState filterStates[10];  // Support up to 10 different filter instances

// Calculate biquad filter coefficients for different filter types
// Based on RBJ Audio EQ Cookbook formulas
static void CalculateFilterCoefficients(int filterType, double cutoff, double q, double sampleRate, 
	double& b0, double& b1, double& b2, double& a1, double& a2) {
	double w0 = 2.0 * M_PI * cutoff / sampleRate;
	double cosw0 = cos(w0);
	double sinw0 = sin(w0);
	double alpha = sinw0 / (2.0 * q);
	double a0 = 1.0 + alpha;
	
	switch (filterType) {
		case 0: // Low-pass
			b0 = (1.0 - cosw0) / (2.0 * a0);
			b1 = (1.0 - cosw0) / a0;
			b2 = (1.0 - cosw0) / (2.0 * a0);
			a1 = -2.0 * cosw0 / a0;
			a2 = (1.0 - alpha) / a0;
			break;
		case 1: // High-pass
			b0 = (1.0 + cosw0) / (2.0 * a0);
			b1 = -(1.0 + cosw0) / a0;
			b2 = (1.0 + cosw0) / (2.0 * a0);
			a1 = -2.0 * cosw0 / a0;
			a2 = (1.0 - alpha) / a0;
			break;
		case 2: // Band-pass
			b0 = sinw0 / (2.0 * a0);
			b1 = 0.0;
			b2 = -sinw0 / (2.0 * a0);
			a1 = -2.0 * cosw0 / a0;
			a2 = (1.0 - alpha) / a0;
			break;
		case 3: // Notch (band-stop)
			b0 = 1.0 / a0;
			b1 = -2.0 * cosw0 / a0;
			b2 = 1.0 / a0;
			a1 = -2.0 * cosw0 / a0;
			a2 = (1.0 - alpha) / a0;
			break;
		default:
			// Default to low-pass
			b0 = (1.0 - cosw0) / (2.0 * a0);
			b1 = (1.0 - cosw0) / a0;
			b2 = (1.0 - cosw0) / (2.0 * a0);
			a1 = -2.0 * cosw0 / a0;
			a2 = (1.0 - alpha) / a0;
			break;
	}
	
	// Normalize coefficients
	b0 /= a0;
	b1 /= a0;
	b2 /= a0;
	a1 /= a0;
	a2 /= a0;
}

// Apply biquad filter to a sample
static double ApplyBiquadFilter(AudioFilterState& state, double input) {
	if (!state.initialized) {
		// Initialize filter state
		state.x1 = 0.0;
		state.x2 = 0.0;
		state.y1 = 0.0;
		state.y2 = 0.0;
		state.initialized = true;
	}
	
	// Biquad filter: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
	double output = state.b0 * input + state.b1 * state.x1 + state.b2 * state.x2
		- state.a1 * state.y1 - state.a2 * state.y2;
	
	// Update filter state
	state.x2 = state.x1;
	state.x1 = input;
	state.y2 = state.y1;
	state.y1 = output;
	
	return output;
}

// ============================================================================
// Lua Audio Functions
// ============================================================================

// getaudioenabled() -> boolean
static int lua_getaudioenabled(lua_State* L)
{
	lua_pushboolean(L, FSettings.SndRate != 0);
	return 1;
}

// getaudiosample([index]) -> integer
static int lua_getaudiosample(lua_State* L)
{
	// If audio is disabled, return 0 to indicate silence
	if (FSettings.SndRate == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}

	int32* buffer = NULL;
	int count = GetSoundBuffer(&buffer);
	if (count <= 0 || !buffer) {
		lua_pushinteger(L, 0);
		return 1;
	}

	// Get index parameter (optional, default to -1 for last sample)
	int index = (int)luaL_optinteger(L, 1, -1);
	
	// Handle negative indices (count from end)
	if (index < 0) {
		index = count + index; // -1 becomes count-1 (last sample)
	}
	
	// Bounds check
	if (index < 0) index = 0;
	if (index >= count) index = count - 1;
	
	lua_pushinteger(L, buffer[index]);
	return 1;
}

// getaudiobuffer(count) -> table
static int lua_getaudiobuffer(lua_State* L)
{
	// If audio is disabled, return empty table
	if (FSettings.SndRate == 0) {
		lua_newtable(L);
		return 1;
	}

	int32* buffer = NULL;
	int count = GetSoundBuffer(&buffer);
	if (count <= 0 || !buffer) {
		lua_newtable(L);
		return 1;
	}

	// Get count parameter (optional, default to all available samples)
	int requestedCount = (int)luaL_optinteger(L, 1, count);
	
	// Limit to reasonable maximum (256 samples) and available count
	if (requestedCount > 256) requestedCount = 256;
	if (requestedCount > count) requestedCount = count;
	if (requestedCount < 0) requestedCount = 0;
	
	// Create table with requested samples (starting from oldest)
	lua_createtable(L, requestedCount, 0);
	for (int i = 0; i < requestedCount; i++) {
		lua_pushinteger(L, buffer[i]);
		lua_rawseti(L, -2, i + 1); // Lua tables are 1-indexed
	}
	
	return 1;
}

// getaudiosampleleft() -> integer
static int lua_getaudiosampleleft(lua_State* L)
{
	// NES audio is mono, so left channel is same as mono sample
	return lua_getaudiosample(L);
}

// getaudiosampleright() -> integer
static int lua_getaudiosampleright(lua_State* L)
{
	// NES audio is mono, so right channel is same as mono sample
	return lua_getaudiosample(L);
}

// getaudiochannel(channel) -> table
static int lua_getaudiochannel(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getaudiochannel", 1, 1, n);
	}
	
	int channel = LuaCheckRange(L, 1, 0, 4, "getaudiochannel", "channel");
	
	// Channel number validated
	if (channel < 0 || channel > 4) {
		return luaL_error(L, "getaudiochannel: channel must be 0-4 (0=Pulse1, 1=Pulse2, 2=Triangle, 3=Noise, 4=DMC)");
	}
	
	// If audio is disabled, return basic channel info
	if (FSettings.SndRate == 0) {
		lua_newtable(L);
		lua_pushstring(L, "enabled");
		lua_pushboolean(L, false);
		lua_settable(L, -3);
		lua_pushstring(L, "name");
		const char* channelNames[] = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"};
		lua_pushstring(L, channelNames[channel]);
		lua_settable(L, -3);
		lua_pushstring(L, "channel");
		lua_pushinteger(L, channel);
		lua_settable(L, -3);
		return 1;
	}
	
	// Channel register base addresses
	const uint16 channelBases[] = {0x4000, 0x4004, 0x4008, 0x400C, 0x4010};
	uint16 baseAddr = channelBases[channel];
	
	// Check if channel is enabled (bit in EnabledChannels)
	bool channelEnabled = false;
	if (channel < 4) {
		channelEnabled = (EnabledChannels & (1 << channel)) != 0;
	} else {
		// DMC channel (bit 4)
		channelEnabled = (EnabledChannels & 0x10) != 0;
	}
	
	// Create result table
	lua_newtable(L);
	
	// Basic info
	const char* channelNames[] = {"Pulse1", "Pulse2", "Triangle", "Noise", "DMC"};
	lua_pushstring(L, "name");
	lua_pushstring(L, channelNames[channel]);
	lua_settable(L, -3);
	
	lua_pushstring(L, "enabled");
	lua_pushboolean(L, channelEnabled);
	lua_settable(L, -3);
	
	lua_pushstring(L, "channel");
	lua_pushinteger(L, channel);
	lua_settable(L, -3);
	
	// Read APU registers for this channel using PSG array
	// PSG array indices: 0x00-0x03=Pulse1, 0x04-0x07=Pulse2, 0x08-0x0B=Triangle, 0x0C-0x0F=Noise
	if (channel < 4) {
		// Pulse 1, Pulse 2, Triangle, Noise channels
		int psgBase = channel * 4;
		uint8 reg0 = PSG[psgBase];
		uint8 reg1 = PSG[psgBase + 1];
		uint8 reg2 = PSG[psgBase + 2];
		uint8 reg3 = PSG[psgBase + 3];
		
		// Length counter (for channels 0-3)
		lua_pushstring(L, "lengthCounter");
		lua_pushinteger(L, lengthcount[channel]);
		lua_settable(L, -3);
		
		if (channel < 2) {
			// Pulse channels (0, 1)
			lua_pushstring(L, "dutyCycle");
			lua_pushinteger(L, (reg0 >> 6) & 0x3);
			lua_settable(L, -3);
			
			lua_pushstring(L, "volume");
			lua_pushinteger(L, reg0 & 0xF);
			lua_settable(L, -3);
			
			lua_pushstring(L, "constantVolume");
			lua_pushboolean(L, (reg0 & 0x10) != 0);
			lua_settable(L, -3);
			
			lua_pushstring(L, "sweepEnabled");
			lua_pushboolean(L, (reg1 & 0x80) != 0);
			lua_settable(L, -3);
			
			lua_pushstring(L, "sweepPeriod");
			lua_pushinteger(L, (reg1 >> 4) & 0x7);
			lua_settable(L, -3);
			
			lua_pushstring(L, "sweepNegate");
			lua_pushboolean(L, (reg1 & 0x8) != 0);
			lua_settable(L, -3);
			
			lua_pushstring(L, "sweepShift");
			lua_pushinteger(L, reg1 & 0x7);
			lua_settable(L, -3);
			
			lua_pushstring(L, "periodLow");
			lua_pushinteger(L, reg2);
			lua_settable(L, -3);
			
			lua_pushstring(L, "periodHigh");
			lua_pushinteger(L, reg3 & 0x7);
			lua_settable(L, -3);
			
			uint16 period = reg2 | ((reg3 & 0x7) << 8);
			lua_pushstring(L, "period");
			lua_pushinteger(L, period);
			lua_settable(L, -3);
			
			lua_pushstring(L, "lengthCounterHalt");
			lua_pushboolean(L, (reg3 & 0x20) != 0);
			lua_settable(L, -3);
			
		} else if (channel == 2) {
			// Triangle channel
			lua_pushstring(L, "linearCounterReload");
			lua_pushinteger(L, reg0 & 0x7F);
			lua_settable(L, -3);
			
			lua_pushstring(L, "linearCounterControl");
			lua_pushboolean(L, (reg0 & 0x80) != 0);
			lua_settable(L, -3);
			
			lua_pushstring(L, "periodLow");
			lua_pushinteger(L, reg2);
			lua_settable(L, -3);
			
			lua_pushstring(L, "periodHigh");
			lua_pushinteger(L, reg3 & 0x7);
			lua_settable(L, -3);
			
			uint16 period = reg2 | ((reg3 & 0x7) << 8);
			lua_pushstring(L, "period");
			lua_pushinteger(L, period);
			lua_settable(L, -3);
			
			lua_pushstring(L, "lengthCounterHalt");
			lua_pushboolean(L, (reg3 & 0x20) != 0);
			lua_settable(L, -3);
			
		} else if (channel == 3) {
			// Noise channel
			lua_pushstring(L, "volume");
			lua_pushinteger(L, reg0 & 0xF);
			lua_settable(L, -3);
			
			lua_pushstring(L, "constantVolume");
			lua_pushboolean(L, (reg0 & 0x10) != 0);
			lua_settable(L, -3);
			
			lua_pushstring(L, "period");
			lua_pushinteger(L, reg2 & 0xF);
			lua_settable(L, -3);
			
			lua_pushstring(L, "loopNoise");
			lua_pushboolean(L, (reg2 & 0x80) != 0);
			lua_settable(L, -3);
			
			lua_pushstring(L, "lengthCounterHalt");
			lua_pushboolean(L, (reg3 & 0x20) != 0);
			lua_settable(L, -3);
		}
		
	} else {
		// DMC channel (4) - use exposed variables
		// DMCFormat = $4010, RawDALatch = $4011, DMCAddressLatch = $4012, DMCSizeLatch = $4013
		
		lua_pushstring(L, "irqEnabled");
		lua_pushboolean(L, (DMCFormat & 0x80) != 0);
		lua_settable(L, -3);
		
		lua_pushstring(L, "loop");
		lua_pushboolean(L, (DMCFormat & 0x40) != 0);
		lua_settable(L, -3);
		
		lua_pushstring(L, "period");
		lua_pushinteger(L, DMCFormat & 0xF);
		lua_settable(L, -3);
		
		lua_pushstring(L, "directLoad");
		lua_pushinteger(L, RawDALatch & 0x7F);
		lua_settable(L, -3);
		
		lua_pushstring(L, "sampleAddress");
		lua_pushinteger(L, 0xC000 + (DMCAddressLatch << 6));
		lua_settable(L, -3);
		
		lua_pushstring(L, "sampleLength");
		lua_pushinteger(L, ((DMCSizeLatch + 1) << 4));
		lua_settable(L, -3);
		
		lua_pushstring(L, "remainingSize");
		lua_pushinteger(L, DMCSize);
		lua_settable(L, -3);
		
		lua_pushstring(L, "active");
		lua_pushboolean(L, DMCSize > 0);
		lua_settable(L, -3);
	}
	
	return 1;
}

// getaudiochannelsample(channel) -> integer
static int lua_getaudiochannelsample(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getaudiochannelsample", 1, 1, n);
	}
	
	int channel = LuaCheckInt(L, 1, "getaudiochannelsample");
	
	// Validate channel number (0-4)
	if (channel < 0 || channel > 4) {
		return luaL_error(L, "getaudiochannelsample: channel must be 0-4 (0=Pulse1, 1=Pulse2, 2=Triangle, 3=Noise, 4=DMC)");
	}
	
	// If audio is disabled, return 0
	if (FSettings.SndRate == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Return the last sample from the specified channel
	lua_pushinteger(L, ChannelLastSample[channel]);
	return 1;
}

// getaudiofft([size]) -> table
static int lua_getaudiofft(lua_State* L)
{
	// If audio is disabled, return empty table
	if (FSettings.SndRate == 0) {
		lua_newtable(L);
		lua_pushstring(L, "size");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pushstring(L, "sampleRate");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		return 1;
	}

	// Get FFT size parameter (optional, default 256)
	int fftSize = (int)luaL_optinteger(L, 1, 256);
	
	// Validate and adjust FFT size (must be power of 2, between 32 and 512)
	if (fftSize < 32) fftSize = 32;
	if (fftSize > 512) fftSize = 512;
	
	// Round down to nearest power of 2
	int bits = 0;
	int temp = fftSize;
	while (temp > 1) {
		temp >>= 1;
		bits++;
	}
	fftSize = 1 << bits;  // Round to power of 2
	
	// Get audio samples
	int32* buffer = NULL;
	int count = GetSoundBuffer(&buffer);
	if (count <= 0 || !buffer) {
		lua_newtable(L);
		lua_pushstring(L, "size");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pushstring(L, "sampleRate");
		lua_pushinteger(L, FSettings.SndRate);
		lua_settable(L, -3);
		return 1;
	}
	
	// Use available samples, but limit to fftSize
	int samplesToUse = (count < fftSize) ? count : fftSize;
	
	// Allocate arrays for FFT (real and imaginary parts)
	std::vector<double> real(fftSize, 0.0);
	std::vector<double> imag(fftSize, 0.0);
	
	// Copy and normalize samples (use most recent samples)
	int startIdx = count - samplesToUse;
	for (int i = 0; i < samplesToUse; i++) {
		// Normalize to -1.0 to 1.0 range (assuming 16-bit samples)
		real[i] = (double)buffer[startIdx + i] / 32768.0;
		imag[i] = 0.0;
	}
	
	// Apply window function (Hanning window) to reduce spectral leakage
	if (samplesToUse > 1) {
		for (int i = 0; i < samplesToUse; i++) {
			double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (samplesToUse - 1)));
			real[i] *= window;
		}
	}
	
	// Perform Radix-2 FFT
	int bitsNeeded = bits;
	
	// Bit-reverse permutation
	for (int i = 0; i < fftSize; i++) {
		int j = ReverseBits(i, bitsNeeded);
		if (i < j) {
			// Swap
			double temp = real[i];
			real[i] = real[j];
			real[j] = temp;
			temp = imag[i];
			imag[i] = imag[j];
			imag[j] = temp;
		}
	}
	
	// FFT computation
	for (int size = 2; size <= fftSize; size <<= 1) {
		double angle = -2.0 * M_PI / size;
		double w_real = cos(angle);
		double w_imag = sin(angle);
		
		for (int i = 0; i < fftSize; i += size) {
			double w_real_current = 1.0;
			double w_imag_current = 0.0;
			
			for (int j = 0; j < size / 2; j++) {
				double u_real = real[i + j];
				double u_imag = imag[i + j];
				double v_real = real[i + j + size / 2] * w_real_current - imag[i + j + size / 2] * w_imag_current;
				double v_imag = real[i + j + size / 2] * w_imag_current + imag[i + j + size / 2] * w_real_current;
				
				real[i + j] = u_real + v_real;
				imag[i + j] = u_imag + v_imag;
				real[i + j + size / 2] = u_real - v_real;
				imag[i + j + size / 2] = u_imag - v_imag;
				
				double temp = w_real_current * w_real - w_imag_current * w_imag;
				w_imag_current = w_real_current * w_imag + w_imag_current * w_real;
				w_real_current = temp;
			}
		}
	}
	
	// Create result table
	lua_newtable(L);
	
	// Add magnitude array (only first half, since FFT is symmetric for real input)
	lua_pushstring(L, "magnitude");
	lua_newtable(L);
	int magnitudeSize = fftSize / 2 + 1;
	for (int i = 0; i < magnitudeSize; i++) {
		double mag = sqrt(real[i] * real[i] + imag[i] * imag[i]);
		lua_pushinteger(L, i + 1);  // Lua is 1-indexed
		lua_pushnumber(L, mag);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);
	
	// Add phase array
	lua_pushstring(L, "phase");
	lua_newtable(L);
	for (int i = 0; i < magnitudeSize; i++) {
		double phase = atan2(imag[i], real[i]);
		lua_pushinteger(L, i + 1);  // Lua is 1-indexed
		lua_pushnumber(L, phase);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);
	
	// Add size
	lua_pushstring(L, "size");
	lua_pushinteger(L, fftSize);
	lua_settable(L, -3);
	
	// Add sample rate
	lua_pushstring(L, "sampleRate");
	lua_pushinteger(L, FSettings.SndRate);
	lua_settable(L, -3);
	
	// Add frequency resolution (Hz per bin)
	lua_pushstring(L, "frequencyResolution");
	lua_pushnumber(L, (double)FSettings.SndRate / fftSize);
	lua_settable(L, -3);
	
	return 1;
}

// getaudiochannelfft(channel, [size]) -> table
static int lua_getaudiochannelfft(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "getaudiochannelfft", 1, 2, n);
	}
	
	int channel = LuaCheckInt(L, 1, "getaudiochannelfft");
	
	// Validate channel number (0-4)
	if (channel < 0 || channel > 4) {
		return luaL_error(L, "getaudiochannelfft: channel must be 0-4 (0=Pulse1, 1=Pulse2, 2=Triangle, 3=Noise, 4=DMC)");
	}
	
	// If audio is disabled, return empty table
	if (FSettings.SndRate == 0) {
		lua_newtable(L);
		lua_pushstring(L, "size");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pushstring(L, "sampleRate");
		lua_pushinteger(L, 0);
		lua_settable(L, -3);
		lua_pushstring(L, "channel");
		lua_pushinteger(L, channel);
		lua_settable(L, -3);
		return 1;
	}
	
	// Get FFT size parameter (optional, default 256)
	int fftSize = (int)luaL_optinteger(L, 2, 256);
	
	// Validate and adjust FFT size (must be power of 2, between 32 and 512)
	if (fftSize < 32) fftSize = 32;
	if (fftSize > 512) fftSize = 512;
	
	// Round down to nearest power of 2
	int bits = 0;
	int temp = fftSize;
	while (temp > 1) {
		temp >>= 1;
		bits++;
	}
	fftSize = 1 << bits;  // Round to power of 2
	
	// Limit to available buffer size (512 samples)
	if (fftSize > 512) fftSize = 512;
	
	// Allocate arrays for FFT (real and imaginary parts)
	std::vector<double> real(fftSize, 0.0);
	std::vector<double> imag(fftSize, 0.0);
	
	// Copy samples from channel buffer (circular buffer, get most recent samples)
	int bufferIndex = ChannelSampleBufferIndex[channel];
	int samplesToUse = fftSize;
	
	// Get samples from circular buffer (most recent samples first)
	for (int i = 0; i < samplesToUse; i++) {
		// Read from buffer in reverse order (newest to oldest)
		int readIndex = (bufferIndex - 1 - i + 512) % 512;
		// Normalize to -1.0 to 1.0 range
		real[samplesToUse - 1 - i] = (double)ChannelSampleBuffer[channel][readIndex] / 32768.0;
		imag[samplesToUse - 1 - i] = 0.0;
	}
	
	// Apply window function (Hanning window) to reduce spectral leakage
	if (samplesToUse > 1) {
		for (int i = 0; i < samplesToUse; i++) {
			double window = 0.5 * (1.0 - cos(2.0 * M_PI * i / (samplesToUse - 1)));
			real[i] *= window;
		}
	}
	
	// Perform Radix-2 FFT
	int bitsNeeded = bits;
	
	// Bit-reverse permutation
	for (int i = 0; i < fftSize; i++) {
		int j = ReverseBits(i, bitsNeeded);
		if (i < j) {
			// Swap
			double temp = real[i];
			real[i] = real[j];
			real[j] = temp;
			temp = imag[i];
			imag[i] = imag[j];
			imag[j] = temp;
		}
	}
	
	// FFT computation
	for (int size = 2; size <= fftSize; size <<= 1) {
		double angle = -2.0 * M_PI / size;
		double w_real = cos(angle);
		double w_imag = sin(angle);
		
		for (int i = 0; i < fftSize; i += size) {
			double w_real_current = 1.0;
			double w_imag_current = 0.0;
			
			for (int j = 0; j < size / 2; j++) {
				double u_real = real[i + j];
				double u_imag = imag[i + j];
				double v_real = real[i + j + size / 2] * w_real_current - imag[i + j + size / 2] * w_imag_current;
				double v_imag = real[i + j + size / 2] * w_imag_current + imag[i + j + size / 2] * w_real_current;
				
				real[i + j] = u_real + v_real;
				imag[i + j] = u_imag + v_imag;
				real[i + j + size / 2] = u_real - v_real;
				imag[i + j + size / 2] = u_imag - v_imag;
				
				double temp = w_real_current * w_real - w_imag_current * w_imag;
				w_imag_current = w_real_current * w_imag + w_imag_current * w_real;
				w_real_current = temp;
			}
		}
	}
	
	// Create result table
	lua_newtable(L);
	
	// Add magnitude array (only first half, since FFT is symmetric for real input)
	lua_pushstring(L, "magnitude");
	lua_newtable(L);
	int magnitudeSize = fftSize / 2 + 1;
	for (int i = 0; i < magnitudeSize; i++) {
		double mag = sqrt(real[i] * real[i] + imag[i] * imag[i]);
		lua_pushinteger(L, i + 1);  // Lua is 1-indexed
		lua_pushnumber(L, mag);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);
	
	// Add phase array
	lua_pushstring(L, "phase");
	lua_newtable(L);
	for (int i = 0; i < magnitudeSize; i++) {
		double phase = atan2(imag[i], real[i]);
		lua_pushinteger(L, i + 1);  // Lua is 1-indexed
		lua_pushnumber(L, phase);
		lua_settable(L, -3);
	}
	lua_settable(L, -3);
	
	// Add size
	lua_pushstring(L, "size");
	lua_pushinteger(L, fftSize);
	lua_settable(L, -3);
	
	// Add sample rate (use frame rate as effective sample rate for channel samples)
	// Channel samples are stored once per frame, so effective rate is frame rate
	lua_pushstring(L, "sampleRate");
	lua_pushinteger(L, 60);  // Approximate frame rate (60 Hz)
	lua_settable(L, -3);
	
	// Add frequency resolution (Hz per bin)
	lua_pushstring(L, "frequencyResolution");
	lua_pushnumber(L, 60.0 / fftSize);  // Frame rate / FFT size
	lua_settable(L, -3);
	
	// Add channel number
	lua_pushstring(L, "channel");
	lua_pushinteger(L, channel);
	lua_settable(L, -3);
	
	return 1;
}

// getaudiofiltered([filterType], [cutoff], [q], [filterId]) -> integer
static int lua_getaudiofiltered(lua_State* L)
{
	// If audio is disabled, return 0
	if (FSettings.SndRate == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Get current audio sample
	int32* buffer = NULL;
	int count = GetSoundBuffer(&buffer);
	if (count <= 0 || !buffer) {
		lua_pushinteger(L, 0);
		return 1;
	}
	
	// Get last sample (most recent)
	int32 inputSample = buffer[count - 1];
	
	// Get filter parameters (all optional)
	const char* filterTypeStr = luaL_optstring(L, 1, "lowpass");
	double cutoff = luaL_optnumber(L, 2, 1000.0);
	double q = luaL_optnumber(L, 3, 0.707);
	int filterId = (int)luaL_optinteger(L, 4, 0);
	
	// Validate filter ID
	if (filterId < 0 || filterId >= 10) {
		filterId = 0;
	}
	
	// Parse filter type
	int filterType = 0;  // Default to low-pass
	if (strcmp(filterTypeStr, "lowpass") == 0 || strcmp(filterTypeStr, "lp") == 0) {
		filterType = 0;
	} else if (strcmp(filterTypeStr, "highpass") == 0 || strcmp(filterTypeStr, "hp") == 0) {
		filterType = 1;
	} else if (strcmp(filterTypeStr, "bandpass") == 0 || strcmp(filterTypeStr, "bp") == 0) {
		filterType = 2;
	} else if (strcmp(filterTypeStr, "notch") == 0 || strcmp(filterTypeStr, "bandstop") == 0 || strcmp(filterTypeStr, "bs") == 0) {
		filterType = 3;
	}
	
	// Validate parameters
	if (cutoff < 1.0) cutoff = 1.0;
	if (cutoff > FSettings.SndRate / 2.0) cutoff = FSettings.SndRate / 2.0;
	if (q < 0.1) q = 0.1;
	if (q > 10.0) q = 10.0;
	
	// Get filter state for this filter ID
	AudioFilterState& state = filterStates[filterId];
	
	// Check if filter needs to be recalculated (first call or parameters changed)
	// For simplicity, we'll recalculate on every call (could optimize by caching parameters)
	double b0, b1, b2, a1, a2;
	CalculateFilterCoefficients(filterType, cutoff, q, (double)FSettings.SndRate, b0, b1, b2, a1, a2);
	
	// Update filter coefficients
	state.b0 = b0;
	state.b1 = b1;
	state.b2 = b2;
	state.a1 = a1;
	state.a2 = a2;
	
	// Normalize input sample to -1.0 to 1.0 range
	double normalizedInput = (double)inputSample / 32768.0;
	
	// Apply filter
	double filteredOutput = ApplyBiquadFilter(state, normalizedInput);
	
	// Convert back to integer sample value
	int32 outputSample = (int32)(filteredOutput * 32768.0);
	
	// Clamp to prevent overflow
	if (outputSample > 32767) outputSample = 32767;
	if (outputSample < -32768) outputSample = -32768;
	
	lua_pushinteger(L, outputSample);
	return 1;
}

// setaudiofilter(enabled, [filterType], [cutoff], [q]) -> nil
static int lua_setaudiofilter(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "setaudiofilter", 1, 4, n);
	}
	
	bool enabled = lua_toboolean(L, 1) != 0;
	
	// Get filter parameters (all optional)
	const char* filterTypeStr = luaL_optstring(L, 2, "lowpass");
	double cutoff = luaL_optnumber(L, 3, 1000.0);
	double q = luaL_optnumber(L, 4, 0.707);
	
	// Parse filter type
	int filterType = 0;  // Default to low-pass
	if (strcmp(filterTypeStr, "lowpass") == 0 || strcmp(filterTypeStr, "lp") == 0) {
		filterType = 0;
	} else if (strcmp(filterTypeStr, "highpass") == 0 || strcmp(filterTypeStr, "hp") == 0) {
		filterType = 1;
	} else if (strcmp(filterTypeStr, "bandpass") == 0 || strcmp(filterTypeStr, "bp") == 0) {
		filterType = 2;
	} else if (strcmp(filterTypeStr, "notch") == 0 || strcmp(filterTypeStr, "bandstop") == 0 || strcmp(filterTypeStr, "bs") == 0) {
		filterType = 3;
	}
	
	// Set the output filter
	SetAudioOutputFilter(enabled, filterType, cutoff, q);
	
	return 0;
}

// getaudiofilter() -> table
static int lua_getaudiofilter(lua_State* L)
{
	bool enabled;
	int filterType;
	double cutoff;
	double q;
	
	GetAudioOutputFilter(&enabled, &filterType, &cutoff, &q);
	
	// Create result table
	lua_newtable(L);
	
	// Add enabled
	lua_pushstring(L, "enabled");
	lua_pushboolean(L, enabled ? 1 : 0);
	lua_settable(L, -3);
	
	// Add filter type string
	const char* filterTypeStr = "lowpass";
	switch (filterType) {
		case 0: filterTypeStr = "lowpass"; break;
		case 1: filterTypeStr = "highpass"; break;
		case 2: filterTypeStr = "bandpass"; break;
		case 3: filterTypeStr = "notch"; break;
	}
	lua_pushstring(L, "filterType");
	lua_pushstring(L, filterTypeStr);
	lua_settable(L, -3);
	
	// Add cutoff
	lua_pushstring(L, "cutoff");
	lua_pushnumber(L, cutoff);
	lua_settable(L, -3);
	
	// Add q
	lua_pushstring(L, "q");
	lua_pushnumber(L, q);
	lua_settable(L, -3);
	
	return 1;
}

// audiosampletofloat(sample) -> number
static int lua_audiosampletofloat(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "audiosampletofloat", 1, 1, n);
	}
	
	int32 sample = (int32)LuaCheckInt(L, 1, "audiosampletofloat");
	
	// Normalize to -1.0 to 1.0 range (assuming 16-bit range)
	double normalized = (double)sample / 32768.0;
	
	// Clamp to prevent values outside -1.0 to 1.0
	if (normalized > 1.0) normalized = 1.0;
	if (normalized < -1.0) normalized = -1.0;
	
	lua_pushnumber(L, normalized);
	return 1;
}

// floattosample(floatValue) -> integer
static int lua_floattosample(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "floattosample", 1, 1, n);
	}
	
	double floatValue = LuaCheckNumber(L, 1, "floattosample");
	
	// Clamp to -1.0 to 1.0 range
	if (floatValue > 1.0) floatValue = 1.0;
	if (floatValue < -1.0) floatValue = -1.0;
	
	// Convert to 16-bit signed integer
	int32 sample = (int32)(floatValue * 32768.0);
	
	// Clamp to prevent overflow
	if (sample > 32767) sample = 32767;
	if (sample < -32768) sample = -32768;
	
	lua_pushinteger(L, sample);
	return 1;
}

// audiosampletouint8(sample) -> integer
static int lua_audiosampletouint8(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "audiosampletouint8", 1, 1, n);
	}
	
	int32 sample = (int32)LuaCheckInt(L, 1, "audiosampletouint8");
	
	// Convert signed 16-bit to unsigned 8-bit
	// Shift and add 128 to convert from -128..127 to 0..255
	uint8 uint8Value = (uint8)((sample >> 8) + 128);
	
	lua_pushinteger(L, uint8Value);
	return 1;
}

// uint8tosample(uint8Value) -> integer
static int lua_uint8tosample(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "uint8tosample", 1, 1, n);
	}
	
	int uint8Value = LuaCheckRange(L, 1, 0, 255, "uint8tosample", "uint8Value");
	
	// Clamp to 0-255 range
	if (uint8Value < 0) uint8Value = 0;
	if (uint8Value > 255) uint8Value = 255;
	
	// Convert unsigned 8-bit to signed 16-bit
	// Subtract 128 to convert from 0..255 to -128..127, then scale to 16-bit
	int32 sample = ((int32)uint8Value - 128) << 8;
	
	lua_pushinteger(L, sample);
	return 1;
}

// normalizeaudiosample(sample, maxValue) -> integer
static int lua_normalizeaudiosample(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "normalizeaudiosample", 1, 2, n);
	}
	
	int32 sample = (int32)LuaCheckInt(L, 1, "normalizeaudiosample");
	double maxValue = luaL_optnumber(L, 2, 32767.0);
	
	if (maxValue <= 0.0) {
		return luaL_error(L, "normalizeaudiosample: maxValue must be positive");
	}
	
	// Normalize to -1.0 to 1.0, then scale to new range
	double normalized = (double)sample / 32768.0;
	if (normalized > 1.0) normalized = 1.0;
	if (normalized < -1.0) normalized = -1.0;
	
	int32 normalizedSample = (int32)(normalized * maxValue);
	
	// Clamp to prevent overflow
	if (normalizedSample > (int32)maxValue) normalizedSample = (int32)maxValue;
	if (normalizedSample < -(int32)maxValue) normalizedSample = -(int32)maxValue;
	
	lua_pushinteger(L, normalizedSample);
	return 1;
}

// monotostereo(monoSample) -> table
static int lua_monotostereo(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 1) {
		return LuaArgCountError(L, "monotostereo", 1, 1, n);
	}
	
	int32 monoSample = (int32)LuaCheckInt(L, 1, "monotostereo");
	
	// Create result table with left and right channels (both same value)
	lua_newtable(L);
	
	lua_pushstring(L, "left");
	lua_pushinteger(L, monoSample);
	lua_settable(L, -3);
	
	lua_pushstring(L, "right");
	lua_pushinteger(L, monoSample);
	lua_settable(L, -3);
	
	return 1;
}

// stereotomono(leftSample, rightSample) -> integer
static int lua_stereotomono(lua_State* L)
{
	int n = lua_gettop(L);
	if (n < 2) {
		return LuaArgCountError(L, "stereotomono", 2, 2, n);
	}
	
	int32 leftSample = (int32)LuaCheckInt(L, 1, "stereotomono");
	int32 rightSample = (int32)LuaCheckInt(L, 2, "stereotomono");
	
	// Average left and right channels
	int32 monoSample = (leftSample + rightSample) / 2;
	
	lua_pushinteger(L, monoSample);
	return 1;
}

// ============================================================================
// Module Registrar and Lifecycle Hooks
// ============================================================================

void Lua_RegisterAudio(lua_State* L)
{
	if (!L) {
		return;
	}

	lua_register(L, "getaudioenabled", lua_getaudioenabled);
	lua_register(L, "getaudiosample", lua_getaudiosample);
	lua_register(L, "getaudiobuffer", lua_getaudiobuffer);
	lua_register(L, "getaudiosampleleft", lua_getaudiosampleleft);
	lua_register(L, "getaudiosampleright", lua_getaudiosampleright);
	lua_register(L, "getaudiochannel", lua_getaudiochannel);
	lua_register(L, "getaudiochannelsample", lua_getaudiochannelsample);
	lua_register(L, "getaudiofft", lua_getaudiofft);
	lua_register(L, "getaudiochannelfft", lua_getaudiochannelfft);
	lua_register(L, "getaudiofiltered", lua_getaudiofiltered);
	lua_register(L, "setaudiofilter", lua_setaudiofilter);
	lua_register(L, "getaudiofilter", lua_getaudiofilter);
	lua_register(L, "audiosampletofloat", lua_audiosampletofloat);
	lua_register(L, "floattosample", lua_floattosample);
	lua_register(L, "audiosampletouint8", lua_audiosampletouint8);
	lua_register(L, "uint8tosample", lua_uint8tosample);
	lua_register(L, "normalizeaudiosample", lua_normalizeaudiosample);
	lua_register(L, "monotostereo", lua_monotostereo);
	lua_register(L, "stereotomono", lua_stereotomono);
}

void Lua_AudioReset(void)
{
	// Reset filter states
	for (int i = 0; i < 10; i++) {
		filterStates[i].initialized = false;
		filterStates[i].x1 = 0.0;
		filterStates[i].x2 = 0.0;
		filterStates[i].y1 = 0.0;
		filterStates[i].y2 = 0.0;
	}
}

#endif // USE_LUA

