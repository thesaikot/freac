 /* BonkEnc version 0.9
  * Copyright (C) 2001-2003 Robert Kausch <robert.kausch@gmx.net>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#ifndef _H_CDDB_MULTIMATCH_
#define _H_CDDB_MULTIMATCH_

#include <smooth.h>
#include <main.h>

using namespace smooth;

class cddbMultiMatchDlg : public Application
{
	private:
		GroupBox	*group_match;
		Text		*text_match;
		ComboBox	*combo_match;

		Divider		*divbar;

		Window		*mainWnd;
		Titlebar	*mainWnd_titlebar;

		Button		*btn_cancel;
		Button		*btn_ok;

		Void		 OK();
	public:
				 cddbMultiMatchDlg(bonkEncConfig *, Bool);
				~cddbMultiMatchDlg();

		Int		 ShowDialog();

		Int		 AddEntry(String, String);
};

#endif