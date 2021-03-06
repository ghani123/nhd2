/*
	Neutrino-GUI  -   DBoxII-Project
	
	$Id: neutrino.h 2013/10/12 mohousch Exp $

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifndef __neutrino__
#define __neutrino__

#include <configfile.h>

#include <neutrinoMessages.h>
#include <driver/framebuffer.h>
#include <system/setting_helpers.h>
#include <system/configure_network.h>
#include <gui/timerlist.h>
#include <timerdclient/timerdtypes.h>
#include <gui/channellist.h>          		/* CChannelList */
#include <gui/rc_lock.h>
#include <daemonc/remotecontrol.h>    		/* st_rmsg      */

/*zapit*/
#include <client/zapitclient.h>

/*gui*/
#include <gui/scan_setup.h>

#include <string>


// CNeutrinoApp -  main run-class
typedef struct neutrino_font_descr
{
	const char * name;
	const char * filename;
	int          size_offset;
} neutrino_font_descr_struct;

class CNeutrinoApp : public CMenuTarget, CChangeObserver
{
 	private:
		CFrameBuffer * frameBuffer;

		enum
		{
			mode_unknown = -1,
			mode_tv = 1,		// tv mode
			mode_radio = 2,		// radio mode
			mode_scart = 3,		// scart mode
			mode_standby = 4,	// standby mode
			mode_audio = 5,		// audioplayer mode
			mode_pic = 6,		// pictureviewer mode
			mode_ts = 7,		// movieplayer mode
			mode_iptv = 8,		// webtv mode
			mode_mask = 0xFF,	//
			norezap = 0x100		//
		};

		CConfigFile configfile;
		
		// network
		int network_dhcp;
		int network_automatic_start;
		
		std::string network_hostname;
		std::string mac_addr;
		
		std::string network_ssid;
		std::string network_key;
		int network_encryption;

		// font
		neutrino_font_descr_struct      font;

		// modes
		int				mode;
		int				lastMode;
		
		CTimerd::RecordingInfo * nextRecordingInfo;

		struct timeval                  standby_pressed_at;

		CZapitClient::responseGetLastChannel    firstchannel;
		st_rmsg				sendmessage;

		bool				skipShutdownTimer;

		CColorSetupNotifier		*colorSetupNotifier;
		CKeySetupNotifier       	*keySetupNotifier;
		CNVODChangeExec         	*NVODChanger;
		CTuxtxtChangeExec		*TuxtxtChanger;		// for user menu
		CIPChangeNotifier		*MyIPChanger;
		CRCLock                         *rcLock;
                CTimerList                      *Timerlist;			// for user menu

		/* neutrino_menue.cpp */
                bool showUserMenu(int button);
                bool getNVODMenu(CMenuWidget * menu);

		/* neutrino.cpp */
		void firstChannel();
		
		void setupRecordingDevice(void);
		void startNextRecording();
		
		void tvMode( bool rezap = true );
		void radioMode( bool rezap = true );
		void webtvMode(bool rezap = true);
		void standbyMode( bool bOnOff );
		void scartMode( bool bOnOff );		// not used
		
		void setvol(int vol);
		
		void saveEpg();
		
		void RealRun(CMenuWidget &mainSettings);
		void InitZapper();
		
		void InitKeySettings(CMenuWidget &, CMenuWidget &bindSettings);
		void InitServiceSettings(CMenuWidget &, CMenuWidget &TunerSetup);
		void InitVideoSettings( CMenuWidget &videoSettings, CVideoSetupNotifier* videoSetupNotifier );
		void InitAudioSettings( CMenuWidget &audioSettings, CAudioSetupNotifier* audioSetupNotifier );
		void InitParentalLockSettings(CMenuWidget &);
		void InitColorSettingsMenuColors(CMenuWidget &);
		void InitColorSettings(CMenuWidget &);
		void InitLanguageSettings(CMenuWidget &);
		void InitColorSettingsStatusBarColors(CMenuWidget &colorSettings_menuColors);
		void InitColorSettingsTiming(CMenuWidget &colorSettings_timing);
		void InitLcdSettings(CMenuWidget &lcdSettings);
		void InitNetworkSettings(CMenuWidget &networkSettings);
		void InitRecordingSettings(CMenuWidget &recordingSettings);
		void InitMoviePlayerSettings(CMenuWidget &moviePlayerSettings);
		void InitScreenSettings(CMenuWidget &);
		void InitAudioplayerSettings(CMenuWidget &);
		void InitPicViewerSettings(CMenuWidget &);
		void InitMiscSettings(CMenuWidget &miscSettings, 
				      CMenuWidget &miscSettingsGeneral, 
				      CMenuWidget &miscSettingsChannelList, 
				      CMenuWidget &miscSettingsEPG, 
				      CMenuWidget &miscSettingsFileBrowser );
				      
		void InitMainMenu(CMenuWidget &mainMenu, 
				  CMenuWidget &mainSettings,
				  CMenuWidget &videoSettings, 
				  CMenuWidget &audioSettings,
				  CMenuWidget &parentallockSettings,
				  CMenuWidget &networkSettings1, 
				  CMenuWidget &networkSettings2,
		                  CMenuWidget &colorSettings, 
				  CMenuWidget &lcdSettings, 
				  CMenuWidget &keySettings,
				  CMenuWidget &miscSettings, 
				  CMenuWidget &service,
                        	  CMenuWidget &audioplayerSettings, 
				  CMenuWidget &PicViewerSettings, 
				  CMenuWidget &moviePlayerSettings, 
				  CMenuWidget &MediaPlayer);

		void SetupFrameBuffer();
		void SelectNVOD();
		
		void CmdParser(int argc, char **argv);
	
		bool doGuiRecord(char * preselectedDir, bool addTimer = false);
		
		CNeutrinoApp();

	public:
		CMenuItem * wlanEnable[3];
		
		void saveSetup(const char * fname);
		int loadSetup(const char * fname);
		void SetupTiming();
		void SetupFonts();

		void AudioMute( int newValue, bool isEvent= false );
		void setVolume(const neutrino_msg_t key, const bool bDoPaint = true, bool nowait = false);
		~CNeutrinoApp();

		/* channel list */
		CChannelList			*TVchannelList;
		CChannelList			*RADIOchannelList;
		CChannelList			* channelList;
		
		/* network config */
		CNetworkConfig                 networkConfig;

		static CNeutrinoApp * getInstance();

		void channelsInit(bool bOnly = false);
		int run(int argc, char **argv);

		//callback stuff only....
		int exec(CMenuTarget * parent, const std::string & actionKey);

		//onchange
		bool changeNotify(const neutrino_locale_t OptionName, void *);

		int handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data);

		int getMode() { return mode; }
		int getLastMode() { return lastMode; }
		
		/* recording flag */
		int recordingstatus;
		/* timeshift flag */
		int timeshiftstatus;
		/* recording_id */
		int recording_id;
		
#if defined (USE_OPENGL)
		int playbackstatus;
#endif		
		
		void SendSectionsdConfig(void);
		int GetChannelMode(void) { return g_settings.channel_mode; };
		void SetChannelMode(int mode);
		
		//dvb/txt subs
		void quickZap(int msg);
		void showInfo();
		void StopSubtitles();
		void StartSubtitles(bool show = true);
		void SelectSubtitles();
		
		// 0 - restart 
		// 1 - halt
		// 2 - reboot
		enum {
			RESTART = 0,
			SHUTDOWN,
			REBOOT
		};
		
		void ExitRun(int retcode = SHUTDOWN);
};


#endif
