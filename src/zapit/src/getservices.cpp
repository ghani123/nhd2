/*
 * $Id: getservices.cpp,v 1.84 2003/12/09 21:12:28 thegoodguy Exp $
 *
 * (C) 2002, 2003 by Andreas Oberritter <obi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <zapit/bouquets.h>
#include <zapit/channel.h>
#include <zapit/debug.h>
#include <zapit/frontend_c.h>
#include <zapit/getservices.h>
#include <zapit/settings.h>
#include <zapit/satconfig.h>
#include <xmlinterface.h>
#include <math.h>
#include <sys/time.h>
#include <set>



extern xmlDocPtr scanInputParser;				/* defined in zapit.cpp */
extern transponder_list_t transponders;				/* defined in zapit.cpp */ // from services.xml
extern tallchans allchans;					/* defined in zapit.cpp */
extern int scanSDT;						/* defined in zapit.cpp */
static int newfound;

satellite_map_t satellitePositions;				// satellite position as specified in satellites.xml
std::map<transponder_id_t, transponder> select_transponders;	// TP map all tps from sats liste

int newtpid;
int tcnt = 0;
int scnt = 0;

extern int zapit_debug;						/* defined in zapit.cpp */
extern map<t_channel_id, audio_map_set_t> audio_map;		/* defined in zapit.cpp */

extern int FrontendCount;
extern CFrontend * getFE(int index);

extern void parseScanInputXml(int feindex);


void ParseTransponders(xmlNodePtr node, t_satellite_position satellitePosition, uint8_t Source )
{
	t_transport_stream_id transport_stream_id;
	t_original_network_id original_network_id;
	FrontendParameters feparams;
	uint8_t polarization = 0;
	freq_id_t freq;
	tcnt =0;

	memset(&feparams, 0, sizeof(FrontendParameters));

	/* read all transponders */
	while ((node = xmlGetNextOccurence(node, "TS")) != NULL) 
	{
		/* common */
		transport_stream_id = xmlGetNumericAttribute(node, "id", 16);
		original_network_id = xmlGetNumericAttribute(node, "on", 16);
		feparams.frequency = xmlGetNumericAttribute(node, "frq", 0);
		feparams.inversion = (fe_spectral_inversion) xmlGetNumericAttribute(node, "inv", 0);

		/* DVB-C */
		if(Source == DVB_C)
		{
			feparams.u.qam.symbol_rate = xmlGetNumericAttribute(node, "sr", 0);
			feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, "fec", 0);
			feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(node, "mod", 0);
		}
		/* DVB-T */
		else if(Source == DVB_T)
		{
			feparams.u.ofdm.bandwidth = (fe_bandwidth_t) xmlGetNumericAttribute(node, "band", 0);
			feparams.u.ofdm.code_rate_HP = (fe_code_rate_t) xmlGetNumericAttribute(node, "HP", 0);
			feparams.u.ofdm.code_rate_LP = (fe_code_rate_t) xmlGetNumericAttribute(node, "LP", 0);
			feparams.u.ofdm.constellation = (fe_modulation_t) xmlGetNumericAttribute(node, "const", 0);
			feparams.u.ofdm.transmission_mode = (fe_transmit_mode_t) xmlGetNumericAttribute(node, "trans", 0);
			feparams.u.ofdm.guard_interval = (fe_guard_interval_t) xmlGetNumericAttribute(node, "guard", 0);
			feparams.u.ofdm.hierarchy_information = (fe_hierarchy_t) xmlGetNumericAttribute(node, "hierarchy", 0);
		}
		/* DVB-S*/
		else if(Source == DVB_S)
		{
			feparams.u.qpsk.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(node, "fec", 0);
			feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(node, "sr", 0);
			polarization = xmlGetNumericAttribute(node, "pol", 0);

			if(feparams.u.qpsk.symbol_rate < 50000) 
				feparams.u.qpsk.symbol_rate = feparams.u.qpsk.symbol_rate * 1000;
			
			// ???
			if(feparams.frequency < 20000) 
				feparams.frequency = feparams.frequency*1000;
		}

		/* ??? */
		//if(feparams.frequency < 20000) 
		//	feparams.frequency = feparams.frequency*1000;

		if(Source == DVB_C)
			freq = feparams.frequency/100;
		else
			freq = feparams.frequency/1000;

		/* add current transponder to list */
		transponder_id_t tid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(freq, satellitePosition, original_network_id, transport_stream_id);

		pair<map<transponder_id_t, transponder>::iterator, bool> ret;

		ret = transponders.insert (std::pair <transponder_id_t, transponder> ( tid, transponder(transport_stream_id, feparams, polarization, original_network_id)));

		DBG("add transponder id %llx freq %d\n", tid, feparams.frequency);
		
		if (ret.second == false)
			printf("[zapit] duplicate transponder id %llx freq %d\n", tid, feparams.frequency);

		/* read channels that belong to the current transponder */
		ParseChannels(node->xmlChildrenNode, transport_stream_id, original_network_id, satellitePosition, freq );

		/* hop to next transponder */
		node = node->xmlNextNode;
	}

	return;
}

void ParseChannels(xmlNodePtr node, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq )
{
	t_service_id service_id;
	std::string  name;
	uint8_t      service_type;
	unsigned short vpid, apid, pcrpid, pmtpid, txpid, vtype, scrambled;
	std::string desc = "";
	desc += "Preset";
	t_channel_id chid;
	int dummy;
	int * have_ptr = &dummy;

	sat_iterator_t sit = satellitePositions.find(satellitePosition);

	if(sit != satellitePositions.end())
		have_ptr = &sit->second.have_channels;

	while ((node = xmlGetNextOccurence(node, "S")) != NULL) 
	{
		*have_ptr = 1;
		service_id = xmlGetNumericAttribute(node, "i", 16);
		name = xmlGetAttribute(node, "n");
		service_type = xmlGetNumericAttribute(node, "t", 16);
		vpid = xmlGetNumericAttribute(node, "v", 16);
		apid = xmlGetNumericAttribute(node, "a", 16);
		pcrpid = xmlGetNumericAttribute(node, "p", 16);
		pmtpid = xmlGetNumericAttribute(node, "pmt", 16);
		txpid = xmlGetNumericAttribute(node, "tx", 16);
		vtype = xmlGetNumericAttribute(node, "vt", 16);
		scrambled = xmlGetNumericAttribute(node, "s", 16);

		chid = CREATE_CHANNEL_ID64;
		char *ptr = xmlGetAttribute(node, "action");
		bool remove = ptr ? (!strcmp(ptr, "remove") || !strcmp(ptr, "replace")) : false;
		bool add    = ptr ? (!strcmp(ptr, "add")    || !strcmp(ptr, "replace")) : true;

		if (remove) 
		{
			int result = allchans.erase(chid);
			printf("%s '%s' (sid=0x%x): %s", add ? "replacing" : "removing", name.c_str(), service_id, result ? "succeded.\n" : "FAILED!\n");
		}

		if(!add) 
		{
			node = node->xmlNextNode;
			continue;
		}

		map<t_channel_id, audio_map_set_t>::iterator audio_map_it;
		audio_map_it = audio_map.find(chid);
		
		if((audio_map_it != audio_map.end()) && (audio_map_it->second.apid != 0)) 
		{
			apid = audio_map_it->second.apid;
		}

		pair<map<t_channel_id, CZapitChannel>::iterator,bool> ret;

		ret = allchans.insert (std::pair <t_channel_id, CZapitChannel> (chid,CZapitChannel ( name, 
												     service_id, 
												     transport_stream_id,
												     original_network_id, 
												     service_type, 
												     satellitePosition, 
												     freq)));

		DBG("getServicess:ParseChannels: add %s %x\n", name.c_str(), &ret.first->second);

		if(ret.second == false) 
		{
			DBG("getSevices:ParseChannels: duplicate channel %s id %llx freq %d (old %s at %d)\n", name.c_str(), chid, freq, ret.first->second.getName().c_str(), ret.first->second.getFreqId());
		} 
		else 
		{
			scnt++;
			tallchans_iterator cit1 = ret.first;
			cit1->second.scrambled = scrambled;
			service_type = cit1->second.getServiceType();

			if(pmtpid != 0 && (((service_type == 2) && (apid > 0)) || ( (service_type == 1)  && (vpid > 0) && (apid > 0))) ) 
			{
				DBG("[getservices] preset chan %s vpid %X sid %X tpid %X onid %X\n", name.c_str(), vpid, service_id, transport_stream_id, transport_stream_id);
							
				cit1->second.setVideoPid(vpid);
				cit1->second.setAudioPid(apid);
				cit1->second.setPcrPid(pcrpid);
				cit1->second.setPmtPid(pmtpid);
				cit1->second.setTeletextPid(txpid);
				cit1->second.setPidsFlag();
				cit1->second.type = vtype;
			}
		}

		node = node->xmlNextNode;
	}

	return;
}

// scan services.xml
void FindTransponder(xmlNodePtr search)
{
	t_satellite_position satellitePosition = 0;
	uint8_t Source;
	newtpid = 0xC000;
	
	while (search) 
	{
		/* type */
		if ( !(strcmp(xmlGetName(search), "cable")) )
		{
			Source = DVB_C;
		}
		else if ( !(strcmp(xmlGetName(search), "terrestrial")) )
		{
			Source = DVB_T;
		}
		else if ( !(strcmp(xmlGetName(search), "sat")) ) 
		{
			Source = DVB_S;
		}
		else
		{
			search = search->xmlNextNode;
			continue;
		}
		
		// sat position from services.xml
		satellitePosition = xmlGetSignedNumericAttribute(search, "position", 10);
			
		printf("getservices:FindTransponder: going to parse dvb-%c provider %s position %d\n", xmlGetName(search)[0], xmlGetAttribute(search, "name"), satellitePosition);
		
		// parse TP
		ParseTransponders(search->xmlChildrenNode, satellitePosition, Source );

		newfound++;
		
		search = search->xmlNextNode;
	}
}

// parse sat transponder from satellites/cables/terrestrials.xml
static uint32_t fake_tid, fake_nid;
void ParseSatTransponders(fe_type_t frontendType, xmlNodePtr search, t_satellite_position satellitePosition)
{
	uint8_t polarization = 0;
	uint8_t system = 0;
	uint8_t modulation = 1;
	int xml_fec;
	FrontendParameters feparams;
	fake_tid = fake_nid = 0;

	xmlNodePtr tps = search->xmlChildrenNode;

	while ((tps = xmlGetNextOccurence(tps, "transponder")) != NULL) 
	{
		memset(&feparams, 0, sizeof(FrontendParameters));

		freq_id_t freq;

		// frequency 
		if (frontendType == FE_OFDM)
			feparams.frequency = xmlGetNumericAttribute(tps, "centre_frequency", 0);
		else
			feparams.frequency = xmlGetNumericAttribute(tps, "frequency", 0);

		// inversion
		feparams.inversion = INVERSION_AUTO;

		if (frontendType == FE_QAM) 		//DVB-C
		{
			feparams.u.qam.symbol_rate = xmlGetNumericAttribute(tps, "symbol_rate", 0);
			feparams.u.qam.fec_inner = (fe_code_rate_t) xmlGetNumericAttribute(tps, "fec_inner", 0);
			feparams.u.qam.modulation = (fe_modulation_t) xmlGetNumericAttribute(tps, "modulation", 0);
		}
		else if (frontendType == FE_OFDM)	//DVB-T
		{
			feparams.u.ofdm.bandwidth = (fe_bandwidth_t) xmlGetNumericAttribute(tps, "bandwidth", 0);
			feparams.u.ofdm.code_rate_HP = (fe_code_rate_t) xmlGetNumericAttribute(tps, "code_rate_hp", 0);
			feparams.u.ofdm.code_rate_LP = (fe_code_rate_t) xmlGetNumericAttribute(tps, "code_rate_lp", 0);
			feparams.u.ofdm.constellation = (fe_modulation_t) xmlGetNumericAttribute(tps, "constellation", 0);
			feparams.u.ofdm.transmission_mode = (fe_transmit_mode_t) xmlGetNumericAttribute(tps, "transmission_mode", 0);
			feparams.u.ofdm.guard_interval = (fe_guard_interval_t) xmlGetNumericAttribute(tps, "guard_interval", 0);
			feparams.u.ofdm.hierarchy_information = (fe_hierarchy_t) xmlGetNumericAttribute(tps, "hierarchy_information", 0);
			feparams.inversion = (fe_spectral_inversion_t)xmlGetNumericAttribute(tps, "inversion", 0);
		}
		else if (frontendType == FE_QPSK) 	//DVB-S
		{
			feparams.u.qpsk.symbol_rate = xmlGetNumericAttribute(tps, "symbol_rate", 0);
			polarization = xmlGetNumericAttribute(tps, "polarization", 0);
			system = xmlGetNumericAttribute(tps, "system", 0);
			modulation = xmlGetNumericAttribute(tps, "modulation", 0);
			xml_fec = xmlGetNumericAttribute(tps, "fec_inner", 0);

			xml_fec = CFrontend::getCodeRate(xml_fec, system);

			// DVB-S2
			if(modulation == 2)
				xml_fec += 9;

			feparams.u.qpsk.fec_inner = (fe_code_rate_t)xml_fec;
		}
		
		if (frontendType == FE_QAM) 
			freq = feparams.frequency/100;
		else
			freq = feparams.frequency/1000;
			
		
		transponder_id_t tid = CREATE_TRANSPONDER_ID_FROM_SATELLITEPOSITION_ORIGINALNETWORK_TRANSPORTSTREAM_ID(freq, satellitePosition, fake_nid, fake_tid);

		polarization &= 1;
		
		// insert TPs list
		select_transponders.insert( std::pair <transponder_id_t, transponder> (tid, transponder(fake_tid, feparams, polarization, fake_nid)));
		
		DBG("add transponder id %llx freq %d\n", tid, feparams.frequency);
		
		fake_nid ++; 
		fake_tid ++;

		tps = tps->xmlNextNode;
	}
}

int LoadMotorPositions(void)
{
	FILE *fd = NULL;
	char buffer[256] = "";
	t_satellite_position satellitePosition;
	int spos = 0, mpos = 0, diseqc = 0, uncom = 0, com = 0, usals = 0, inuse;
	int offH = 10600, offL = 9750, sw = 11700;

	printf("getservices:loadingmotorpositions...\n");

	if ((fd = fopen(SATCONFIG, "r"))) 
	{
		fgets(buffer, 255, fd);

		while(!feof(fd)) 
		{
			sscanf(buffer, "%d %d %d %d %d %d %d %d %d %d", &spos, &mpos, &diseqc, &com, &uncom, &offL, &offH, &sw, &inuse, &usals);

			satellitePosition = spos;
			sat_iterator_t sit = satellitePositions.find(satellitePosition);

			if(sit != satellitePositions.end()) 
			{
				sit->second.motor_position = mpos;
				sit->second.diseqc = diseqc;
				sit->second.commited = com;
				sit->second.uncommited = uncom;
				sit->second.lnbOffsetLow = offL;
				sit->second.lnbOffsetHigh = offH;
				sit->second.lnbSwitch = sw;
				sit->second.use_in_scan = inuse;
				sit->second.use_usals = usals;
			}
			fgets(buffer, 255, fd);
		}
		fclose(fd);
	}
	else
		printf("[getservices] %s not found.\n", SATCONFIG);

	return 0;
}

void SaveMotorPositions()
{
	FILE * fd;
	sat_iterator_t sit;
	printf("[getservices] saving motor positions...\n");

	fd = fopen(SATCONFIG, "w");
	if(fd == NULL) 
	{
		printf("[zapit] cannot open %s\n", SATCONFIG);
		return;
	}
	
	fprintf(fd, "# sat position, stored rotor, diseqc, commited, uncommited, low, high, switch, use in full scan, use usals\n");
	
	for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) 
	{
		if(sit->second.type == DVB_S)
		      fprintf(fd, "%d %d %d %d %d %d %d %d %d %d\n", 
				sit->first, 
				sit->second.motor_position,
				sit->second.diseqc, 
				sit->second.commited, 
				sit->second.uncommited, 
				sit->second.lnbOffsetLow,
				sit->second.lnbOffsetHigh, 
				sit->second.lnbSwitch, 
				sit->second.use_in_scan, 
				sit->second.use_usals);
	
	}
	fdatasync(fileno(fd));
	fclose(fd);
}

void init_sat(t_satellite_position position)
{
	satellitePositions[position].position = 0;
	satellitePositions[position].diseqc = -1;
	satellitePositions[position].commited = -1;
	satellitePositions[position].uncommited = -1;
	satellitePositions[position].motor_position = 0;
	satellitePositions[position].diseqc_order = 0;
	satellitePositions[position].lnbOffsetLow = 9750;
	satellitePositions[position].lnbOffsetHigh = 10600;
	satellitePositions[position].lnbSwitch = 11700;
	satellitePositions[position].use_in_scan = 0;
	satellitePositions[position].use_usals = 0;
}

// load transponders
int loadTransponders()
{
	bool satcleared = 0;
	scnt = 0;
	
	t_satellite_position position = 0; //first postion

	printf("getServices:loadTransponders:\n");
	
	select_transponders.clear();
	fake_tid = fake_nid = 0;
	
	if(!satcleared)
		satellitePositions.clear();

	satcleared = 1;

	// parse sat tp
	for(int i = 0; i < FrontendCount; i++)
	{
		parseScanInputXml(i);
			
		if ( scanInputParser != NULL ) 
		{
			xmlNodePtr search = xmlDocGetRootElement(scanInputParser)->xmlChildrenNode;

			while (search) 
			{
				if (!(strcmp(xmlGetName(search), "sat"))) 
				{
					// position
					position = xmlGetSignedNumericAttribute(search, "position", 10);
					
					char * name = xmlGetAttribute(search, "name");

					if(satellitePositions.find(position) == satellitePositions.end()) 
					{
						init_sat(position);
					}

					// name
					satellitePositions[position].name = name;
					
					// type
					satellitePositions[position].type = DVB_S;
				}
				else if(!(strcmp(xmlGetName(search), "cable"))) 
				{
					//flags ???
					//satfeed ???
					
					char * name = xmlGetAttribute(search, "name");

					if(satellitePositions.find(position) == satellitePositions.end()) 
					{
						init_sat(position);
					}

					// name
					satellitePositions[position].name = name;
					
					// type //needed to resort list for scan menue
					satellitePositions[position].type = DVB_C;
				}
				else if(!(strcmp(xmlGetName(search), "terrestrial"))) 
				{
					char * name = xmlGetAttribute(search, "name");

					if(satellitePositions.find(position) == satellitePositions.end()) 
					{
						init_sat(position);
					}

					// name
					satellitePositions[position].name = name;
					
					// type //needed to resort list for scan menue
					satellitePositions[position].type = DVB_T;
				}
				
				DBG("parse provider %s position %d\n", xmlGetAttribute(search, "name"), position);

				// parse sat TP
				ParseSatTransponders( getFE(i)->getInfo()->type, search, position);
				
				position++;
				
				search = search->xmlNextNode;
			}
		}
	}
	
	return 0;
}	

// load services
int LoadServices(bool only_current)
{
	xmlDocPtr parser;
	scnt = 0;

	printf("getServices:LoadServices:\n");

	if(only_current)
		goto do_current;

	// parse services.xml
	parser = parseXmlFile(SERVICES_XML);

	if (parser != NULL) 
	{
		xmlNodePtr search = xmlDocGetRootElement(parser)->xmlChildrenNode;

		while (search) 
		{
			if (!(strcmp(xmlGetName(search), "sat"))) 
			{
				// position
				t_satellite_position position = xmlGetSignedNumericAttribute(search, "position", 10);
				char * name = xmlGetAttribute(search, "name");

				if(satellitePositions.find(position) == satellitePositions.end()) 
				{
					init_sat(position);
							
					satellitePositions[position].name = name;
				}
			}

			// jump to the next node
			search = search->xmlNextNode;
		}

		FindTransponder( xmlDocGetRootElement(parser)->xmlChildrenNode );
		
		xmlFreeDoc(parser);
	}

	// load motor position
	for(int i = 0; i < FrontendCount; i++)
	{	if( getFE(i)->getInfo()->type == FE_QPSK)
			LoadMotorPositions();
	}

	printf("[zapit] %d services loaded (%d)...\n", scnt, allchans.size());

	if(zapit_debug) 
	{
		//FIXME
		sat_iterator_t sit;
		for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++)
			printf("satelliteName = %s (%d), satellitePosition = %d motor position = %d usals %d\n", sit->second.name.c_str(), sit->second.name.size(), sit->first, sit->second.motor_position, sit->second.use_usals);
	}

do_current:
	DBG("Loading current..\n");

	if (scanSDT && (parser = parseXmlFile(CURRENTSERVICES_XML))) 
	{
		newfound = 0;
		printf("[getservices] " CURRENTSERVICES_XML "  found.\n");
		FindTransponder( xmlDocGetRootElement(parser)->xmlChildrenNode );
		
		xmlFreeDoc(parser);
		unlink(CURRENTSERVICES_XML);
		
		if(newfound)
			SaveServices(true); //FIXME for second tuner
	}

	if(!only_current) 
	{
		parser = parseXmlFile(MYSERVICES_XML);
		if (parser != NULL) 
		{
			FindTransponder(xmlDocGetRootElement(parser)->xmlChildrenNode);
			
			xmlFreeDoc(parser);
		}
	}

	return 0;
}

//#define SAVE_DEBUG
void zapit_cp(char * from, char * to)
{
        char cmd[256] = "cp -f ";
        strcat(cmd, from);
        strcat(cmd, " ");
        strcat(cmd, to);
        system(cmd);
	sync();
}

void SaveServices(bool tocopy)
{
	transponder_id_t tpid = 0;
	FILE * fd = 0;
	bool updated = 0;

	tallchans_iterator ccI;
	tallchans_iterator dI;
	transponder_list_t::iterator tI;
	char tpstr[256];
	bool tpdone = 0;
	bool satdone = 0;
	int processed = 0;
	sat_iterator_t spos_it;
	updated = 0;
	
#ifdef SAVE_DEBUG
	set<t_channel_id> chans_processed;
	DBG("\nChannel size: %d\n", sizeof(CZapitChannel));
#endif
	printf("total channels: %d\n", allchans.size());
	
	fd = fopen(SERVICES_TMP, "w");
	if(!fd) 
	{
		perror(SERVICES_TMP);
		return;
	}

	fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<zapit>\n");
	
	/* loop througth satpos */
	for (spos_it = satellitePositions.begin(); spos_it != satellitePositions.end(); spos_it++) 
	{
		satdone = 0;
		
#ifdef SAVE_DEBUG
		printf("Process sat: %s\n", spos_it->second.name.c_str());
		printf("processed channels: %d\n", chans_processed.size());
		printf("tp count: %d\n", transponders.size());
#endif

		/* loop througth TPs */
		for(tI = transponders.begin(); tI != transponders.end(); tI++) 
		{
			t_satellite_position satpos = GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xFFF;
			tpdone = 0;

			if(GET_SATELLITEPOSITION_FROM_TRANSPONDER_ID(tI->first) & 0xF000)
				satpos = -satpos;

			if(satpos != spos_it->first) 
			{
#ifdef SAVE_DEBUG
				printf("Sat position %d not found !!\n", satpos);
#endif
				continue;
			}
			
			switch(spos_it->second.type)
			{
				case DVB_S:
					sprintf(tpstr, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" pol=\"%hu\">\n",
							tI->second.transport_stream_id, tI->second.original_network_id,
							tI->second.feparams.frequency, tI->second.feparams.inversion,
							tI->second.feparams.u.qpsk.symbol_rate, tI->second.feparams.u.qpsk.fec_inner,
							tI->second.polarization);
					break;

				case DVB_C:
					sprintf(tpstr, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" sr=\"%u\" fec=\"%hu\" mod=\"%hu\">\n",
							tI->second.transport_stream_id, tI->second.original_network_id,
							tI->second.feparams.frequency, tI->second.feparams.inversion,
							tI->second.feparams.u.qam.symbol_rate, tI->second.feparams.u.qam.fec_inner,
							tI->second.feparams.u.qam.modulation);
					break;

				case DVB_T:
					sprintf(tpstr, "\t\t<TS id=\"%04x\" on=\"%04x\" frq=\"%u\" inv=\"%hu\" band=\"%hu\" HP=\"%hu\" LP=\"%hu\" const=\"%hu\" trans=\"%hu\" guard=\"%hu\" hierarchy=\"%hu\">\n",
                                        tI->second.transport_stream_id, tI->second.original_network_id,
                                        tI->second.feparams.frequency, tI->second.feparams.inversion,
                                        tI->second.feparams.u.ofdm.bandwidth, tI->second.feparams.u.ofdm.code_rate_HP,
                                        tI->second.feparams.u.ofdm.code_rate_LP, tI->second.feparams.u.ofdm.constellation,tI->second.feparams.u.ofdm.transmission_mode, tI->second.feparams.u.ofdm.guard_interval, tI->second.feparams.u.ofdm.hierarchy_information);
					break;

				default:
					break;
			}

			tpid = tI->first;
			
			// loop througth channels
			for (ccI = allchans.begin(); ccI != allchans.end(); ccI++) 
			{
				if(ccI->second.getTransponderId() == tpid) 
				{
					if(!satdone) 
					{
						switch(spos_it->second.type)
						{
							case DVB_S:
							{
								fprintf(fd, "\t<sat name=\"%s\" position=\"%hd\" diseqc=\"%hd\" uncommited=\"%hd\">\n",spos_it->second.name.c_str(), spos_it->first, spos_it->second.diseqc, spos_it->second.uncommited);
							}
							break;

							case DVB_C:
							{
								fprintf(fd, "\t<cable name=\"%s\" position=\"%hd\">\n", spos_it->second.name.c_str(), spos_it->first);
							}
							break;

							case DVB_T:
							{
								fprintf(fd, "\t<terrestrial name=\"%s\" position=\"%hd\">\n", spos_it->second.name.c_str(), spos_it->first);
			
							}
							break;

							default:
							break;
						}

						satdone = 1;
					}

					if(!tpdone) 
					{
						fprintf(fd, "%s", tpstr);
						tpdone = 1;
					}

					if(ccI->second.getPidsFlag()) 
					{
						fprintf(fd, "\t\t\t<S i=\"%04x\" n=\"%s\" v=\"%x\" a=\"%x\" p=\"%x\" pmt=\"%x\" tx=\"%x\" t=\"%x\" vt=\"%d\" s=\"%d\"/>\n",
								ccI->second.getServiceId(), 
								convert_UTF8_To_UTF8_XML(ccI->second.getName().c_str()).c_str(),
								ccI->second.getVideoPid(), 
								ccI->second.getPreAudioPid(),
								ccI->second.getPcrPid(), 
								ccI->second.getPmtPid(), 
								ccI->second.getTeletextPid(),
								ccI->second.getServiceType(true), 
								ccI->second.type, 
								ccI->second.scrambled);
					} 
					else 
					{
						fprintf(fd, "\t\t\t<S i=\"%04x\" n=\"%s\" t=\"%x\" s=\"%d\"/>\n",
								ccI->second.getServiceId(), 
								convert_UTF8_To_UTF8_XML(ccI->second.getName().c_str()).c_str(),
								ccI->second.getServiceType(true), 
								ccI->second.scrambled);
					}
					processed++;
#ifdef SAVE_DEBUG
					chans_processed.insert(ccI->second.getChannelID());
#endif
				}
			}
			if(tpdone) fprintf(fd, "\t\t</TS>\n");
		}

		if(satdone) 
		{
			switch(spos_it->second.type)
			{
				case DVB_S:
					fprintf(fd, "\t</sat>\n");
					break;
					
				case DVB_C:
					fprintf(fd, "\t</cable>\n");
					break;
					
				case DVB_T:
					fprintf(fd, "\t</terrestrial>\n");
					break;
				default:
					break;
			}
		}
	}
	fprintf(fd, "</zapit>\n");
	fclose(fd);

	if(tocopy) 
	{
		zapit_cp((char *) SERVICES_TMP, (char *) SERVICES_XML);
		unlink(SERVICES_TMP);
	}
#ifdef SAVE_DEBUG
	printf("processed channels: %d\n", chans_processed.size());
	int i = 0;
	for (tallchans::iterator it = allchans.begin(); it != allchans.end(); it++)
		if (chans_processed.find(it->first) == chans_processed.end())
			printf("unused channel %d sat %d freq %d sid %04X: %s\n", ++i, it->second.getSatellitePosition(), it->second.getFreqId(), it->second.getServiceId(), it->second.getName().c_str());
	chans_processed.clear();
#endif
	printf("processed channels: %d\n", processed);
}
