/******************************************************************************
CytronEZMP3.cpp
Cytron EasyMP3 Shield Library Main Source File
Created by Ng Beng Chet @ Cytron Technologies Sdn Bhd 
Original Creation Date: Mar 10, 2016
https://github.com/CytronTechnologies/Cytron-EasyMP3-Shield/

Modified from DFPlayer_Mini_Mp3 Library Main Source File
Credit to lisper <lisper.li@dfrobot.com> @ DFRobot
Original Creation Date: May 30, 2014
https://github.com/DFRobot/DFPlayer-Mini-mp3

!!! Description Here !!!

Development environment specifics:
	IDE: Arduino 1.6.7
	Hardware Platform: Arduino Uno or any other compatible boards
	Cytron EasyMP3 Shield Version: 1.0

Distributed as-is; no warranty is given.
******************************************************************************/

#include "CytronEZMP3.h"
#ifdef __AVR__
#include <SoftwareSerial.h>
#endif

//
CytronEZMP3::CytronEZMP3():
	isManyFolder(false),
	dev_delay(0)
{
}

//
CytronEZMP3::~CytronEZMP3()
{  
}
//
#ifdef __AVR__
bool CytronEZMP3::begin(uint8_t rxpin, uint8_t txpin,long baudrate)
{
	
	//if(_rxpin==0 &&_txpin==1)
		//return begin(Serial, baudrate);

	pinMode(PIN_BUSY, INPUT);
	_rxpin = rxpin;
	_txpin = txpin;	 

	swSerial =  new SoftwareSerial(_rxpin,_txpin); 		
	swSerial->begin(baudrate);
	_serial = swSerial;
	isHardwareSerial = false;
	delay(100);
	reset();
	return init();

}
#endif

bool CytronEZMP3::begin(Uart &_hSerial, long baudrate)
{
	pinMode(PIN_BUSY, INPUT); 
	_hSerial.begin(baudrate);
	_serial = &_hSerial;
	isHardwareSerial = true;
	_txpin = 1;
	delay(100);
	reset();
	return init();

}

//
void CytronEZMP3::fill_uint16_bigend (uint8_t *thebuf, uint16_t data) {
	*thebuf =	(uint8_t)(data>>8); // high byte
	*(thebuf+1) =	(uint8_t)data; // low byte
}

//calc checksum (1~6 byte)
uint16_t CytronEZMP3::mp3_get_checksum (uint8_t *thebuf) {
	uint16_t sum = 0;
	for (int i=1; i<7; i++) {
		sum += thebuf[i];
	}
	return -sum;
}

//fill checksum to send_buf (7~8 byte)
void CytronEZMP3::mp3_fill_checksum () {
	uint16_t checksum = mp3_get_checksum (send_buf);
	fill_uint16_bigend (send_buf+7, checksum);
}

//
void CytronEZMP3::mp3_send_cmd (uint8_t cmd, uint16_t arg) {
	
	// clear serial buffer before start new command
	flush();
	listen(); //this is for multiple softwareserial

	pinMode(_txpin, OUTPUT);
	send_buf[3] = cmd;
	send_buf[4] = _isReply;
	fill_uint16_bigend ((send_buf+5), arg);
	mp3_fill_checksum ();

	send_func();
	delay(100 + dev_delay); // min 100ms delay
	pinMode(_txpin, INPUT_PULLUP);
}

//
void CytronEZMP3::mp3_send_cmd (uint8_t cmd) {
	mp3_send_cmd(cmd, 0);
}

void CytronEZMP3::send_func () {
	for (int i=0; i<10; i++) {
		write(send_buf[i]);
	}
}

bool CytronEZMP3::init ()
{	
	int timeout = 5000; // set 5 sec timeout after chip reset
	long _startMillis = millis();
	uint16_t _resp = 0;
	do{
		if(_serial->available()){
			if(_serial->peek() != 0x7e)
				_serial->read();
			else{
				_resp = millis() - _startMillis;
				break;
			}
		}	
	}while(millis() - _startMillis < timeout);

	#if DEBUG1
		Serial.print("\r\nResponse Time:");
		Serial.print(_resp);
		Serial.println();
	#endif

	if(_resp == 0) return false; // no response

	int stat = readForResponses(0x3f, 0x40, 100, true);
	
	if(stat <= 0) return false;

#if DEBUG1
	Serial.print("\r\nSystem Init successful!\r\n");
#endif
	
	_dev = recv_buf[6];
	
	switch(_dev)
	{
		case 1: 
		case 2: 
			break;
		case 3: 
		#if DEBUG1
			Serial.print("\r\nBoth TF card and U-Disk online\r\n");
		#endif
			_dev = 2;
			break;
		default: 
		#if DEBUG1
			Serial.print("\r\nPC online\r\n");
		#endif
			_dev = _dev & 0x03;
			if(_dev==3) _dev = 2;
			break;
	}

	setDevice(_dev);

	if(_dev == 1){ // if the device is U-Disk
		// Serial.println("\r\nU-Disk online\r\n");
		// delay(2000); // U-disk takes longer time to initialise file system
		
		// set delay for send command
		dev_delay = _resp;	// dev_delay = 500;
	}
	
	setVolume(15);

	#if DEBUG1
		Serial.print("\r\nInit successful\r\n");
	#endif

	// set timeout for serial for responses later
	_serial->setTimeout(1000);

	// start the fix
	pinMode(_txpin, INPUT_PULLUP);

	return true;
	
}
//
void CytronEZMP3::setFolderOption (bool opt) {
	isManyFolder = opt;
}

//
void CytronEZMP3::playPhysical (uint16_t num) {
	_isReply = 0;
	mp3_send_cmd (0x03, num);
}

//
void CytronEZMP3::playPhysical () {
	_isReply = 0;
	mp3_send_cmd (0x03);
}

//
void CytronEZMP3::next () {
	_isReply = 0;
	mp3_send_cmd (0x01);
}

//
void CytronEZMP3::prev () {
	_isReply = 0;
	mp3_send_cmd (0x02);
}

//0x06 set volume 0-30
void CytronEZMP3::setVolume (uint16_t volume) {
	_isReply = 0;
	mp3_send_cmd (0x06, volume);
}

//0x07 set EQ0/1/2/3/4/5    Normal/Pop/Rock/Jazz/Classic/Bass
void CytronEZMP3::setEQ (uint16_t eq) {
	_isReply = 0;
	mp3_send_cmd (0x07, eq);
}

//0x09 set device 1/2/3/4/5 U/SD/AUX/SLEEP/FLASH
void CytronEZMP3::setDevice (uint16_t device) {
	_isReply = 0;
	mp3_send_cmd (0x09, device);
}

//
void CytronEZMP3::sleep () {
	_isReply = 0;
	mp3_send_cmd (0x0a);
}

//
void CytronEZMP3::reset () {
	_isReply = 0;
	mp3_send_cmd (0x0c);
}

//
void CytronEZMP3::play () {
	_isReply = 0;
	mp3_send_cmd (0x0d);
}

//
void CytronEZMP3::pause () {
	_isReply = 0;
	mp3_send_cmd (0x0e);
}

void CytronEZMP3::stop () {
	_isReply = 0;
	mp3_send_cmd (0x16);
}

//
void CytronEZMP3::on(){
	_isReply = 0;
	mp3_DAC(true);
}

//
void CytronEZMP3::off(){
	_isReply = 0;
	mp3_DAC(false);
}

//play mp3 file in mp3 folder in your tf card
void CytronEZMP3::playTrack (uint16_t num) {
	_isReply = 0;
	mp3_send_cmd (0x12, num);
}

uint8_t CytronEZMP3::getCurrentDevice (){
	return _dev;
}

uint8_t CytronEZMP3::getCurrentStatus (){
	
	_isReply = 0;
	mp3_send_cmd (0x42);
	readForResponses(0x42, 0x40, 5000, true);
	_state = retVal[1];
	return _state;
}

//
uint16_t CytronEZMP3::getVolume () {
	
	_isReply = 0;
	mp3_send_cmd (0x43);
	readForResponses(0x43, 0x40, 5000, true);
	_vol = retVal[0] << 8 | retVal[1];
	return _vol;
}

uint16_t CytronEZMP3::getTotalFiles (uint8_t disk){
	
	_totalFiles = 0;
	if(!(_dev == 1||_dev == 2))
		return _totalFiles;
	
	switch(disk)
	{
		case 1: get_u_sum();break;
		case 2: get_tf_sum();break;
	}

	return _totalFiles;
}

uint16_t CytronEZMP3::getTotalFiles (){
	return getTotalFiles(_dev);
}

//
void CytronEZMP3::get_u_sum () {
	
	_isReply = 1;
	mp3_send_cmd (0x47);
	readForResponses(0x47, 0x40, 5000, true);
	_totalFiles = retVal[0] << 8 | retVal[1];
}

//
void CytronEZMP3::get_tf_sum () {
	_isReply = 1;
	mp3_send_cmd (0x48);
	readForResponses(0x48, 0x40, 5000, true);
	_totalFiles = retVal[0] << 8 | retVal[1];
}

//
//void CytronEZMP3::get_flash_sum () {
//	mp3_send_cmd (0x49);
//}
//
uint16_t CytronEZMP3::getTrackNo(uint8_t disk){
	
	if(!(_dev == 1||_dev == 2))
		return 0;
		
	switch(disk)
	{
		case 1: get_u_current();break;
		case 2: get_tf_current();break;
	}

	return _track;
}

uint16_t CytronEZMP3::getTrackNo(){
	return getTrackNo(_dev);
}

//
void CytronEZMP3::get_tf_current () {
	_isReply = 0;
	mp3_send_cmd (0x4c);
	if(readForResponses(0x4c, 0x40, 5000, true)==1)
		_track = retVal[0] << 8 | retVal[1];
}

//
void CytronEZMP3::get_u_current () {
	_isReply = 0;
	mp3_send_cmd (0x4b);
	if(readForResponses(0x4b, 0x40, 5000, true)==1)
		_track = retVal[0] << 8 | retVal[1];
}

//
//void CytronEZMP3::get_flash_current () {
//	mp3_send_cmd (0x4d);
//}

// max folder 99, max songs 255 per folder
void CytronEZMP3::playTrackFromFolder(uint8_t folder, uint8_t track){

	if(isManyFolder)
	{
		if(folder==0 || folder > 99) return;
		if(track == 0) return;	
		_isReply = 0;
		uint16_t dat = (folder << 8) + track;	
		mp3_send_cmd (0x0F, dat);
	}
	else
		playManyTracksFromSingleFolder(folder, track);
	
}

// Play at most 1000 songs per folder but folder must be named 1 to 16 only.
void CytronEZMP3::playManyTracksFromSingleFolder(uint8_t folder, uint16_t track){
	if(folder==0 || folder > 16) return;
	if(track > 0xFFF) return;
	uint16_t dat = (folder << 12) + track;
	mp3_send_cmd(0x14, dat);
}

// repeat songs from folder
void CytronEZMP3::playFolderRepeat(uint8_t folder){
	if(folder==0 || folder > 99) return;
	_isReply = 0;	
	mp3_send_cmd (0x17, folder);
}

void CytronEZMP3::playAll(boolean state){
	mp3_send_cmd (0x11, state);
}

void CytronEZMP3::single_loop (boolean state) {
	mp3_send_cmd (0x19, !state);
}

//add 
void CytronEZMP3::repeatTrack (uint16_t num) {
	playTrack (num);
	delay (10);
	single_loop (true); 
	//mp3_send_cmd (0x19, !state);
}


//
void CytronEZMP3::mp3_DAC (boolean state) {
	mp3_send_cmd (0x1a, !state);
}

//
void CytronEZMP3::randomPlay () {
	mp3_send_cmd (0x18);
}
//
void CytronEZMP3::setErrorCallback(void (*func)()){
	this->errMsg = func;
}

//
mp3_error_type CytronEZMP3::errorCode(){
	return (mp3_error_type )_errorCode;
}

int CytronEZMP3::available()
{
	int available = _serial->available();
	if(available==0) delay(5);
	
	return _serial->available();
}

size_t CytronEZMP3::write(uint8_t c)
{
	return _serial->write(c);
}

int CytronEZMP3::read()
{
	return _serial->read();
}

int CytronEZMP3::peek()
{
	return _serial->peek();
}

void CytronEZMP3::flush()
{
	 while(available()) read();
}

void CytronEZMP3::listen()
{
	#ifdef __AVR__
	if(!isHardwareSerial)
	{
		if(!swSerial->isListening())
			swSerial->listen();
	}
	#endif
}

int CytronEZMP3::readForResponses(uint16_t cmd, uint16_t fail, unsigned int timeout, bool returnVal)
{
	_received = -2; // received keeps track of number of chars read
	bool _fail = false;
	
	memset(recv_buf, '\0', EZMP3_RX_BUFFER_LEN);
	memset(retVal, '\0', 2);
	
	// set timeout
	long _startmillis = millis();		
	do{
		//wait until first byte is 0x7e
		if(peek() == 0x7e){
			// reset fail flag
			_fail = false;

			// start checking first 4 bytes
			recv_buf[0] = timedRead();
			recv_buf[1] = timedRead(); // 0xff
			uint8_t len = timedRead(); // len
			recv_buf[2] = len;
			int c = timedRead();
			recv_buf[3] = c;

			// if cmd is correct, start data extraction
			if(c == cmd){ 
				_received = readBytes(recv_buf+4, len);
				break;
			}

			// if cmd is fail cmd, set fail flag to true
			else if(c == fail){
				_received = readBytes(recv_buf+4, len);
				_fail = true;
					
				// instant break, or wait until timeout
				break;
			}
				
		}
		else
			read(); // clear the one byte from serial buffer
				
	}while(millis() - _startmillis < timeout);

	if(_received < 0){
#if DEBUG1
		Serial.print("\r\nTimeout\r\n");
#endif
		return _received;
	}

#if DEBUG0
	//possible outcome:
	Serial.print("\r\nAmount of data received: ");Serial.println(_received);
	if(_received > 0){
		for(uint8_t i=0;i<10;i++){
			if(recv_buf[i] < 16)
				Serial.print('0');			
			Serial.print(recv_buf[i], HEX);
			Serial.print(' ');
		}
	}
	Serial.println();
#endif
	if(_received == 0) return 0;
	else if (_received > 0 && _fail){
		_errorCode = recv_buf[5] << 8 | recv_buf[6];
#if DEBUG1
		Serial.print("\r\nError\r\n");
#endif
		if(this->errMsg) (*this->errMsg)();
		return 0;	
	}
	else{
		if(returnVal){
			retVal[0] = recv_buf[5];
			retVal[1] = recv_buf[6]; 
		}
		return 1;
	}
}

int CytronEZMP3::timedRead()
{
  	int _timeout = 1000;
  	int c;
  	long _startMillis = millis();
  	do{
    	c = _serial->read();
    	if (c >= 0) return c;
  	} while(millis() - _startMillis < _timeout);

  	return -1; // -1 indicates timeout
}
