#include <SdFat.h>
#include <TMRpcm.h>
#include <pcmConfig.h>

const byte togByte = _BV(ICIE1); //Get the value for toggling the buffer interrupt on/off
#define buffSize 504
#define rampMega

volatile byte *TIMSK[] = {&TIMSK1,&TIMSK3,&TIMSK4,&TIMSK5};
volatile byte *TCCRnA[] = {&TCCR1A,&TCCR3A,&TCCR4A,&TCCR5A};
volatile byte *TCCRnB[] = {&TCCR1B, &TCCR3B,&TCCR4B,&TCCR5B};
volatile unsigned int *OCRnA[] = {&OCR1A,&OCR3A,&OCR4A,&OCR5A};
volatile unsigned int *ICRn[] = {&ICR1, &ICR3,&ICR4,&ICR5};
volatile unsigned int *TCNT[] = {&TCNT1,&TCNT3,&TCNT4,&TCNT5};
volatile unsigned int *OCRnB[] = {&OCR1B, &OCR3B,&OCR4B,&OCR5B};

ISR(TIMER3_OVF_vect, ISR_ALIASOF(TIMER1_OVF_vect));
ISR(TIMER3_CAPT_vect, ISR_ALIASOF(TIMER1_CAPT_vect)); // switch to manually buffering

ISR(TIMER4_OVF_vect, ISR_ALIASOF(TIMER1_OVF_vect));
ISR(TIMER4_CAPT_vect, ISR_ALIASOF(TIMER1_CAPT_vect)); // switch to manually buffering

ISR(TIMER5_OVF_vect, ISR_ALIASOF(TIMER1_OVF_vect));
ISR(TIMER5_CAPT_vect, ISR_ALIASOF(TIMER1_CAPT_vect)); // switch to manually buffering

volatile unsigned int dataEnd;
unsigned int resolution;
volatile boolean buffEmpty[2] = {false,false}, whichBuff = false, playing = 0;
volatile boolean loadCounter=0;
boolean paused = 0, rampUp = 1, _2bytes=0;
char volMod=0;
volatile byte buffer[2][buffSize];
uint16_t buffCount = 0;
byte tt;
SdFile sFile;

void TMRpcm::timerSt(){
	*ICRn[tt] = resolution;
	*TCCRnA[tt] = _BV(WGM11) | _BV(COM1A1) | _BV(COM1B0) | _BV(COM1B1); //WGM11,12,13 all set to 1 = fast PWM/w ICR TOP
	*TCCRnB[tt] = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
}

void TMRpcm::setPin() {
	disable();
	pinMode(speakerPin,OUTPUT);
	tt = 2;

	SPSR |= (1 << SPI2X);
    SPCR &= ~((1 <<SPR1) | (1 << SPR0));
}

boolean TMRpcm::wavInfo(char* filename) {
	// open the actual file on SD card
	sFile.open(filename);
	if (!ifOpen()) {
		return false;
	}

	seek(8);
	char wavStr[] = {'W','A','V','E'};
	for (uint8_t i = 0; i < 4; i++) {
		if(sFile.read() != wavStr[i]){
			break;
		}
    }
	seek(24);

    SAMPLE_RATE = sFile.read();
    SAMPLE_RATE = sFile.read() << 8 | SAMPLE_RATE;

	seek(36);
    char datStr[4] = {'d','a','t','a'};
    for (uint8_t i = 0; i < 4; i++) {
		if(sFile.read() != datStr[i]) {
		    seek(40);
		    unsigned int siz = sFile.read();
		    siz = (sFile.read() << 8 | siz) + 2;
		    seek(fPosition() + siz);
            for (byte j = 0; j < 4; j++) {
				if (sFile.read() != datStr[j]) {
					return false;
				}
	        }
		}
	 }

	unsigned long dataBytes = sFile.read();
    for (byte i = 8; i < 32; i += 8) {
		dataBytes = sFile.read() << i | dataBytes;
	}

	dataEnd = sFile.fileSize() - fPosition() - dataBytes + buffSize;

	return true;
}

void TMRpcm::quality(boolean q) {
//	if (!playing) {
//		qual = q;
//	}
}

void TMRpcm::stopPlayback() {
//	playing = 0;
//	*TIMSK[tt] &= ~(togByte | _BV(TOIE1));
//	if (ifOpen()) {
//		sFile.close();
//	}
}

void TMRpcm::pause() {
//	paused = !paused;
//	if (!paused && playing) {
//		*TIMSK[tt] |= ( _BV(ICIE1) | _BV(TOIE1) );
//	} else if (paused && playing) {
//		*TIMSK[tt] &= ~( _BV(TOIE1) );
//	}
}

boolean TMRpcm::seek(unsigned long pos ){
	return sFile.seekSet(pos);
}

unsigned long TMRpcm::fPosition(){
	return sFile.curPosition();
}

boolean TMRpcm::ifOpen(){
	return sFile.isOpen();
}

#if !defined (ENABLE_MULTI) //Below section for normal playback of 1 track at a time


void TMRpcm::play(char* filename){
	play(filename,0);
}


void TMRpcm::play(char* filename, unsigned long seekPoint) {
	if (speakerPin != lastSpeakPin) {
		setPin();
		lastSpeakPin=speakerPin;
	}
	stopPlayback();

	// verify it's a valid wav file, also open the file to SdFile
	if (!wavInfo(filename)) {
		Serial.println("[ERROR] invalid wav file");
        return;
    }

	// skip the header info
  	if (seekPoint > 0) {
		seekPoint = (SAMPLE_RATE * seekPoint) + fPosition();
  		seek(seekPoint);
	}

	playing = true;
	paused = false;

	// cap max sample rate
	if (SAMPLE_RATE > 45050 ) {
		SAMPLE_RATE = 24000;
  	}

	// quality 0 = 8 bit, quality 1 = 16 bit
#ifdef EIGHT_BIT_AUDIO
	resolution = 8 * (1000000/SAMPLE_RATE);
#endif
#ifdef SIXTEEN_BIT_AUDIO
		resolution = 16 * (1000000/SAMPLE_RATE);
#endif

    byte tmp = (sFile.read() + sFile.peek()) / 2;

    if (rampUp) {
		*OCRnA[tt] = 0;
		*OCRnB[tt] = resolution;
		timerSt();
		for (unsigned int i = 0; i < resolution; i++) {
			*OCRnB[tt] = constrain(resolution - i, 0, resolution);
			delayMicroseconds(150);
		}
	}

	rampUp = 0;
	unsigned int mod;
	if (volMod > 0) {
		mod = *OCRnA[tt] >> volMod;
	} else {
		mod = *OCRnA[tt] << (volMod * -1);
	}

	if (tmp > mod) {
		for (unsigned int i = 0; i < buffSize; i++) {
			mod = constrain(mod + 1, mod, tmp);
			buffer[0][i] = mod;
		}
		for (unsigned int i = 0; i < buffSize; i++) {
			mod = constrain(mod+1,mod, tmp);
			buffer[1][i] = mod;
		}
	} else {
		for (unsigned int i = 0; i < buffSize; i++) {
			mod = constrain(mod-1,tmp ,mod);
			buffer[0][i] = mod;
		}
		for (unsigned int i = 0; i < buffSize; i++) {
			mod = constrain(mod-1,tmp, mod);
			buffer[1][i] = mod;
		}
	}

    whichBuff = 0;
	buffEmpty[0] = true;
	buffEmpty[1] = true;
	buffCount = 0;

    noInterrupts();
	timerSt();
    *TIMSK[tt] = ( togByte | _BV(TOIE1) );
    interrupts();
}

void TMRpcm::volume(char upDown){
  if(upDown){
	  volMod++;
  }else{
	  volMod--;
  }
}

void TMRpcm::setVolume(char vol) {
    volMod = vol - 4 ;
}

// load from sd to buffers
// todo remove operation from interrupt
volatile boolean a;
void TMRpcm::fillBuffers() {
	if (buffEmpty[!whichBuff]) {
		a = !whichBuff;
		*TIMSK[tt] &= ~togByte;
		sei();
		sFile.read((byte*)buffer[a],buffSize);
		if (sFile.available() <= dataEnd) {
			playing = 0;
			*TIMSK[tt] &= ~( togByte | _BV(TOIE1) );
			if (sFile.isOpen()) {
				sFile.close();
			}
			return;
		}
		buffEmpty[a] = 0;
		*TIMSK[tt] |= togByte;
	}
}

// switch to manually buffering
ISR(TIMER1_CAPT_vect) {
	//TMRpcm::fillBuffers();
//	if (buffEmpty[!whichBuff]) {
//		a = !whichBuff;
//		*TIMSK[tt] &= ~togByte;
//		sei();
//		sFile.read((byte*)buffer[a],buffSize);
//		if (sFile.available() <= dataEnd) {
//		  	playing = false;
//		  	*TIMSK[tt] &= ~( togByte | _BV(TOIE1) );
//			  if (sFile.isOpen()) {
//				  sFile.close();
//			  }
//			return;
//	  	}
//		buffEmpty[a] = 0;
//		*TIMSK[tt] |= togByte;
//   	}
}

ISR(TIMER1_OVF_vect) {
//	if (volMod < 0 ) {
//		*OCRnA[tt] = *OCRnB[tt] = buffer[whichBuff][buffCount] >> (volMod*-1);
//    } else {
//		*OCRnA[tt] = *OCRnB[tt] = buffer[whichBuff][buffCount] << volMod;
//    }
	*OCRnA[tt] = *OCRnB[tt] = buffer[whichBuff][buffCount];
    buffCount++;

	// check if a buffer is empty, then move to the other buffer
  	if (buffCount >= buffSize) {
	  buffCount = 0;
      buffEmpty[whichBuff] = true;
      whichBuff = !whichBuff;
  	}
}

void TMRpcm::disable() {
//	playing = 0;
//	*TIMSK[tt] &= ~( togByte | _BV(TOIE1) );
//	if (ifOpen()) {
//		sFile.close();
//	}
//	if (bitRead(*TCCRnA[tt],7) > 0) {
//		int current = *OCRnA[tt];
//		for (int i = 0; i < resolution; i++) {
//			*OCRnB[tt] = constrain((current + i),0,resolution);
//			*OCRnA[tt] = constrain((current - i),0,resolution);
//			for(int j = 0; j < 10; j++){
//				while(*TCNT[tt] < resolution-50){}
//			}
//		}
//	}
//    rampUp = 1;
//    *TCCRnA[tt] = *TCCRnB[tt] = 0;
}

boolean TMRpcm::isPlaying(){
	return playing;
}

#endif

byte TMRpcm::getInfo(char* filename, char* tagData, byte infoNum) {
	byte gotInfo = 0;
	if ((gotInfo = metaInfo(1,filename, tagData, infoNum)) < 1) {
		gotInfo = metaInfo(0,filename, tagData, infoNum);
	}
	return gotInfo;
}

byte TMRpcm::listInfo(char* filename, char* tagData, byte infoNum) {
	return metaInfo(0, filename, tagData, infoNum);
}

//http://id3.org/id3v2.3.0
byte TMRpcm::id3Info(char* filename, char* tagData, byte infoNum){
	return metaInfo(1, filename, tagData, infoNum);
}

unsigned long TMRpcm::searchMainTags(SdFile xFile, char *datStr){
	xFile.seekSet(36);
	boolean found = 0;
    char dChars[4] = {'d','a','t','a'};
	char tmpChars[4];

	//xFile.seek(36);
    xFile.read((char*)tmpChars,4);
    for (byte i =0; i<4; i++){
		if(tmpChars[i] != dChars[i]){
			xFile.seekSet(40);
			unsigned int siz = xFile.read(); siz = (xFile.read() << 8 | siz)+2;
		    xFile.seekSet(xFile.curPosition() + siz);
		    xFile.read((char*)tmpChars,4);
            for (byte i =0; i<4; i++){
				if(tmpChars[i] != dChars[i]){
					return 0;
				}
	        }
		}
	 }

	unsigned long tmpp=0;
	unsigned long daBytes = xFile.read();
    for (byte i =8; i<32; i+=8){
		tmpp = xFile.read();
		daBytes = tmpp << i | daBytes;
	}

	daBytes = xFile.curPosition() + daBytes;
	if (xFile.fileSize() == daBytes) {
		return 0;
	}
	xFile.seekSet(daBytes);
	while (xFile.available() > 5) {
		if (xFile.read() == datStr[0] && xFile.peek() == datStr[1]) {
			xFile.read((char*)tmpChars,3);
            if (tmpChars[1] == datStr[2] && tmpChars[2] == datStr[3]) {
				found = 1;
				return xFile.curPosition();
			} else {
				unsigned long pos = xFile.curPosition()-1;
				xFile.seekSet(pos - 4);
			}
		}
	}
	return 0;
}

byte TMRpcm::metaInfo(boolean infoType, char* filename, char* tagData, byte whichInfo) {
	if (ifOpen()) { noInterrupts();}

		SdFile xFile;
		xFile.open(filename);
		xFile.seekSet(36);

	boolean found=0;
		char* datStr = "LIST";
		if(infoType == 1){datStr = "ID3 "; datStr[3] = 3;}
		char tmpChars[4];

	if(infoType == 0){ //if requesting LIST info, check for data at beginning of file first
		xFile.read((char*)tmpChars,4);
		for (byte i=0; i<4; i++){ //4 tagSize
			if(tmpChars[i] != datStr[i]){
				break;
		  	}else if(i==3){
				found = 1;
		  	}
		}
	}
	if(found == 0){

			unsigned long pos = searchMainTags(xFile, datStr);
			xFile.seekSet(pos);
			if(pos > 0){ found = 1; }

	}

//** This section finds the starting point and length of the tag info
	if(found == 0){ xFile.close(); if(ifOpen()){ interrupts();} return 0; }

	unsigned long listEnd;
	unsigned int listLen;
	char* tagNames[] = {"INAM","IART","IPRD"};

	if(infoType == 0){ //LIST format
		listLen = xFile.read(); listLen = xFile.read() << 8 | listLen;

			xFile.seekSet(xFile.curPosition() +6);
			listEnd = xFile.curPosition() + listLen;


	}else{				//ID3 format

			xFile.seekSet(xFile.curPosition() + 5);

			listLen = xFile.read() << 7 | listLen; listLen = xFile.read() | listLen;
			tagNames[0] = "TPE1"; tagNames[1] ="TIT2"; tagNames[2] ="TALB";

			listEnd = xFile.curPosition() + listLen;
	}

	char tgs[4];
	unsigned int len = 0;
	unsigned long tagPos = 0;

//** This section reads the tags and gets the size of the tag data and its position
//** Should work with very long tags if a big enough buffer is provided

	while(xFile.curPosition() < listEnd){


		xFile.read((char*)tgs,4);

		if(infoType == 0){ //LIST
			len = xFile.read()-1;
			len = xFile.read() << 8 | len;

				xFile.seekSet(xFile.curPosition()+2);

		}else{ 				//ID3

				xFile.seekSet(xFile.curPosition()+3);

			len = xFile.read();
			len = xFile.read() << 8 | len;
			len = (len-3)/2;

			    xFile.seekSet(xFile.curPosition() +4);
				tagPos = xFile.curPosition();


		}

		found =0;
	//** This section checks to see if the tag we found is the one requested
	//** If so, it loads the data into the buffer
		for(int p=0; p<4;p++){
			if(tgs[p] != tagNames[whichInfo][p]){
				break;
			}else{
				if(p==3){
					if(infoType == 1){
						byte junk;
						for(byte j=0; j<len; j++){
							tagData[j] = xFile.read();
							junk=xFile.read();
						}
					}else{
						xFile.read((char*)tagData,len);
					}
					tagData[len] = '\0';
					xFile.close();
					if(ifOpen()){ interrupts();}
 					return len;
				}
			}
		}

		if(found){break;}

	//**This section jumps to the next tag position if the requested tag wasn't found

		if(infoType == 0){
			if(!found){	xFile.seekSet(xFile.curPosition()+len);}
			while(xFile.peek() == 0){xFile.read();}
		}else{
			if(!found){	xFile.seekSet(tagPos+len); }
			while(xFile.peek() != 'T'){xFile.read();}
		}


 	}
 	xFile.close();
 	if(ifOpen()){ interrupts();}
 	return len;
}