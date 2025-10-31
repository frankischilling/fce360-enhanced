// config.cpp : définit le point d'entrée pour l'application console.
#include "xbox/fceusupport.h"
#include "config_reader.h"
#include <iostream>

using namespace std;

Config fcecfg;

void ReadConfig()
{
	/***
	Sound
	**/
	fcecfg.Set("sound","enabled", 1);
	fcecfg.Set("sound","rate", 48000);
	fcecfg.Set("sound","bufsize", 128);
	fcecfg.Set("sound","volume", 100);
	fcecfg.Set("sound","trianglevolume", 256);
	fcecfg.Set("sound","square1volume", 256);
	fcecfg.Set("sound","square2volume", 256);
	fcecfg.Set("sound","noisevolume", 256);
	fcecfg.Set("sound","pcmvolume", 256);

	/***
	Vidéo
	**/
	fcecfg.Set("video","region","NTSC"); //not used
	fcecfg.Set("video","swfilter", 1);
	fcecfg.Set("video","screenaspect", 1);

	/***
	Network
	**/
	fcecfg.Set("network","port", 4096); //not used
	fcecfg.Set("network","enable", 0);
	fcecfg.Set("network","username", "xbox");
	fcecfg.Set("network","key", "");
	fcecfg.Set("network","server", "192.168.0.12");
	
	/***
	Controller
	**/
	DWORD gamepad_dpad_up, gamepad_dpad_down, gamepad_dpad_left, gamepad_dpad_right, 
		gamepad_start, gamepad_back, 
		gamepad_left_thumb, gamepad_right_thumb, 
		gamepad_left_shoulder, gamepad_right_shoulder, 
		gamepad_a, gamepad_b, gamepad_x, gamepad_y,
		gamepad_left_trigger, gamepad_right_trigger;
	
	gamepad_dpad_up = JOY_UP;
	gamepad_dpad_down = JOY_DOWN;
	gamepad_dpad_left = JOY_LEFT;
	gamepad_dpad_right = JOY_RIGHT;

	gamepad_a = JOY_A;
	gamepad_b = JOY_B;
	gamepad_x = JOY_B; 
	gamepad_y = 0;

	gamepad_start = JOY_START; 
	gamepad_back = JOY_SELECT;

	gamepad_left_shoulder = 0; 
	gamepad_right_shoulder = 0;

	gamepad_left_thumb = 0; 
	gamepad_right_thumb = 0;

	gamepad_left_trigger = JOY_A;
	gamepad_right_trigger = JOY_B;

	fcecfg.Set("controller","XINPUT_GAMEPAD_DPAD_UP", gamepad_dpad_up );
	fcecfg.Set("controller","XINPUT_GAMEPAD_DPAD_DOWN", gamepad_dpad_down );
	fcecfg.Set("controller","XINPUT_GAMEPAD_DPAD_LEFT", gamepad_dpad_left );
	fcecfg.Set("controller","XINPUT_GAMEPAD_DPAD_RIGHT", gamepad_dpad_right );
	fcecfg.Set("controller","XINPUT_GAMEPAD_START", gamepad_start );
	fcecfg.Set("controller","XINPUT_GAMEPAD_BACK", gamepad_back );
	fcecfg.Set("controller","XINPUT_GAMEPAD_A", gamepad_a );
	fcecfg.Set("controller","XINPUT_GAMEPAD_B", gamepad_b );
	fcecfg.Set("controller","XINPUT_GAMEPAD_X", gamepad_x );
	fcecfg.Set("controller","XINPUT_GAMEPAD_Y", gamepad_y);
	fcecfg.Set("controller","XINPUT_GAMEPAD_LEFT_THUMB", gamepad_left_thumb );
	fcecfg.Set("controller","XINPUT_GAMEPAD_RIGHT_THUMB", gamepad_right_thumb );
	fcecfg.Set("controller","XINPUT_GAMEPAD_LEFT_SHOULDER", gamepad_left_shoulder);
	fcecfg.Set("controller","XINPUT_GAMEPAD_RIGHT_SHOULDER", gamepad_right_shoulder);
	fcecfg.Set("controller","XINPUT_LEFT_TRIGGER", gamepad_left_trigger);
	fcecfg.Set("controller","XINPUT_RIGHT_TRIGGER", gamepad_right_trigger);
	
	//load the ini file
	fcecfg.Load("game:\\fceui.ini");//overwrite default
	fcecfg.Save("game:\\fceui.ini");//save in all case
	
}

