#ifndef TMRpcm_h   // if x.h hasn't been included yet...
#define TMRpcm_h   //   #define this so the compiler knows it has been included

#include <Arduino.h>
#include <pcmConfig.h>
#include <SdFat.h>

class TMRpcm {
public:
	void play(char* filename);
	void stopPlayback();
	void volume(char vol);
	void setVolume(char vol);
	void disable();
	void pause();
	void quality(boolean q);
	static void fillBuffers();
	byte speakerPin;
	boolean wavInfo(char* filename);
	boolean isPlaying();
	boolean rfPlaying;
	unsigned int SAMPLE_RATE;
	byte listInfo(char* filename, char *tagData, byte infoNum);
	byte id3Info(char* filename, char *tagData, byte infoNum);
	byte getInfo(char* filename, char* tagData, byte infoNum);
	void play(char* filename, unsigned long seekPoint);

private:
	byte lastSpeakPin;
	void setPin();
	void timerSt();
	byte metaInfo(boolean infoType, char* filename, char* tagData, byte whichInfo);
	boolean seek(unsigned long pos);
	boolean ifOpen();
	unsigned long fPosition();
	unsigned long searchMainTags(SdFile xFile, char *datStr);
};

#endif


