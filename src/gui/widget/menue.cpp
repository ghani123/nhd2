/*
	Neutrino-GUI  -   DBoxII-Project
	
	$Id: menue.cpp 2013/10/12 mohousch Exp $

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/menue.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/color.h>

#include <gui/widget/stringinput.h>

#include <global.h>
#include <neutrino.h>
#include <gui/widget/icons.h>

#include <cctype>


#define ITEM_ICON_W	128	// min=100, max = 128
#define ITEM_ICON_H	128	// min=100, max = 128

#define ICON_OFFSET	5	// offset from left border
#define LOCAL_OFFSET	8	// offset from painted icon at left border

static int HEIGHT;
static int FULL_HEIGHT;


/* the following generic menu items are integrated into multiple menus at the same time */
CMenuSeparator CGenericMenuSeparator;
CMenuSeparator CGenericMenuSeparatorLine(CMenuSeparator::LINE);

CMenuForwarder CGenericMenuBack(LOCALE_MENU_BACK, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_LEFT);
//CMenuForwarder CGenericMenuCancel(LOCALE_MENU_CANCEL, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_HOME);
//CMenuForwarder CGenericMenuNext(LOCALE_MENU_NEXT, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_HOME);

//
CMenuForwarderItemMenuIcon CGenericMenuBackItemMenuIcon(LOCALE_MENU_BACK, true, NULL, NULL, NULL, CRCInput::RC_nokey, NEUTRINO_ICON_BUTTON_LEFT);

//
CMenuSeparatorItemMenuIcon CGenericMenuSeparatorItemMenuIcon;
CMenuSeparatorItemMenuIcon CGenericMenuSeparatorLineItemMenuIcon(CMenuSeparatorItemMenuIcon::LINE);;

// globals
CMenuSeparator * const GenericMenuSeparator = &CGenericMenuSeparator;
CMenuSeparator * const GenericMenuSeparatorLine = &CGenericMenuSeparatorLine;

CMenuForwarder * const GenericMenuBack = &CGenericMenuBack;
//CMenuForwarder * const GenericMenuCancel = &CGenericMenuCancel;
//CMenuForwarder * const GenericMenuNext = &CGenericMenuNext;

//
CMenuForwarderItemMenuIcon * const GenericMenuBackItemMenuIcon = &CGenericMenuBackItemMenuIcon;

//
CMenuSeparatorItemMenuIcon * const GenericMenuSeparatorItemMenuIcon = &CGenericMenuSeparatorItemMenuIcon;
CMenuSeparatorItemMenuIcon * const GenericMenuSeparatorLineItemMenuIcon = &CGenericMenuSeparatorLineItemMenuIcon;

// CMenuItem
void CMenuItem::init(const int X, const int Y, const int DX, const int OFFX)
{
	x    = X;
	y    = Y;
	dx   = DX;
	offx = OFFX;
}

void CMenuItem::setActive(const bool Active)
{
	active = Active;
	
	if (x != -1)
		paint();
}

// CMenuWidget
CMenuWidget::CMenuWidget()
{
        nameString = g_Locale->getText(NONEXISTANT_LOCALE);
	name = NONEXISTANT_LOCALE;
        iconfile = "";
        selected = -1;
        iconOffset = 0;
	offx = offy = 0;
	
	//
	savescreen	= false;
	background	= NULL;
}

CMenuWidget::CMenuWidget(const neutrino_locale_t Name, const std::string & Icon, const int mwidth, const int mheight )
{
	name = Name;
        nameString = g_Locale->getText(NONEXISTANT_LOCALE);

	Init(Icon, mwidth, mheight);
}

CMenuWidget::CMenuWidget(const char* Name, const std::string & Icon, const int mwidth, const int mheight)
{
	name = NONEXISTANT_LOCALE;
        nameString = Name;

	Init(Icon, mwidth, mheight);
}

void CMenuWidget::Init(const std::string & Icon, const int mwidth, const int mheight)
{
        frameBuffer = CFrameBuffer::getInstance();
        iconfile = Icon;
        selected = -1;
        width = mwidth;
	
        if(width > (int) frameBuffer->getScreenWidth())
		width = frameBuffer->getScreenWidth();
	
      	height = mheight;
        wanted_height = mheight;

        current_page = 0;
	offx = offy = 0;
	
	//
	savescreen	= false;
	background	= NULL;
}

void CMenuWidget::move(int xoff, int yoff)
{
	offx = xoff;
	offy = yoff;
}

CMenuWidget::~CMenuWidget()
{
	for(unsigned int count = 0; count < items.size(); count++) 
	{
		CMenuItem * item = items[count];
		
		if ((item != GenericMenuSeparator) && (item != GenericMenuSeparatorLine) && (item != GenericMenuBack))
			delete item;
	}

	items.clear();
	page_start.clear();
}

void CMenuWidget::addItem(CMenuItem *menuItem, const bool defaultselected)
{
	if (defaultselected)
		selected = items.size();
	
	//TODO:need marker to skip not needed item (e.g HDD setup when any device is pluged on)
	items.push_back(menuItem);
}

bool CMenuWidget::hasItem()
{
	return !items.empty();
}

int CMenuWidget::exec(CMenuTarget * parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int pos = 0;
	exit_pressed = false;

	if (parent)
		parent->hide();

	paint();
	
	//
	frameBuffer->blit();

	int retval = menu_return::RETURN_REPAINT;
	unsigned long long int timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	//control loop
	do {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

		if ( msg <= CRCInput::RC_MaxRC ) 
		{
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
		}
		
		int handled = false;

		// colored/numeric direct key
		for (unsigned int i = 0; i < items.size(); i++) 
		{
			CMenuItem * titem = items[i];
			
			if ((titem->directKey != CRCInput::RC_nokey) && (titem->directKey == msg)) 
			{
				if (titem->isSelectable()) 
				{
					items[selected]->paint( false );
					selected = i;
					msg = CRCInput::RC_ok;
				} 
				else 
				{
					// swallow-key...
					handled = true;
				}
				break;
			}
		}

		if (!handled) 
		{
			switch (msg) 
			{
				case (NeutrinoMessages::EVT_TIMER):
					if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) 
					{
						retval = menu_return::RETURN_EXIT_ALL;
						msg = CRCInput::RC_timeout;
					}
					break;
					
				case (CRCInput::RC_page_up) :
				case (CRCInput::RC_page_down) :
					if(msg == CRCInput::RC_page_up) 
					{
						if(current_page) 
						{
							pos = (int) page_start[current_page] - 1;
							for (unsigned int count=pos ; count > 0; count--) 
							{
								CMenuItem * item = items[pos];
								if ( item->isSelectable() ) 
								{
									if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page])) 
									{
										items[selected]->paint( false );
										item->paint( true );
										selected = pos;
									} 
									else 
									{
										selected=pos;
										paintItems();
									}
									break;
								}
								pos--;
							}
						} 
						else 
						{
							pos = 0;
							for (unsigned int count = 0; count < items.size(); count++) 
							{
								CMenuItem * item = items[pos];
								if ( item->isSelectable() ) 
								{
									if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page])) 
									{
										items[selected]->paint( false );
										item->paint( true );
										selected = pos;
									} 
									else 
									{
										selected = pos;
										paintItems();
									}
									break;
								}
								pos++;
							}
						}
					}
					else if(msg == CRCInput::RC_page_down) 
					{
						pos = (int) page_start[current_page + 1];// - 1;
						if(pos >= (int) items.size()) 
							pos = items.size()-1;
						for (unsigned int count = pos ; count < items.size(); count++) 
						{
							CMenuItem * item = items[pos];
							if ( item->isSelectable() ) 
							{
								if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page])) 
								{
									items[selected]->paint( false );
									item->paint( true );
									selected = pos;
								} 
								else 
								{
									selected = pos;
									paintItems();
								}
								break;
							}
							pos++;
						}
					}
					break;
					
				case (CRCInput::RC_up) :
				case (CRCInput::RC_down) :
					{
						//search next / prev selectable item
						for (unsigned int count = 1; count < items.size(); count++) 
						{
							if ( msg == CRCInput::RC_up ) 
							{
								pos = selected - count;
								if ( pos < 0 )
									pos += items.size();
							}
							else if( msg == CRCInput::RC_down ) 
							{
								pos = (selected + count)%items.size();
							}

							CMenuItem * item = items[pos];

							if ( item->isSelectable() ) 
							{
								if ((pos < (int)page_start[current_page + 1]) && (pos >= (int)page_start[current_page]))
								{ 
									// Item is currently on screen
									//clear prev. selected
									items[selected]->paint( false );
									//select new
									item->paint( true );
									selected = pos;
								} 
								else 
								{
									selected = pos;
									paintItems();
								}
								break;
							}
						}
					}
					break;
					
				case (CRCInput::RC_left):
					if(!(items[selected]->can_arrow)) 
					{
						msg = CRCInput::RC_timeout;
						break;
					}
					
				case (CRCInput::RC_right):
				case (CRCInput::RC_ok):
					{
						if(hasItem()) 
						{
							//exec this item...
							CMenuItem * item = items[selected];
							item->msg = msg;
							
							int rv = item->exec( this );
							
							switch ( rv ) 
							{
								case menu_return::RETURN_EXIT_ALL:
									retval = menu_return::RETURN_EXIT_ALL;
									
								case menu_return::RETURN_EXIT:
									msg = CRCInput::RC_timeout;
									break;
									
								case menu_return::RETURN_REPAINT:
									paint();
									break;
							}
						} 
						else
							msg = CRCInput::RC_timeout;
					}
					break;

				case (CRCInput::RC_home):
					exit_pressed = true;
					msg = CRCInput::RC_timeout;
					break;
					
				case (CRCInput::RC_timeout):
					break;

				case (CRCInput::RC_sat):
				case (CRCInput::RC_favorites):
					g_RCInput->postMsg (msg, 0);
					
				//close any menue on setup-key
				case (CRCInput::RC_setup):
					{
						msg = CRCInput::RC_timeout;
						retval = menu_return::RETURN_EXIT_ALL;
					}
					break;

				default:
					if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) 
					{
						retval = menu_return::RETURN_EXIT_ALL;
						msg = CRCInput::RC_timeout;
					}
			}

			if ( msg <= CRCInput::RC_MaxRC )
			{
				// recalculate timeout f�r RC-Tasten
				timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);
			}
		}
			
		//
		frameBuffer->blit();
	}
	while ( msg != CRCInput::RC_timeout );
	
	hide();	

	// vfd
	if(!parent)
	{
		if(CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_iptv)
			CVFD::getInstance()->setMode(CVFD::MODE_IPTV);
		else
			CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	}

	// init items
	for (unsigned int count = 0; count < items.size(); count++) 
	{
		CMenuItem * item = items[count];
		item->init(-1, 0, 0, 0);
	}
	
	return retval;
}

void CMenuWidget::hide()
{
	if( savescreen && background)
		restoreScreen();//FIXME
	else
		frameBuffer->paintBackgroundBoxRel(x, y, full_width, full_height); 
	
	//
	frameBuffer->blit();
}

void CMenuWidget::paint()
{
	const char * l_name;
	
	if(name == NONEXISTANT_LOCALE)
		l_name = nameString.c_str();
	else
        	l_name = g_Locale->getText(name);	

	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8 );

	height = wanted_height;

	// recalculate height
	if(height > ((int)frameBuffer->getScreenHeight() - 10))
		height = frameBuffer->getScreenHeight() - 10;

	int neededWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(l_name, true); // UTF-8

	// recalculate width
	if (neededWidth > width - 48) 
	{
		width = neededWidth + 49;
		
		if(width > (int)frameBuffer->getScreenWidth())
			width = frameBuffer->getScreenWidth();
	}

	// head height
	int icon_head_w, icon_head_h;
	frameBuffer->getIconSize(iconfile.c_str(), &icon_head_w, &icon_head_h);
	int hheight = std::max(icon_head_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
	
	// globals
	int itemHeightTotal = 0;
	int item_height = 0;
	int heightCurrPage = 0;
	page_start.clear();
	page_start.push_back(0);
	total_pages = 1;
	int sp_height = 5;
	
	// foot height
	int icon_foot_w, icon_foot_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
	int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;

	//
	for (unsigned int i = 0; i < items.size(); i++) 
	{
		item_height = items[i]->getHeight();
		itemHeightTotal += item_height;
		heightCurrPage += item_height;

		if(heightCurrPage > (height - hheight - 2*sp_height - fheight)) 
		{
			page_start.push_back(i);
			total_pages++;
			heightCurrPage = item_height;
		}
	}

	page_start.push_back(items.size());

	// icon offset
	iconOffset = 0;

	for (unsigned int i = 0; i < items.size(); i++) 
	{
		if ((!(items[i]->iconName.empty())) || CRCInput::isNumeric(items[i]->directKey))
		{
			iconOffset = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
			break;
		}
	}

	// shrink menu if less items
	if(hheight + sp_height + itemHeightTotal + sp_height + fheight < height)
		height = hheight + sp_height + itemHeightTotal + sp_height + fheight;

	// coordinations
	x = offx + frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - width ) >> 1 );
	y = offy + frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - height) >> 1 );

	int sb_width = 0;
	
	if(total_pages > 1)
		sb_width = SCROLLBAR_WIDTH;
	else
		sb_width = 0;
	
	full_width = width + sb_width;
	full_height = height;
	
	FULL_HEIGHT = full_height;
	
	//
	if(savescreen) 
		saveScreen();

	// paint head
	frameBuffer->paintBoxRel(x, y, width + sb_width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_TOP);
	
	//paint icon
	frameBuffer->paintIcon(iconfile, x + BORDER_LEFT, y + (hheight - icon_head_h)/2);
	
	// head title (centered)
	int stringstartposX = x + ((width + sb_width)>> 1) - ( neededWidth >> 1) - BORDER_LEFT;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(stringstartposX, y + (hheight - g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight(), width - BORDER_RIGHT - (stringstartposX - x), l_name, COL_MENUHEAD, 0, true); // UTF-8
	
	// paint separator
	frameBuffer->paintBoxRel(x, y + hheight, width + sb_width, sp_height, COL_MENUCONTENTDARK_PLUS_0);
	
	//paint foot
	frameBuffer->paintBoxRel(x, y + (height - fheight), width + sb_width, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
	
	// all height position (needed to paint itemIcon and help text)
	HEIGHT = y + full_height;
	
	//item_start_y
	item_start_y = y + hheight + sp_height;
	
	// paint items
	paintItems();
}

/* paint items */
void CMenuWidget::paintItems()
{
	// fheight
	int icon_foot_w, icon_foot_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
	int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
	
	// item height
	int item_height = height - fheight - (item_start_y - y); // all items height without sep up/down

	//Item not currently on screen
	if (selected >= 0)
	{
		while(selected < (int)page_start[current_page])
			current_page--;
		
		while(selected >= (int)page_start[current_page + 1])
			current_page++;
	}

	// paint right scroll bar if we have more then one page
	if(total_pages > 1)
	{
		//sb_width = SCROLLBAR_WIDTH;
		int sbh = ((item_height - 4) / total_pages);

		//scrollbar
		frameBuffer->paintBoxRel(x + width, item_start_y, SCROLLBAR_WIDTH, item_height, COL_MENUCONTENT_PLUS_1); //15 = sb width
		frameBuffer->paintBoxRel(x + width + 2, item_start_y + 2 + current_page * sbh, SCROLLBAR_WIDTH - 4, sbh, COL_MENUCONTENT_PLUS_3);
	}
	
	// paint items background
	frameBuffer->paintBoxRel(x, item_start_y, width, item_height, COL_MENUCONTENTDARK_PLUS_0 );

	// paint items
	int ypos = item_start_y;
	
	for (unsigned int count = 0; count < items.size(); count++) 
	{
		CMenuItem * item = items[count];

		if ((count >= page_start[current_page]) && (count < page_start[current_page + 1])) 
		{
			item->init(x, ypos, width, iconOffset);
			
			if( (item->isSelectable()) && (selected == -1) ) 
			{
				ypos = item->paint(true);
				selected = count;
			} 
			else 
			{
				ypos = item->paint( selected == ((signed int) count) );
			}
		} 
		else 
		{
			/* x = -1 is a marker which prevents the item from being painted on setActive changes */
			item->init(-1, 0, 0, 0);
		}	
	} 
}

void CMenuWidget::saveScreen()
{
	if(!savescreen)
		return;

	delete[] background;

	background = new fb_pixel_t[full_width*full_height];
	
	if(background)
		frameBuffer->SaveScreen(x, y, full_width, full_height, background);
}

void CMenuWidget::restoreScreen()
{
	if(background) 
	{
		if(savescreen)
			frameBuffer->RestoreScreen(x, y, full_width, full_height, background);
		
		//delete [] background;
		//background = NULL;
	}
}

void CMenuWidget::enableSaveScreen(bool enable)
{
	savescreen = enable;
	
	if(!enable && background) 
	{
		delete[] background;
		background = NULL;
	}
}

//CMenuOptionNumberChooser
CMenuOptionNumberChooser::CMenuOptionNumberChooser(const neutrino_locale_t name, int * const OptionValue, const bool Active, const int min_value, const int max_value, CChangeObserver * const Observ, const int print_offset, const int special_value, const neutrino_locale_t special_value_name, const char * non_localized_name)
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	height = std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;

	optionName           = name;
	active               = Active;
	optionValue          = OptionValue;

	lower_bound          = min_value;
	upper_bound          = max_value;

	display_offset       = print_offset;

	localized_value      = special_value;
	localized_value_name = special_value_name;

	optionString         = non_localized_name;
	can_arrow	= true;
	observ = Observ;
}

int CMenuOptionNumberChooser::exec(CMenuTarget *)
{
	if( msg == CRCInput::RC_left ) 
	{
		if (((*optionValue) > upper_bound) || ((*optionValue) <= lower_bound))
			*optionValue = upper_bound;
		else
			(*optionValue)--;
	} 
	else 
	{
		if (((*optionValue) >= upper_bound) || ((*optionValue) < lower_bound))
			*optionValue = lower_bound;
		else
			(*optionValue)++;
	}
	
	paint(true);
	
	if(observ)
		observ->changeNotify(optionName, optionValue);

	return menu_return::RETURN_NONE;
}

int CMenuOptionNumberChooser::paint(bool selected)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	uint8_t color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;

	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	
	if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
	
	// paint item
	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, dx - (BORDER_LEFT + BORDER_RIGHT), height, bgcolor); //FIXME

	const char * l_option;
	char option_value[11];

	if ((localized_value_name == NONEXISTANT_LOCALE) || ((*optionValue) != localized_value))
	{
		sprintf(option_value, "%d", ((*optionValue) + display_offset));
		l_option = option_value;
	}
	else
		l_option = g_Locale->getText(localized_value_name);

	int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_option, true); // UTF-8
	int stringstartposName = x + BORDER_LEFT + LOCAL_OFFSET;
	int stringstartposOption = x + dx - stringwidth - BORDER_RIGHT - 5; //+ offx

	const char * l_optionName = (optionString != NULL) ? optionString : g_Locale->getText(optionName);

	// locale
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - (stringstartposName - x), l_optionName, color, 0, true); // UTF-8
	// option value
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - (stringstartposOption - x), l_option, color, 0, true); // UTF-8
	
	if(selected)
	{  
		//helpbar
		int icon_foot_w, icon_foot_h;
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
		int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
		int fposy = HEIGHT - fheight;
		
		// refresh
		frameBuffer->paintBoxRel(x, fposy, dx, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
		
		// paint help icon
		int icon_w = 0;
		int icon_h = 0;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_w, &icon_h);
			
		frameBuffer->paintIcon(NEUTRINO_ICON_INFO, x + BORDER_LEFT - 2, fposy + (fheight - icon_h)/2);
			
		// help text locale
		const char * help_text = (optionString != NULL) ? optionString : g_Locale->getText(optionName);
			
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(x + BORDER_LEFT + icon_w + 5, fposy + (fheight -g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight(), dx - (x + (offx == 0? 0 : offx) + BORDER_LEFT + icon_w + 5 - x), help_text, COL_MENUFOOT, 0, true); // UTF-8
		
		// vfd
		char str[256];
		snprintf(str, 255, "%s %s", l_optionName, l_option);

		CVFD::getInstance()->showMenuText(0, str, -1, true);
	}

	return y + height;
}

// CMenuOptionChooser
CMenuOptionChooser::CMenuOptionChooser(const neutrino_locale_t OptionName, int *const OptionValue, const struct keyval *const Options, const unsigned Number_Of_Options, const bool Active, CChangeObserver * const Observ, const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	height = std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;
	
	optionNameString  = g_Locale->getText(OptionName);
	optionName        = OptionName;
	options           = Options;
	active            = Active;
	optionValue       = OptionValue;
	number_of_options = Number_Of_Options;
	observ            = Observ;
	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow	  = true;
	pulldown          = Pulldown;
}

CMenuOptionChooser::CMenuOptionChooser(const char *OptionName, int *const OptionValue, const struct keyval *const Options, const unsigned Number_Of_Options, const bool Active, CChangeObserver * const Observ, const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	height = std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;

	optionNameString  = OptionName;
	optionName        = NONEXISTANT_LOCALE;
	options           = Options;
	active            = Active;
	optionValue       = OptionValue;
	number_of_options = Number_Of_Options;
	observ            = Observ;
	directKey         = DirectKey;
	iconName          = IconName;
	can_arrow	  = true;
	pulldown          = Pulldown;
}

void CMenuOptionChooser::setOptionValue(const int newvalue)
{
	*optionValue = newvalue;
}

int CMenuOptionChooser::getOptionValue(void) const
{
	return *optionValue;
}

int CMenuOptionChooser::exec(CMenuTarget *parent)
{
	bool wantsRepaint = false;
	int ret = menu_return::RETURN_NONE;
	
	if (parent)
		parent->hide();

	// pulldown
	if( (!parent || msg == CRCInput::RC_ok) && pulldown ) 
	{
		int select = -1;
		char cnt[5];
		CMenuWidget *menu = new CMenuWidget(optionNameString.c_str(), NEUTRINO_ICON_SETTINGS);
		
		//if(parent)
		//	menu->move(20, 0);
		
		menu->enableSaveScreen(true);
		
		CMenuSelectorTarget *selector = new CMenuSelectorTarget(&select);
		
		// intros
		//menu->addItem(GenericMenuSeparator);

		for(unsigned int count = 0; count < number_of_options; count++) 
		{
			bool selected = false;
			const char *l_option;
			
			if (options[count].key == (*optionValue))
				selected = true;

			if(options[count].valname != 0)
				l_option = options[count].valname;
			else
				l_option = g_Locale->getText(options[count].value);
			
			sprintf(cnt, "%d", count);
			menu->addItem(new CMenuForwarderNonLocalized(l_option, true, NULL, selector, cnt), selected);
		}
		
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;
		
		if(select >= 0) 
			*optionValue = options[select].key;
		
		delete menu;
		delete selector;
	} 
	else 
	{
		for(unsigned int count = 0; count < number_of_options; count++) 
		{
			if (options[count].key == (*optionValue)) 
			{
				if( msg == CRCInput::RC_left ) 
				{
					if(count > 0)
						*optionValue = options[(count-1) % number_of_options].key;
					else
						*optionValue = options[number_of_options-1].key;
				} 
				else
					*optionValue = options[(count+1) % number_of_options].key;
				
				wantsRepaint = true;
				break;
			}
		}
	}
	
	if(parent)
		paint(true);
	
	if(observ)
		wantsRepaint = observ->changeNotify(optionName, optionValue);

	if ( wantsRepaint )
		ret = menu_return::RETURN_REPAINT;

	return ret;
}

int CMenuOptionChooser::paint( bool selected )
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();

	uint8_t color   = COL_MENUCONTENT;
	fb_pixel_t bgcolor = COL_MENUCONTENT_PLUS_0;

	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
	
	// paint item
	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, dx - (BORDER_LEFT + BORDER_RIGHT), height, bgcolor); //FIXME

	neutrino_locale_t option = NONEXISTANT_LOCALE;
	const char * l_option = NULL;

	for(unsigned int count = 0 ; count < number_of_options; count++) 
	{
		if (options[count].key == *optionValue) 
		{
			option = options[count].value;
			if(options[count].valname != 0)
				l_option = options[count].valname;
			else
				l_option = g_Locale->getText(option);
			break;
		}
	}

	if(l_option == NULL) 
	{
		*optionValue = options[0].key;
		option = options[0].value;
		if(options[0].valname != 0)
			l_option = options[0].valname;
		else
			l_option = g_Locale->getText(option);
	}

	// paint icon (left)
	int icon_w = 0;
	int icon_h = 0;
		
	if (!(iconName.empty()))
	{
		frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
		
		frameBuffer->paintIcon(iconName, x + BORDER_LEFT + ICON_OFFSET, y + ((height - icon_h)/2) );
	}
	else if (CRCInput::isNumeric(directKey))
	{
		// define icon name depends of numeric value
		char i_name[6]; // X +'\0'
		snprintf(i_name, 6, "%d", CRCInput::getNumericValue(directKey));
		i_name[5] = '\0'; // even if snprintf truncated the string, ensure termination
		iconName = i_name;
		
		if (!iconName.empty())
		{
			frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
			
			frameBuffer->paintIcon(iconName, x + BORDER_LEFT + 5, y+ ((height - icon_h)/2) );
		}
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + BORDER_LEFT + 5, y+ height, height, CRCInput::getKeyName(directKey), color, height);
        }

	int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_option, true); // UTF-8
	int stringstartposName = x + BORDER_LEFT + ICON_OFFSET + (icon_w? icon_w + LOCAL_OFFSET : 0);
	int stringstartposOption = x + dx - (stringwidth + BORDER_RIGHT  + ICON_OFFSET); //+ offx

	// locale
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - (stringstartposName - x), optionNameString.c_str(), color, 0, true); // UTF-8
	
	// option
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - (stringstartposOption - x), l_option, color, 0, true); // UTF-8

	if (selected)
	{
#if 0	  
		//helpbar
		//int fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();	// helpbar
		int icon_foot_w, icon_foot_h;
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
		int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
		
		// refresh
		frameBuffer->paintBoxRel(x, HEIGHT - fheight, dx, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
		
		// paint help icon
		int icon_h_w = 0;
		int icon_h_h = 0;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_h_w, &icon_h_h);
			
		frameBuffer->paintIcon(NEUTRINO_ICON_INFO, x + BORDER_LEFT - 2, HEIGHT - fheight + (fheight - icon_h_h)/2);
			
		// help text locale
		const char * help_text = optionNameString.c_str();
		int HelpTextHeight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
			
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(x + BORDER_LEFT + icon_h_w + 5, HEIGHT - HelpTextHeight/3, dx - (x + (offx == 0? 0 : offx) + BORDER_LEFT + icon_h_w + 5 - x), help_text, COL_MENUFOOT, 0, true); // UTF-8
#endif		
		
		// vfd
		char str[256];
		snprintf(str, 255, "%s %s", optionNameString.c_str(), l_option);

		CVFD::getInstance()->showMenuText(0, str, -1, true);
	}

	return y + height;
}

// CMenuOptionStringChooser
CMenuOptionStringChooser::CMenuOptionStringChooser(const neutrino_locale_t OptionName, char * OptionValue, bool Active, CChangeObserver* Observ, const neutrino_msg_t DirectKey, const std::string & IconName, bool Pulldown)
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	height = std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;

	optionName  = OptionName;
	active      = Active;
	optionValue = OptionValue;
	observ      = Observ;

	directKey   = DirectKey;
	iconName    = IconName;
	can_arrow   = true;
	pulldown    = Pulldown;
}

CMenuOptionStringChooser::~CMenuOptionStringChooser()
{
	options.clear();
}

void CMenuOptionStringChooser::addOption(const char * const value)
{
	options.push_back(std::string(value));
}

int CMenuOptionStringChooser::exec(CMenuTarget *parent)
{
	bool wantsRepaint = false;
	int ret = menu_return::RETURN_NONE;

	if (parent)
		parent->hide();

	if( (!parent || msg == CRCInput::RC_ok) && pulldown ) 
	{
		int select = -1;
		char cnt[5];
		
		CMenuWidget * menu = new CMenuWidget(optionName, NEUTRINO_ICON_SETTINGS);
		
		//if(parent) 
		//	menu->move(20, 0);
		
		menu->enableSaveScreen(true);
		
		CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);
		
		// intros
		//menu->addItem(GenericMenuSeparator);
		
		for(unsigned int count = 0; count < options.size(); count++) 
		{
			bool selected = false;
			if (strcmp(options[count].c_str(), optionValue) == 0)
				selected = true;
			sprintf(cnt, "%d", count);
			menu->addItem(new CMenuForwarderNonLocalized(options[count].c_str(), true, NULL, selector, cnt), selected);
		}
		menu->exec(NULL, "");
		ret = menu_return::RETURN_REPAINT;
		
		if(select >= 0)
			strcpy(optionValue, options[select].c_str());
		
		delete menu;
		delete selector;
	} 
	else 
	{
		//select next value
		for(unsigned int count = 0; count < options.size(); count++) 
		{
			if (strcmp(options[count].c_str(), optionValue) == 0) 
			{
				if( msg == CRCInput::RC_left ) 
				{
					if(count > 0)
						strcpy(optionValue, options[(count - 1) % options.size()].c_str());
					else
						strcpy(optionValue, options[options.size() - 1].c_str());
				} 
				else
					strcpy(optionValue, options[(count + 1) % options.size()].c_str());
				wantsRepaint = true;
				break;
			}
		}
	}

	if(parent)
		paint(true);
	
	if(observ) 
		wantsRepaint = observ->changeNotify(optionName, optionValue);
	
	if (wantsRepaint)
		ret = menu_return::RETURN_REPAINT;

	return ret;
}

int CMenuOptionStringChooser::paint( bool selected )
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	uint8_t color   = COL_MENUCONTENT;
	fb_pixel_t bgcolor = COL_MENUCONTENT_PLUS_0;
	
	if (selected) 
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (!active) 
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
	
	// paint item
	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, dx - (BORDER_LEFT + BORDER_RIGHT), height, bgcolor); //FIXME

	// paint icon
	int icon_w = 0;
	int icon_h = 0;
		
	if (!(iconName.empty()))
	{
		frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
		
		frameBuffer->paintIcon(iconName, x + BORDER_LEFT + ICON_OFFSET, y + ((height - icon_h)/2) );	
	}
	else if (CRCInput::isNumeric(directKey))
	{
		// define icon name depends of numeric value
		char i_name[6]; // X +'\0'
		snprintf(i_name, 6, "%d", CRCInput::getNumericValue(directKey));
		i_name[5] = '\0'; // even if snprintf truncated the string, ensure termination
		iconName = i_name;
		
		if (!iconName.empty())
		{
			frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
			
			frameBuffer->paintIcon(iconName, x + BORDER_LEFT + 5, y+ ((height - icon_h)/2) );
		}
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + BORDER_LEFT  + 5, y + height, height, CRCInput::getKeyName(directKey), color, height);
        }
        
        // locale text
	const char * l_optionName = g_Locale->getText(optionName);
	int stringstartposName = x + BORDER_LEFT + ICON_OFFSET + (icon_w? icon_w + LOCAL_OFFSET : 0);
	
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposName,   y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx- (stringstartposName - x),  l_optionName, color, 0, true); // UTF-8
	
	// option value
	int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(optionValue, true) + BORDER_RIGHT;
	int stringstartposOption = std::max(stringstartposName + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_optionName, true) + ICON_OFFSET, x + dx - stringwidth - ICON_OFFSET); //+ offx
	
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - (stringstartposOption - x),  optionValue, color, 0, true);
	
	if (selected)
	{
#if 0
		//helpbar
		//int fheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();	// helpbar
		int icon_foot_w, icon_foot_h;
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
		int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
		
		// refresh
		frameBuffer->paintBoxRel(x, HEIGHT - fheight, dx, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
		
		// paint help icon
		int icon_h_w = 0;
		int icon_h_h = 0;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_h_w, &icon_h_h);
			
		frameBuffer->paintIcon(NEUTRINO_ICON_INFO, x + BORDER_LEFT - 2, HEIGHT - fheight + (fheight - icon_h_h)/2);
			
		// help text locale
		const char * help_text = l_optionName;
		int HelpTextHeight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
			
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(x + BORDER_LEFT + icon_h_w + 5, HEIGHT - HelpTextHeight/3, dx - (x + (offx == 0? 0 : offx) + BORDER_LEFT + icon_h_w + 5 - x), help_text, COL_MENUFOOT, 0, true); // UTF-8
#endif		

		// vfd
		char str[256];
		snprintf(str, 255, "%s %s", l_optionName, optionValue);

		CVFD::getInstance()->showMenuText(0, str, -1, true);
	}

	return y + height;
}

// CMenuOptionLanguageChooser
CMenuOptionLanguageChooser::CMenuOptionLanguageChooser(char *OptionValue, CChangeObserver *Observ, const char * const IconName)
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	height = std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;
	
	optionValue = OptionValue;
	observ      = Observ;

	directKey   = CRCInput::RC_nokey;
	iconName = IconName ? IconName : "";
}

CMenuOptionLanguageChooser::~CMenuOptionLanguageChooser()
{
	options.clear();
}

void CMenuOptionLanguageChooser::addOption(const char * const value)
{
	options.push_back(std::string(value));
}

int CMenuOptionLanguageChooser::exec(CMenuTarget*)
{
	bool wantsRepaint = false;

	//select value
	for(unsigned int count = 0; count < options.size(); count++)
	{
		if (strcmp(options[count].c_str(), optionValue) == 0)
		{
			strcpy(g_settings.language, options[(count + 1) % options.size()].c_str());
			break;
		}
	}

	paint(true);
	
	if(observ)
	{
		wantsRepaint = observ->changeNotify(LOCALE_LANGUAGESETUP_SELECT, optionValue);
	}
	
	return menu_return::RETURN_EXIT;
	
	if ( wantsRepaint )
		return menu_return::RETURN_REPAINT;
	else
		return menu_return::RETURN_NONE;
}

int CMenuOptionLanguageChooser::paint( bool selected )
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	
	uint8_t color   = COL_MENUCONTENT;
	fb_pixel_t bgcolor = COL_MENUCONTENT_PLUS_0;
	
	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	
	// paint item
	CFrameBuffer::getInstance()->paintBoxRel(x + BORDER_LEFT, y, dx - (BORDER_LEFT + BORDER_RIGHT), height, bgcolor); //FIXME

	// paint icon
	int icon_w;
	int icon_h;
	
	if (!(iconName.empty()))
	{
		frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
		
		frameBuffer->paintIcon(iconName, x + BORDER_LEFT + ICON_OFFSET, y+ ((height - icon_h)/2) );
	}

	// locale
	int stringstartposOption = x + BORDER_LEFT + ICON_OFFSET + (icon_w? icon_w + LOCAL_OFFSET: 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx- (stringstartposOption - x), optionValue, color, 0, true); //UTF-8

	if (selected)
	{
		//helpbar
		int icon_foot_w, icon_foot_h;
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
		int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
		int fposy = HEIGHT - fheight;
		
		// refresh
		frameBuffer->paintBoxRel(x, fposy, dx, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
		
		// paint help icon
		int icon_h_w = 0;
		int icon_h_h = 0;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_h_w, &icon_h_h);
			
		frameBuffer->paintIcon(NEUTRINO_ICON_INFO, x + BORDER_LEFT - 2, fposy + (fheight - icon_h_h)/2);
			
		// help text locale
		const char * help_text = optionValue;
			
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(x + BORDER_LEFT + icon_h_w + 5, fposy + (fheight - g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight(), dx - (x + (offx == 0? 0 : offx) + BORDER_LEFT + icon_h_w + 5 - x), help_text, COL_MENUFOOT, 0, true); // UTF-8
		
		// vfd
		CVFD::getInstance()->showMenuText(1, optionValue);
	}

	return y + height;
}

//CMenuForwarder
CMenuForwarder::CMenuForwarder(const neutrino_locale_t Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName)
{
	option = Option;
	option_string = NULL;
	text = Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
}

CMenuForwarder::CMenuForwarder(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName)
{
	option = NULL;
	option_string = &Option;
	text=Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
}

int CMenuForwarder::getHeight(void) const
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	
	return std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;
}

int CMenuForwarder::getWidth(void) const
{
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(text), true);
	const char * option_text = NULL;

	if (option)
		option_text = option;
	else if (option_string)
		option_text = option_string->c_str();
	

        if (option_text != NULL)
                tw += BORDER_LEFT + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_text, true);

	return tw;
}

int CMenuForwarder::exec(CMenuTarget* parent)
{
	if(jumpTarget)
	{
		return jumpTarget->exec(parent, actionKey);
	}
	else
	{
		return menu_return::RETURN_EXIT;
	}
}

const char * CMenuForwarder::getOption(void)
{
	if (option)
		return option;
	else
		if (option_string)
			return option_string->c_str();
		else
			return NULL;
}

const char * CMenuForwarder::getName(void)
{
	return g_Locale->getText(text);
}

int CMenuForwarder::paint(bool selected)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	int height = getHeight();
	const char * l_text = getName();
	
	int stringstartposX = x + (offx == 0 ? BORDER_LEFT : offx) + BORDER_LEFT;

	const char * option_text = getOption();
	
	if (selected)
	{
		//help bar
		int icon_foot_w, icon_foot_h;
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
		int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
		int fposy = HEIGHT - fheight;
		
		// refresh
		frameBuffer->paintBoxRel(x, fposy, dx, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
			
		// paint help icon
		int icon_w = 0;
		int icon_h = 0;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_w, &icon_h);
			
		frameBuffer->paintIcon(NEUTRINO_ICON_INFO, x + BORDER_LEFT - 2, fposy + (fheight - icon_h)/2);
			
		// help text locale
		const char * help_text = getName();
			
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(x + BORDER_LEFT + icon_w + 5, fposy + (fheight - g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight(), dx - (x + (offx == 0? 0 : offx) + BORDER_LEFT + icon_w + 5 - x), help_text, COL_MENUFOOT, 0, true); // UTF-8
		
		// vfd
		char str[256];

		if (option_text != NULL) 
		{
			snprintf(str, 255, "%s %s", l_text, option_text);

			CVFD::getInstance()->showMenuText(0, str, -1, true);
		} 
		else
		{
			CVFD::getInstance()->showMenuText(0, l_text, -1, true);
		}
	}

	uint8_t color   = COL_MENUCONTENT;
	fb_pixel_t    bgcolor = COL_MENUCONTENT_PLUS_0;

	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
	
	// paint item
	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, dx -(BORDER_LEFT + BORDER_RIGHT), height, bgcolor); //FIXME
	
	// paint icon/direkt-key
	int icon_w = 0;
	int icon_h = 0;
	
	if (!iconName.empty())
	{
		//get icon size
		frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
		
		frameBuffer->paintIcon(iconName, x + BORDER_LEFT + ICON_OFFSET, y + ((height - icon_h)/2) );
	}
	else if (CRCInput::isNumeric(directKey))
	{
		//define icon name depends of numeric value
		char i_name[6]; // X +'\0'
		snprintf(i_name, 6, "%d", CRCInput::getNumericValue(directKey));
		i_name[5] = '\0'; // even if snprintf truncated the string, ensure termination
		iconName = i_name;
		
		if (!iconName.empty())
		{
			//get icon size
			frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
			
			frameBuffer->paintIcon(iconName, x + BORDER_LEFT + 5, y+ ((height - icon_h)/2) );
		}
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + BORDER_LEFT + 5, y + height, height, CRCInput::getKeyName(directKey), color, height);
	}
	
	// locale text
	stringstartposX = x + BORDER_LEFT + ICON_OFFSET + (icon_w?icon_w + LOCAL_OFFSET : 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - BORDER_LEFT - 5 - (stringstartposX - x), l_text, color, 0, true); // UTF-8

	//option-text
	if (option_text != NULL)
	{
		int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_text, true);
		int stringstartposOption = std::max(stringstartposX + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true) + BORDER_LEFT + BORDER_LEFT/2, x + dx - stringwidth - BORDER_RIGHT - BORDER_RIGHT/2); //+ offx
		
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), dx - BORDER_LEFT - (stringstartposOption- x),  option_text, color, 0, true);
	}
	
	return y + height;
}

// CMenuForwarderNonLocalized 
const char * CMenuForwarderNonLocalized::getName(void)
{
	return the_text.c_str();
}

CMenuForwarderNonLocalized::CMenuForwarderNonLocalized(const char * const Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName) : CMenuForwarder(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName)
{
	the_text = Text;
}

CMenuForwarderNonLocalized::CMenuForwarderNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName) : CMenuForwarder(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName)
{
	the_text = Text;
}

// CMenuSeparator
CMenuSeparator::CMenuSeparator(const int Type, const neutrino_locale_t Text)
{
	directKey = CRCInput::RC_nokey;
	iconName = "";
	type     = Type;
	text     = Text;
}


int CMenuSeparator::getHeight(void) const
{
	if ((type & LINE))
	{
		return (text == NONEXISTANT_LOCALE) ? BORDER_LEFT : g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	}
	else
	{
		int iconName_w, iconName_h;
		CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
		
		return std::max(iconName_h, (text == NONEXISTANT_LOCALE) ? BORDER_LEFT : g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;
	}
}

int CMenuSeparator::getWidth(void) const
{
	return 0;
}

const char * CMenuSeparator::getString(void)
{
	return g_Locale->getText(text);
}

int CMenuSeparator::paint(bool /*selected*/)
{
	int height;
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	height = getHeight();

	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, dx - BORDER_LEFT - BORDER_RIGHT, height, COL_MENUCONTENT_PLUS_0);

	if ((type & LINE))
	{
		frameBuffer->paintHLineRel(x + BORDER_LEFT, dx - BORDER_LEFT - BORDER_RIGHT, y + (height >> 1), COL_MENUCONTENTDARK_PLUS_0 );
		frameBuffer->paintHLineRel(x + BORDER_LEFT, dx - BORDER_LEFT - BORDER_RIGHT, y + (height >> 1) + 1, COL_MENUCONTENTDARK_PLUS_0 );
	}

	if ((type & STRING))
	{

		if (text != NONEXISTANT_LOCALE)
		{
			int stringstartposX;

			const char * l_text = getString();
			int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true); // UTF-8

			/* if no alignment is specified, align centered */
			if (type & ALIGN_LEFT)
				stringstartposX = x + (BORDER_LEFT + 10);
			else if (type & ALIGN_RIGHT)
				stringstartposX = x + dx - stringwidth - (BORDER_RIGHT + 10 );
			else /* ALIGN_CENTER */
				stringstartposX = x + (dx >> 1) - (stringwidth >> 1);

			frameBuffer->paintBoxRel(stringstartposX - 5, y, stringwidth + 10, height, COL_MENUCONTENT_PLUS_0);

			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, y + height, dx- (stringstartposX- x) , l_text, COL_MENUCONTENTINACTIVE, 0, true); // UTF-8
		}
	}

	return y + height;
}

bool CPINProtection::check()
{
	char cPIN[5];
	neutrino_locale_t hint = NONEXISTANT_LOCALE;
	
	do
	{
		cPIN[0] = 0;
		CPINInput * PINInput = new CPINInput(LOCALE_PINPROTECTION_HEAD, cPIN, 4, hint);
		PINInput->exec( getParent(), "");
		delete PINInput;
		hint = LOCALE_PINPROTECTION_WRONGCODE;
	} while ((strncmp(cPIN, validPIN, 4) != 0) && (cPIN[0] != 0));
	
	return ( strncmp(cPIN,validPIN, 4) == 0);
}

bool CZapProtection::check()
{
	int res;
	char cPIN[5];
	neutrino_locale_t hint2 = NONEXISTANT_LOCALE;
	
	do
	{
		cPIN[0] = 0;

		CPLPINInput* PINInput = new CPLPINInput(LOCALE_PARENTALLOCK_HEAD, cPIN, 4, hint2, fsk);

		res = PINInput->exec(getParent(), "");
		delete PINInput;

		hint2 = LOCALE_PINPROTECTION_WRONGCODE;
	} while ( (strncmp(cPIN,validPIN, 4) != 0) &&
		  (cPIN[0] != 0) &&
		  ( res == menu_return::RETURN_REPAINT ) &&
		  ( fsk >= g_settings.parentallock_lockage ) );
		  
	return ( ( strncmp(cPIN, validPIN, 4) == 0 ) ||
			 ( fsk < g_settings.parentallock_lockage ) );
}

int CLockedMenuForwarder::exec(CMenuTarget * parent)
{
	Parent = parent;
	
	if( (g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER) || AlwaysAsk )
	{
		if (!check())
		{
			Parent = NULL;
			return menu_return::RETURN_REPAINT;
		}
	}

	Parent = NULL;
	
	return CMenuForwarder::exec(parent);
}

int CMenuSelectorTarget::exec(CMenuTarget*/*parent*/, const std::string & actionKey)
{
        if (actionKey != "")
                *m_select = atoi(actionKey.c_str());
        else
                *m_select = -1;
	
        return menu_return::RETURN_EXIT;
}

// new
// CMenuForwarderItemMenuIcon
CMenuForwarderItemMenuIcon::CMenuForwarderItemMenuIcon(const neutrino_locale_t Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const ItemIcon, const neutrino_locale_t HelpText )
{
	option = Option;
	option_string = NULL;
	text = Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
	itemIcon = ItemIcon ? ItemIcon : "";
	helptext = HelpText;
}

CMenuForwarderItemMenuIcon::CMenuForwarderItemMenuIcon(const neutrino_locale_t Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const ItemIcon, const neutrino_locale_t HelpText)
{
	option = NULL;
	option_string = &Option;
	text = Text;
	active = Active;
	jumpTarget = Target;
	actionKey = ActionKey ? ActionKey : "";
	directKey = DirectKey;
	iconName = IconName ? IconName : "";
	itemIcon = ItemIcon ? ItemIcon : "";
	helptext = HelpText;
}

int CMenuForwarderItemMenuIcon::getHeight(void) const
{
	int iconName_w, iconName_h;
	CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
	
	return std::max(iconName_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;
}

int CMenuForwarderItemMenuIcon::getWidth(void) const
{
	int tw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(g_Locale->getText(text), true);
	const char * option_text = NULL;

	if (option)
		option_text = option;
	else if (option_string)
		option_text = option_string->c_str();
	

        if (option_text != NULL)
                tw += BORDER_LEFT + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_text, true);

	return tw;
}

int CMenuForwarderItemMenuIcon::exec(CMenuTarget* parent)
{
	if(jumpTarget)
		return jumpTarget->exec(parent, actionKey);
	else
		return menu_return::RETURN_EXIT;
}

const char * CMenuForwarderItemMenuIcon::getOption(void)
{
	if (option)
		return option;
	else
	{
		if (option_string)
			return option_string->c_str();
		else
			return NULL;
	}
}

const char * CMenuForwarderItemMenuIcon::getName(void)
{
	return g_Locale->getText(text);
}

const char * CMenuForwarderItemMenuIcon::getHelpText(void)
{
	return g_Locale->getText(helptext);
}

int CMenuForwarderItemMenuIcon::paint(bool selected)
{
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	int height = getHeight();
	const char * l_text = getName();
	int stringstartposX = x + (offx == 0? BORDER_LEFT : offx) + BORDER_LEFT;
	const char * option_text = getOption();	
	uint8_t color   = COL_MENUCONTENT;
	fb_pixel_t bgcolor = COL_MENUCONTENT_PLUS_0;

	if (selected)
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
	}
	else if (!active)
	{
		color   = COL_MENUCONTENTINACTIVE;
		bgcolor = COL_MENUCONTENTINACTIVE_PLUS_0;
	}
	
	if (selected)
	{
		// item icon
		if (!itemIcon.empty())
		{
			//NOTE: CMenuWidget menues are always centered
			/* get icon size */
			int icon_w = 0;
			int icon_h = 0;
			int hheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();

			int icon_foot_w, icon_foot_h;
			frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
			int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
			int sp_height = 5;

			
			// check item icon size w/h equal 0 means icons doesnt exist
			frameBuffer->getIconSize(itemIcon.c_str(), &icon_w, &icon_h);
			
			//refresh pic box
			if( ((FULL_HEIGHT - (hheight + fheight + 2*sp_height)) >= (ITEM_ICON_H) ) /*&& (icon_w >= 100 || icon_h >= 100)*/ )
				frameBuffer->paintBoxRel(x + BORDER_LEFT + (dx/3)*2 + (((dx - (dx/3)*2 - BORDER_RIGHT)/2) - ITEM_ICON_W/2), ( frameBuffer->getScreenHeight(true) - ITEM_ICON_H)/2, ITEM_ICON_W, ITEM_ICON_H, COL_MENUCONTENTDARK_PLUS_0 ); // 25 foot height
		
			// paint item icon
			if( ((FULL_HEIGHT - (hheight + fheight + 2*sp_height)) >= (ITEM_ICON_H) ) /*&& (icon_w >= 100 || icon_h >= 100)*/ )
				frameBuffer->paintIcon(itemIcon.c_str(), x + BORDER_LEFT + (dx/3)*2 + ((( dx - (dx/3)*2 - BORDER_RIGHT)/2) - (icon_w > ITEM_ICON_W? ITEM_ICON_W : icon_w)/2), ( frameBuffer->getScreenHeight(true) - (icon_h > ITEM_ICON_H? ITEM_ICON_H : icon_h))/2, 0, true, (icon_w > ITEM_ICON_W? ITEM_ICON_W : icon_w), (icon_h > ITEM_ICON_H? ITEM_ICON_H : icon_h));  //25:foot height
		}
		
		// help bar
		int icon_foot_w, icon_foot_h;
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_foot_w, &icon_foot_h);
		int fheight = std::max(icon_foot_h, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight()) + 6;
		int fposy = HEIGHT - fheight;
		
		// refresh
		frameBuffer->paintBoxRel(x, fposy, dx, fheight, COL_MENUFOOT_PLUS_0, RADIUS_MID, (g_settings.rounded_corners == ONLY_TOP) ? 0x0 : CORNER_BOTTOM);
			
		// paint help icon
		int icon_w = 0;
		int icon_h = 0;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_INFO, &icon_w, &icon_h);
			
		frameBuffer->paintIcon(NEUTRINO_ICON_INFO, x + BORDER_LEFT - 2, fposy + (fheight - icon_h)/2);
			
		// help text locale
		//const char * help_text = getHelpText();	// use helptext locale
		const char * help_text = getName();
			
		g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(x + BORDER_LEFT + icon_w + 5, fposy + (fheight - g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight(), dx - (x + (offx == 0? 0 : offx) + BORDER_LEFT + icon_w + 5 - x), help_text, COL_MENUFOOT, 0, true); // UTF-8
	
		// menutitle on VFD
		char str[256];

		if (option_text != NULL) 
		{
			snprintf(str, 255, "%s %s", l_text, option_text);

			CVFD::getInstance()->showMenuText(0, str, -1, true);
		} 
		else
		{
			CVFD::getInstance()->showMenuText(0, l_text, -1, true);
		}
	}
	
	// paint item
	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, (dx/3)*2, height, bgcolor);

	// paint icon/direkt-key
	int icon_w = 0;
	int icon_h = 0;
		
	if (!iconName.empty())
	{
		frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
		
		frameBuffer->paintIcon(iconName, x + BORDER_LEFT + ICON_OFFSET, y + ((height - icon_h)/2) );
	}
	else if (CRCInput::isNumeric(directKey))
	{
		// define icon name depends of numeric value
		char i_name[6]; 							// X +'\0'
		snprintf(i_name, 6, "%d", CRCInput::getNumericValue(directKey));
		i_name[5] = '\0'; 							// even if snprintf truncated the string, ensure termination
		iconName = i_name;
		
		if (!iconName.empty())
		{
			frameBuffer->getIconSize(iconName.c_str(), &icon_w, &icon_h);
			
			frameBuffer->paintIcon(iconName, x + BORDER_LEFT + 5, y + ((height - icon_h)/2) );
		}
		else
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + BORDER_LEFT + 5, y + height, height, CRCInput::getKeyName(directKey), color, height);
	}
	
	//local-text
	stringstartposX = x + BORDER_LEFT + ICON_OFFSET + (icon_w? icon_w + LOCAL_OFFSET : 0);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), (dx/3)*2 - (stringstartposX - x), l_text, color, 0, true); // UTF-8
	
	//option-text
	if (option_text != NULL)
	{
		int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(option_text, true);
		int stringstartposOption = std::max(stringstartposX + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true) + BORDER_LEFT + BORDER_LEFT/2, x + (dx/3)*2 - stringwidth - BORDER_RIGHT ); //+ offx
		
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposOption, y + (height - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight())/2 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight(), (dx/3)*2 - (stringstartposOption- x),  option_text, color, 0, true);
	}

	return y + height;
}

// CMenuForwarderItemMenuIconNonLocalized
const char * CMenuForwarderItemMenuIconNonLocalized::getName(void)
{
	return the_text.c_str();
}

CMenuForwarderItemMenuIconNonLocalized::CMenuForwarderItemMenuIconNonLocalized(const char * const Text, const bool Active, const char * const Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const ItemIcon) : CMenuForwarderItemMenuIcon(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName, ItemIcon)
{
	the_text = Text;
}

CMenuForwarderItemMenuIconNonLocalized::CMenuForwarderItemMenuIconNonLocalized(const char * const Text, const bool Active, const std::string &Option, CMenuTarget* Target, const char * const ActionKey, neutrino_msg_t DirectKey, const char * const IconName, const char * const ItemIcon) : CMenuForwarderItemMenuIcon(NONEXISTANT_LOCALE, Active, Option, Target, ActionKey, DirectKey, IconName, ItemIcon)
{
	the_text = Text;
}

// CMenuSeparatorMainMenu
CMenuSeparatorItemMenuIcon::CMenuSeparatorItemMenuIcon(const int Type, const neutrino_locale_t Text)
{
	directKey = CRCInput::RC_nokey;
	iconName = "";
	type     = Type;
	text     = Text;
}

int CMenuSeparatorItemMenuIcon::getHeight(void) const
{
	if ((type & LINE))
	{
		return (text == NONEXISTANT_LOCALE) ? BORDER_LEFT : g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	}
	else
	{
		int iconName_w, iconName_h;
		CFrameBuffer::getInstance()->getIconSize(NEUTRINO_ICON_BUTTON_RED, &iconName_w, &iconName_h);
		
		return std::max(iconName_h, (text == NONEXISTANT_LOCALE) ? BORDER_LEFT : g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight()) + 3;
	}
	
}

int CMenuSeparatorItemMenuIcon::getWidth(void) const
{
	return 0;
}

const char * CMenuSeparatorItemMenuIcon::getString(void)
{
	return g_Locale->getText(text);
}

int CMenuSeparatorItemMenuIcon::paint(bool /*selected*/)
{
	int height;
	CFrameBuffer * frameBuffer = CFrameBuffer::getInstance();
	height = getHeight();

	frameBuffer->paintBoxRel(x + BORDER_LEFT, y, (dx/3)*2, height, COL_MENUCONTENT_PLUS_0 );

	if ((type & LINE))
	{
		frameBuffer->paintHLineRel(x + BORDER_LEFT, (dx/3)*2, y + (height >> 1), COL_MENUCONTENTDARK_PLUS_0);
		frameBuffer->paintHLineRel(x + BORDER_LEFT, (dx/3)*2, y + (height >> 1) + 1, COL_MENUCONTENTDARK_PLUS_0);
	}

	if ((type & STRING))
	{

		if (text != NONEXISTANT_LOCALE)
		{
			int stringstartposX;

			const char * l_text = getString();
			int stringwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(l_text, true); // UTF-8

			/* if no alignment is specified, align centered */
			if (type & ALIGN_LEFT)
				stringstartposX = x + (BORDER_LEFT + 10) ;
			else if (type & ALIGN_RIGHT)
				stringstartposX = x + (dx/3)*2 - stringwidth - (BORDER_RIGHT + 10);
			else /* ALIGN_CENTER */
				stringstartposX = x + ( ((dx/3)*2) >> 1) - (stringwidth >> 1);

			frameBuffer->paintBoxRel(stringstartposX - 5, y, stringwidth + 10, height, COL_MENUCONTENT_PLUS_0);

			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(stringstartposX, y + height, (dx/3)*2 - (stringstartposX- x) , l_text, COL_MENUCONTENTINACTIVE, 0, true); // UTF-8
		}
	}

	return y + height;
}

int CLockedMenuForwarderItemMenuIcon::exec(CMenuTarget *parent)
{
	Parent = parent;
	
	if( (g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER) || AlwaysAsk )
	{
		if (!check())
		{
			Parent = NULL;
			return menu_return::RETURN_REPAINT;
		}
	}

	Parent = NULL;
	
	return CMenuForwarderItemMenuIcon::exec(parent);
}

