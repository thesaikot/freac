 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2009 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <startgui.h>
#include <resources.h>

#ifdef __WIN32__
#	include <dbt.h>
#endif

#include <dllinterfaces.h>
#include <joblist.h>
#include <utilities.h>

#include <jobs/job_addfiles.h>
#include <jobs/job_checkforupdates.h>

#include <dialogs/config/config.h>
#include <dialogs/config/configcomponent.h>

#include <dialogs/adddirectory.h>
#include <dialogs/addpattern.h>

#include <cddb/cddb.h>
#include <cddb/cddbremote.h>
#include <cddb/cddbbatch.h>
#include <cddb/cddbcache.h>

#include <dialogs/cddb/query.h>
#include <dialogs/cddb/submit.h>
#include <dialogs/cddb/manage.h>
#include <dialogs/cddb/managequeries.h>
#include <dialogs/cddb/managesubmits.h>

Int StartGUI(const Array<String> &args)
{
	BonkEnc::BonkEncGUI	*app = BonkEnc::BonkEncGUI::Get();

	app->Loop();

	BonkEnc::BonkEncGUI::Free();

	return 0;
}

BonkEnc::BonkEncGUI *BonkEnc::BonkEncGUI::Get()
{
	if (instance == NIL) new BonkEncGUI();

	return (BonkEncGUI *) instance;
}

Void BonkEnc::BonkEncGUI::Free()
{
	if (instance != NIL) delete (BonkEncGUI *) instance;
}

BonkEnc::BonkEncGUI::BonkEncGUI()
{
	BoCA::Config	*config = BoCA::Config::Get();

	currentConfig->enable_console = false;

	dontUpdateInfo = False;

	clicked_drive = -1;
	clicked_encoder = -1;

	String	 language = config->language;

	if (language == NIL) language = GetSystemLanguage();

	i18n->ActivateLanguage(language);

	config->language = language;

	Rect	 workArea = MultiMonitor::GetVirtualScreenMetrics();

	if (config->wndPos.x + config->wndSize.cx > workArea.right + 2 ||
	    config->wndPos.y + config->wndSize.cy > workArea.bottom + 2)
	{
		config->wndPos.x = (Int) Math::Min(workArea.right - 10 - config->wndSize.cx, config->wndPos.x);
		config->wndPos.y = (Int) Math::Min(workArea.bottom - 10 - config->wndSize.cy, config->wndPos.y);
	}

	if (config->wndPos.x < workArea.left - 2 ||
	    config->wndPos.y < workArea.top - 2)
	{
		config->wndPos.x = (Int) Math::Max(workArea.left + 10, config->wndPos.x);
		config->wndPos.y = (Int) Math::Max(workArea.top + 10, config->wndPos.y);

		config->wndSize.cx = (Int) Math::Min(workArea.right - 20, config->wndSize.cx);
		config->wndSize.cy = (Int) Math::Min(workArea.bottom - 20, config->wndSize.cy);
	}

	mainWnd			= new GUI::Window(String(BonkEnc::appName).Append(" ").Append(BonkEnc::version), config->wndPos, config->wndSize);
	mainWnd->SetRightToLeft(i18n->IsActiveLanguageRightToLeft());

	mainWnd_titlebar	= new Titlebar();
	mainWnd_menubar		= new Menubar();
	mainWnd_iconbar		= new Menubar();
	mainWnd_statusbar	= new Statusbar(String(BonkEnc::appName).Append(" ").Append(BonkEnc::version).Append(" - Copyright (C) 2001-2009 Robert Kausch"));
	menu_file		= new PopupMenu();
	menu_options		= new PopupMenu();
	menu_addsubmenu		= new PopupMenu();
	menu_encode		= new PopupMenu();
	menu_drives		= new PopupMenu();
	menu_files		= new PopupMenu();
	menu_seldrive		= new PopupMenu();
	menu_database		= new PopupMenu();
	menu_database_query	= new PopupMenu();
	menu_help		= new PopupMenu();
	menu_encoders		= new PopupMenu();
	menu_encoder_options	= new PopupMenu();

	hyperlink		= new Hyperlink("www.bonkenc.org", NIL, "http://www.bonkenc.org/", Point(91, -22));
	hyperlink->SetOrientation(OR_UPPERRIGHT);
	hyperlink->SetIndependent(True);

	tabs_main		= new TabWidget(Point(6, 7), Size(700, 500));

	tab_layer_joblist	= new LayerJoblist();
	tab_layer_joblist->onRequestSkipTrack.Connect(&Encoder::SkipTrack, encoder);

	tabs_main->Add(tab_layer_joblist);

	InitExtensionComponents();

	for (Int i = 0; i < extensionComponents.Length(); i++)
	{
		Layer	*mainTabLayer	= extensionComponents.GetNth(i)->getMainTabLayer.Emit();
		Layer	*statusBarLayer = extensionComponents.GetNth(i)->getStatusBarLayer.Emit();

		if (mainTabLayer   != NIL) tabs_main->Add(mainTabLayer);
		if (statusBarLayer != NIL) mainWnd_statusbar->Add(statusBarLayer);
	}

	tab_layer_threads	= new LayerThreads();

/* ToDo: Add threads layer once it's ready.
 */
	tabs_main->Add(tab_layer_threads);

	joblist			= tab_layer_joblist->GetJoblist();

	Add(mainWnd);

	SetLanguage();

	mainWnd->Add(hyperlink);
	mainWnd->Add(mainWnd_titlebar);
	mainWnd->Add(mainWnd_statusbar);
	mainWnd->Add(mainWnd_menubar);
	mainWnd->Add(mainWnd_iconbar);
	mainWnd->Add(tabs_main);

	mainWnd->SetIcon(ImageLoader::Load("BonkEnc.pci:0"));

	mainWnd->onChangePosition.Connect(&BonkEncGUI::OnChangePosition, this);
	mainWnd->onChangeSize.Connect(&BonkEncGUI::OnChangeSize, this);

	mainWnd->onEvent.Connect(&BonkEncGUI::MessageProc, this);

	if (BoCA::Config::Get()->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, Config::SettingsShowTipsDefault)) mainWnd->onShow.Connect(&BonkEncGUI::ShowTipOfTheDay, this);

	mainWnd->doClose.Connect(&BonkEncGUI::ExitProc, this);
	mainWnd->SetMinimumSize(Size(530, 340 + (config->showTitleInfo ? 68 : 0)));

	if (config->maximized) mainWnd->Maximize();

	encoder->onStartEncoding.Connect(&LayerJoblist::OnEncoderStartEncoding, tab_layer_joblist);
	encoder->onFinishEncoding.Connect(&LayerJoblist::OnEncoderFinishEncoding, tab_layer_joblist);

	encoder->onEncodeTrack.Connect(&LayerJoblist::OnEncoderEncodeTrack, tab_layer_joblist);

	encoder->onTrackProgress.Connect(&LayerJoblist::OnEncoderTrackProgress, tab_layer_joblist);
	encoder->onTotalProgress.Connect(&LayerJoblist::OnEncoderTotalProgress, tab_layer_joblist);

	if (BoCA::Config::Get()->checkUpdatesAtStartup) (new JobCheckForUpdates(True))->Schedule();
}

BonkEnc::BonkEncGUI::~BonkEncGUI()
{
	FreeExtensionComponents();

	DeleteObject(mainWnd_menubar);
	DeleteObject(mainWnd_iconbar);
	DeleteObject(mainWnd_titlebar);
	DeleteObject(mainWnd_statusbar);
	DeleteObject(mainWnd);

	DeleteObject(tabs_main);

	DeleteObject(tab_layer_joblist);
	DeleteObject(tab_layer_threads);

	DeleteObject(hyperlink);

	DeleteObject(menu_file);
	DeleteObject(menu_options);
	DeleteObject(menu_addsubmenu);
	DeleteObject(menu_encode);
	DeleteObject(menu_drives);
	DeleteObject(menu_files);
	DeleteObject(menu_seldrive);
	DeleteObject(menu_encoders);
	DeleteObject(menu_encoder_options);
	DeleteObject(menu_database);
	DeleteObject(menu_database_query);
	DeleteObject(menu_help);
}

Void BonkEnc::BonkEncGUI::InitExtensionComponents()
{
	Registry	&boca = Registry::Get();

	for (Int i = 0; i < boca.GetNumberOfComponents(); i++)
	{
		if (boca.GetComponentType(i) != BoCA::COMPONENT_TYPE_EXTENSION) continue;

		ExtensionComponent	*component = (ExtensionComponent *) boca.CreateComponentByID(boca.GetComponentID(i));

		if (component != NIL) extensionComponents.Add(component);
	}
}

Void BonkEnc::BonkEncGUI::FreeExtensionComponents()
{
	Registry	&boca = Registry::Get();

	for (Int i = 0; i < extensionComponents.Length(); i++)
	{
		boca.DeleteComponent(extensionComponents.GetNth(i));
	}

	extensionComponents.RemoveAll();
}

Bool BonkEnc::BonkEncGUI::ExitProc()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (encoder->IsEncoding())
	{
		if (IDNO == QuickMessage(i18n->TranslateString("The encoding thread is still running! Do you really want to quit?"), i18n->TranslateString("Currently encoding"), MB_YESNO, IDI_QUESTION)) return False;

		encoder->Stop();
	}

	/* Stop playback if playing a track
	 */
	tab_layer_joblist->StopPlayback();

	Rect	 wndRect = mainWnd->GetRestoredWindowRect();

	config->wndPos = Point(wndRect.left, wndRect.top);
	config->wndSize = Size(wndRect.right - wndRect.left, wndRect.bottom - wndRect.top);
	config->maximized = mainWnd->IsMaximized();

	config->SaveSettings();

	return True;
}

Void BonkEnc::BonkEncGUI::MessageProc(Int message, Int wParam, Int lParam)
{
	switch (message)
	{
#ifdef __WIN32__
		case WM_DEVICECHANGE:
			if (wParam == DBT_DEVICEARRIVAL && currentConfig->enable_cdrip && BoCA::Config::Get()->GetIntValue("CDRip", "AutoReadContents", True))
			{
				if (((DEV_BROADCAST_HDR *) lParam)->dbch_devicetype != DBT_DEVTYP_VOLUME || !(((DEV_BROADCAST_VOLUME *) lParam)->dbcv_flags & DBTF_MEDIA)) break;

				/* Get drive letter from message.
				 */
				String	 driveLetter = String(" :");

				for (Int drive = 0; drive < 26; drive++)
				{
					if (((DEV_BROADCAST_VOLUME *) lParam)->dbcv_unitmask >> drive & 1)
					{
						driveLetter[0] = drive + 'A';

						break;
					}
				}

				if (driveLetter[0] == ' ') break;

				Int	 trackLength = 0;

				/* Read length of first track using MCI.
				 */
				MCI_OPEN_PARMSA	 openParms;

				openParms.lpstrDeviceType  = (LPSTR) MCI_DEVTYPE_CD_AUDIO;
				openParms.lpstrElementName = driveLetter;

				MCIERROR	 error = mciSendCommandA(NIL, MCI_OPEN, MCI_WAIT | MCI_OPEN_SHAREABLE | MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_OPEN_ELEMENT, (DWORD) &openParms);

				if (error == 0)
				{
					MCI_SET_PARMS		 setParms;

					setParms.dwTimeFormat	= MCI_FORMAT_MSF;

					mciSendCommandA(openParms.wDeviceID, MCI_SET, MCI_WAIT | MCI_SET_TIME_FORMAT, (DWORD) &setParms);

					MCI_STATUS_PARMS	 statusParms;

					statusParms.dwItem	= MCI_STATUS_LENGTH;
					statusParms.dwTrack	= 1;

					mciSendCommandA(openParms.wDeviceID, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM | MCI_TRACK, (DWORD) &statusParms);

					trackLength = MCI_MSF_MINUTE(statusParms.dwReturn) * 60 * 75 +
						      MCI_MSF_SECOND(statusParms.dwReturn) * 75	     +
						      MCI_MSF_FRAME (statusParms.dwReturn);

					MCI_GENERIC_PARMS	 closeParms;

					mciSendCommandA(openParms.wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD) &closeParms);
				}

				/* Look for the actual drive using
				 * the length of the first track.
				 */
				if (trackLength > 0)
				{
					Bool	 ok = False;
					Int	 drive = 0;

					for (drive = 0; drive < BoCA::Config::Get()->cdrip_numdrives; drive++)
					{
						ex_CR_SetActiveCDROM(drive);

						ex_CR_ReadToc();

						TOCENTRY	 entry = ex_CR_GetTocEntry(0);
						TOCENTRY	 nextentry = ex_CR_GetTocEntry(1);
						Int		 length = nextentry.dwStartSector - entry.dwStartSector;

						if (!(entry.btFlag & CDROMDATAFLAG) && length == trackLength) { ok = True; break; }
					}

					if (ok)
					{
						BoCA::Config::Get()->cdrip_activedrive = drive;
						BoCA::Config::Get()->cdrip_autoRead_active = True;

						ReadCD();

						BoCA::Config::Get()->cdrip_autoRead_active = False;
					}
				}
			}

			break;
#endif
	}
}

Void BonkEnc::BonkEncGUI::OnChangePosition(const Point &nPos)
{
	BoCA::Config	*config = BoCA::Config::Get();

	config->wndPos = mainWnd->GetPosition();
}

Void BonkEnc::BonkEncGUI::OnChangeSize(const Size &nSize)
{
	BoCA::Config	*config = BoCA::Config::Get();

	config->wndSize = mainWnd->GetSize();

	mainWnd->SetStatusText(String(BonkEnc::appName).Append(" ").Append(BonkEnc::version).Append(" - Copyright (C) 2001-2009 Robert Kausch"));

	Rect	 clientRect = mainWnd->GetClientRect();
	Size	 clientSize = Size(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

	tabs_main->SetSize(clientSize - Size(12, 15));
}

Void BonkEnc::BonkEncGUI::Close()
{
	mainWnd->Close();
}

Void BonkEnc::BonkEncGUI::About()
{
#ifdef __WIN32__
	QuickMessage(String(BonkEnc::appName).Append(" ").Append(BonkEnc::version).Append("\nCopyright (C) 2001-2009 Robert Kausch\n\n").Append(String(i18n->TranslateString("Translated by %1.")).Replace("%1", i18n->GetActiveLanguageAuthor())).Append("\n\n").Append(i18n->TranslateString("This program is being distributed under the terms\nof the GNU General Public License (GPL).")), String(i18n->TranslateString("About %1")).Replace("%1", BonkEnc::appName), MB_OK, MAKEINTRESOURCE(IDI_ICON));
#endif
}

Void BonkEnc::BonkEncGUI::ConfigureEncoder()
{
	if (!currentConfig->CanChangeConfig()) return;

	BoCA::Config	*config = BoCA::Config::Get();
	Registry	&boca = Registry::Get();
	Component	*component = boca.CreateComponentByID(config->encoderID);
	ConfigLayer	*layer = component->GetConfigurationLayer();

	if (layer != NIL)
	{
		ConfigComponentDialog	*dlg = new ConfigComponentDialog(layer);

		dlg->SetParentWindow(mainWnd);
		dlg->ShowDialog();

		DeleteObject(dlg);
	}
	else
	{
		QuickMessage(String(i18n->TranslateString("No configuration dialog available for:\n\n%1")).Replace("%1", component->GetName()), BonkEnc::i18n->TranslateString("Error"), MB_OK, IDI_INFORMATION);
	}

	component->FreeConfigurationLayer();

	boca.DeleteComponent(component);
}

Void BonkEnc::BonkEncGUI::ConfigureSettings()
{
	if (!currentConfig->CanChangeConfig()) return;

	BoCA::Config	*config = BoCA::Config::Get();
	ConfigDialog	*dlg = new ConfigDialog();

	dlg->ShowDialog();

	DeleteObject(dlg);

	if (config->languageChanged)
	{
		SetLanguage();

		config->languageChanged = false;
	}

	BoCA::Settings::Get()->onChangeConfigurationSettings.Emit();

	tab_layer_joblist->UpdateEncoderText();
	tab_layer_joblist->UpdateOutputDir();

	CheckBox::internalCheckValues.Emit();
	ToggleUseInputDirectory();

	config->SaveSettings();
}


Void BonkEnc::BonkEncGUI::ReadCD()
{
	if (!joblist->CanModifyJobList()) return;

#ifdef __WIN32__
	ex_CR_SetActiveCDROM(BoCA::Config::Get()->cdrip_activedrive);

	ex_CR_ReadToc();

	Array<String>	 files;

	Int	 numTocEntries = ex_CR_GetNumTocEntries();

	for (Int i = 0; i < numTocEntries; i++)
	{
		TOCENTRY entry = ex_CR_GetTocEntry(i);

		if (!(entry.btFlag & CDROMDATAFLAG) && entry.btTrackNumber == i + 1)
		{
			/* Add CD track to joblist using a cdda:// URI
			 */
			files.Add(String("cdda://")
				 .Append(String::FromInt(BoCA::Config::Get()->cdrip_activedrive))
				 .Append("/")
				 .Append(String::FromInt(entry.btTrackNumber))
			);
		}
	}

	Job	*job = new JobAddFiles(files);

	job->Schedule();

	if (BoCA::Config::Get()->enable_auto_cddb) job->onFinish.Connect(&BonkEncGUI::QueryCDDB, this);
	if (BoCA::Config::Get()->GetIntValue("CDRip", "AutoRip", False)) job->onFinish.Connect(&BonkEncGUI::Encode, this);
#endif
}

Void BonkEnc::BonkEncGUI::ReadSpecificCD()
{
	BoCA::Config::Get()->cdrip_activedrive = clicked_drive;

	clicked_drive = -1;

	ReadCD();
}

Void BonkEnc::BonkEncGUI::QueryCDDB()
{
	BoCA::Config	*config = BoCA::Config::Get();

	/* ToDo: Check that this message is not displayed
	 *	 when QueryCDDB is called from ReadCD because
	 *	 of auto queries enabled.
	 */
	if (!config->enable_local_cddb && !config->enable_remote_cddb)
	{
		Utilities::ErrorMessage("CDDB support is disabled! Please enable local or\nremote CDDB support in the configuration dialog.");

		return;
	}

	Array<Int>	 discIDs;
	Array<String>	 queryStrings;

	for (Int i = 0; i < joblist->GetNOfTracks(); i++)
	{
		const Track	&track = joblist->GetNthTrack(i);
		const Info	&info = track.GetInfo();

		if (info.mcdi.Size() > 0)
		{
			Int	 discID	     = CDDB::DiscIDFromMCDI(info.mcdi);
			String	 queryString = CDDB::QueryStringFromMCDI(info.mcdi);

			discIDs.Add(discID, queryString.ComputeCRC32());
			queryStrings.Add(queryString, queryString.ComputeCRC32());
		}
		else if (info.offsets != NIL)
		{
			Int	 discID	     = CDDB::DiscIDFromOffsets(info.offsets);
			String	 queryString = CDDB::QueryStringFromOffsets(info.offsets);

			discIDs.Add(discID, queryString.ComputeCRC32());
			queryStrings.Add(queryString, queryString.ComputeCRC32());
		}
	}

	for (Int j = 0; j < queryStrings.Length(); j++)
	{
		Int	 discID	     = discIDs.GetNth(j);
		String	 queryString = queryStrings.GetNth(j);
		CDDBInfo cdInfo;

		if (config->enable_cddb_cache) cdInfo = CDDBCache::Get()->GetCacheEntry(discID);

		if (cdInfo == NIL)
		{
			cddbQueryDlg	*dlg = new cddbQueryDlg();

			dlg->SetQueryString(queryString);

			cdInfo = dlg->QueryCDDB(True);

			DeleteObject(dlg);

			if (cdInfo != NIL) CDDBCache::Get()->AddCacheEntry(cdInfo);
		}

		if (cdInfo != NIL)
		{
			for (Int k = 0; k < joblist->GetNOfTracks(); k++)
			{
				Track	 track = joblist->GetNthTrack(k);
				Info	&info = track.GetInfo();

				if ((info.mcdi.Size() > 0 && discID == CDDB::DiscIDFromMCDI(info.mcdi))	      ||
				    (info.offsets != NIL  && discID == CDDB::DiscIDFromOffsets(info.offsets)))
				{
					Int	 trackNumber = -1;

					if (track.isCDTrack) trackNumber = track.cdTrack;
					else		     trackNumber = info.track;

					if (trackNumber == -1) continue;

					info.artist	= (cdInfo.dArtist == "Various" ? cdInfo.trackArtists.GetNth(trackNumber - 1) : cdInfo.dArtist);
					info.title	= cdInfo.trackTitles.GetNth(trackNumber - 1);
					info.album	= cdInfo.dTitle;
					info.genre	= cdInfo.dGenre;
					info.year	= cdInfo.dYear;
					info.track	= trackNumber;

					track.SetOriginalInfo(info);

					track.outfile	= NIL;

					BoCA::JobList::Get()->onComponentModifyTrack.Emit(track);
				}
			}
		}
	}

	joblist->Paint(SP_PAINT);

	if (joblist->GetSelectedTrack() != NIL) joblist->OnSelectEntry();
}

Void BonkEnc::BonkEncGUI::QueryCDDBLater()
{
	Array<Int>	 drives;

	for (Int i = 0; i < joblist->GetNOfTracks(); i++)
	{
		const Track	&track = joblist->GetNthTrack(i);

		if (track.isCDTrack) drives.Add(track.drive, track.drive);
	}

	if (drives.Length() > 0)
	{
		CDDBBatch	*queries = new CDDBBatch();

		for (Int j = 0; j < drives.Length(); j++)
		{
			Int		 drive = drives.GetNth(j);
			CDDBRemote	 cddb;

			cddb.SetActiveDrive(drive);

			queries->AddQuery(cddb.GetCDDBQueryString());
		}

		delete queries;
	}
}

Void BonkEnc::BonkEncGUI::SubmitCDDBData()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (!config->enable_local_cddb && !config->enable_remote_cddb)
	{
		Utilities::ErrorMessage("CDDB support is disabled! Please enable local or\nremote CDDB support in the configuration dialog.");

		return;
	}

	cddbSubmitDlg	*dlg = new cddbSubmitDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Void BonkEnc::BonkEncGUI::ManageCDDBData()
{
	cddbManageDlg	*dlg = new cddbManageDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Void BonkEnc::BonkEncGUI::ManageCDDBBatchData()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (!config->enable_remote_cddb)
	{
		Utilities::ErrorMessage("Remote CDDB support is disabled! Please enable\nremote CDDB support in the configuration dialog.");

		return;
	}

	cddbManageSubmitsDlg	*dlg = new cddbManageSubmitsDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Void BonkEnc::BonkEncGUI::ManageCDDBBatchQueries()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (!config->enable_remote_cddb)
	{
		Utilities::ErrorMessage("Remote CDDB support is disabled! Please enable\nremote CDDB support in the configuration dialog.");

		return;
	}

	cddbManageQueriesDlg	*dlg = new cddbManageQueriesDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Bool BonkEnc::BonkEncGUI::SetLanguage()
{
	Bool	 prevRTL = i18n->IsActiveLanguageRightToLeft();

	i18n->ActivateLanguage(BoCA::Config::Get()->language);

	MessageDlg::SetDefaultRightToLeft(i18n->IsActiveLanguageRightToLeft());

	if (i18n->IsActiveLanguageRightToLeft() != prevRTL)
	{
		mainWnd->SetUpdateRect(Rect(Point(0, 0), mainWnd->GetSize()));
		mainWnd->SetRightToLeft(i18n->IsActiveLanguageRightToLeft());
		mainWnd->Paint(SP_PAINT);
	}

	BoCA::Settings::Get()->onChangeLanguageSettings.Emit();

	FillMenus();

	hyperlink->Hide();
	hyperlink->Show();

	return true;
}

Void BonkEnc::BonkEncGUI::FillMenus()
{
	mainWnd_menubar->Hide();
	mainWnd_iconbar->Hide();

	menu_file->RemoveAllEntries();
	menu_options->RemoveAllEntries();
	menu_drives->RemoveAllEntries();
	menu_files->RemoveAllEntries();
	menu_seldrive->RemoveAllEntries();
	menu_addsubmenu->RemoveAllEntries();
	menu_encode->RemoveAllEntries();
	menu_encoders->RemoveAllEntries();
	menu_encoder_options->RemoveAllEntries();
	menu_database->RemoveAllEntries();
	menu_database_query->RemoveAllEntries();
	menu_help->RemoveAllEntries();

	MenuEntry	*entry = NIL;

	menu_file->AddEntry(i18n->TranslateString("Add"), ImageLoader::Load("BonkEnc.pci:21"), menu_addsubmenu);
	entry = menu_file->AddEntry(i18n->TranslateString("Remove"), ImageLoader::Load("BonkEnc.pci:24"));
	entry->onAction.Connect(&JobList::RemoveSelectedTrack, joblist);
	entry->SetShortcut(SC_CTRL, 'R', mainWnd);
	menu_file->AddEntry();
	menu_file->AddEntry(i18n->TranslateString("Load joblist..."))->onAction.Connect(&JobList::LoadList, joblist);
	menu_file->AddEntry(i18n->TranslateString("Save joblist..."))->onAction.Connect(&JobList::SaveList, joblist);
	menu_file->AddEntry();
	entry = menu_file->AddEntry(i18n->TranslateString("Clear joblist"), ImageLoader::Load("BonkEnc.pci:25"));
	entry->onAction.Connect(&JobList::StartJobRemoveAllTracks, joblist);
	entry->SetShortcut(SC_CTRL | SC_SHIFT, 'R', mainWnd);
	menu_file->AddEntry();
	entry = menu_file->AddEntry(i18n->TranslateString("Exit"), ImageLoader::Load("BonkEnc.pci:36"));
	entry->onAction.Connect(&BonkEncGUI::Close, this);
	entry->SetShortcut(SC_ALT, VK_F4, mainWnd);

	entry = menu_options->AddEntry(i18n->TranslateString("General settings..."), ImageLoader::Load("BonkEnc.pci:28"));
	entry->onAction.Connect(&BonkEncGUI::ConfigureSettings, this);
	entry->SetShortcut(SC_CTRL | SC_SHIFT, 'C', mainWnd);
	entry = menu_options->AddEntry(i18n->TranslateString("Configure selected encoder..."), ImageLoader::Load("BonkEnc.pci:29"));
	entry->onAction.Connect(&BonkEncGUI::ConfigureEncoder, this);
	entry->SetShortcut(SC_CTRL | SC_SHIFT, 'E', mainWnd);

	if (currentConfig->enable_cdrip && BoCA::Config::Get()->cdrip_numdrives > 1)
	{
		for (Int j = 0; j < BoCA::Config::Get()->cdrip_numdrives; j++)
		{
			menu_seldrive->AddEntry(BoCA::Config::Get()->cdrip_drives.GetNth(j), NIL, NIL, NIL, &BoCA::Config::Get()->cdrip_activedrive, j);
		}

		menu_options->AddEntry();
		menu_options->AddEntry(i18n->TranslateString("Active CD-ROM drive"), ImageLoader::Load("BonkEnc.pci:30"), menu_seldrive);
	}

	entry = menu_addsubmenu->AddEntry(String(i18n->TranslateString("Audio file(s)")).Append("..."), ImageLoader::Load("BonkEnc.pci:22"));
	entry->onAction.Connect(&JobList::AddTrackByDialog, joblist);
	entry->SetShortcut(SC_CTRL, 'A', mainWnd);

	if (currentConfig->enable_cdrip && BoCA::Config::Get()->cdrip_numdrives >= 1)
	{
		for (Int j = 0; j < BoCA::Config::Get()->cdrip_numdrives; j++)
		{
			menu_drives->AddEntry(BoCA::Config::Get()->cdrip_drives.GetNth(j), NIL, NIL, NIL, &clicked_drive, j)->onAction.Connect(&BonkEncGUI::ReadSpecificCD, this);
		}

		entry = menu_addsubmenu->AddEntry(i18n->TranslateString("Audio CD contents"), ImageLoader::Load("BonkEnc.pci:23"));
		entry->onAction.Connect(&BonkEncGUI::ReadCD, this);
		entry->SetShortcut(SC_CTRL, 'D', mainWnd);
	}

	menu_files->AddEntry(String(i18n->TranslateString("By pattern")).Append("..."))->onAction.Connect(&BonkEncGUI::AddFilesByPattern, this);
	menu_files->AddEntry(String(i18n->TranslateString("From directory")).Append("..."))->onAction.Connect(&BonkEncGUI::AddFilesFromDirectory, this);

	menu_addsubmenu->AddEntry();
	menu_addsubmenu->AddEntry(i18n->TranslateString("Audio file(s)"), NIL, menu_files);

	if (currentConfig->enable_cdrip && BoCA::Config::Get()->cdrip_numdrives > 1)
	{
		menu_addsubmenu->AddEntry(i18n->TranslateString("Audio CD contents"), NIL, menu_drives);
	}

	entry = menu_encode->AddEntry(i18n->TranslateString("Start encoding"), ImageLoader::Load("BonkEnc.pci:31"));
	entry->onAction.Connect(&BonkEncGUI::Encode, this);
	entry->SetShortcut(SC_CTRL, 'E', mainWnd);
	menu_encode->AddEntry(i18n->TranslateString("Pause/resume encoding"), ImageLoader::Load("BonkEnc.pci:32"))->onAction.Connect(&BonkEncGUI::PauseResumeEncoding, this);
	menu_encode->AddEntry(i18n->TranslateString("Stop encoding"), ImageLoader::Load("BonkEnc.pci:33"))->onAction.Connect(&BonkEncGUI::StopEncoding, this);

	Registry	&boca = Registry::Get();

	for (Int i = 0; i < boca.GetNumberOfComponents(); i++)
	{
		if (boca.GetComponentType(i) != BoCA::COMPONENT_TYPE_ENCODER) continue;

		menu_encoders->AddEntry(boca.GetComponentName(i), NIL, NIL, NIL, &clicked_encoder, i)->onAction.Connect(&BonkEncGUI::EncodeSpecific, this);
	}

	if (Registry::Get().GetNumberOfComponentsOfType(COMPONENT_TYPE_ENCODER) > 0)
	{
		menu_encode->AddEntry();
		menu_encode->AddEntry(i18n->TranslateString("Start encoding"), NIL, menu_encoders);
	}

	menu_encoder_options->AddEntry(i18n->TranslateString("Encode to single file"), NIL, NIL, &BoCA::Config::Get()->encodeToSingleFile);

	menu_encoder_options->AddEntry();
	menu_encoder_options->AddEntry(i18n->TranslateString("Encode to input file directory if possible"), NIL, NIL, &BoCA::Config::Get()->writeToInputDir)->onAction.Connect(&BonkEncGUI::ToggleUseInputDirectory, this);
	allowOverwriteMenuEntry = menu_encoder_options->AddEntry(i18n->TranslateString("Allow overwriting input file"), NIL, NIL, &BoCA::Config::Get()->allowOverwrite);

	menu_encoder_options->AddEntry();
	menu_encoder_options->AddEntry(i18n->TranslateString("Delete original files after encoding"), NIL, NIL, &currentConfig->deleteAfterEncoding)->onAction.Connect(&BonkEncGUI::ConfirmDeleteAfterEncoding, this);

	menu_encoder_options->AddEntry();
	menu_encoder_options->AddEntry(i18n->TranslateString("Shutdown after encoding"), NIL, NIL, &currentConfig->shutdownAfterEncoding);

	menu_encode->AddEntry();
	menu_encode->AddEntry(i18n->TranslateString("Encoder options"), ImageLoader::Load("BonkEnc.pci:29"), menu_encoder_options);

	ToggleUseInputDirectory();

	entry = menu_database->AddEntry(i18n->TranslateString("Query CDDB database"), ImageLoader::Load("BonkEnc.pci:26"));
	entry->onAction.Connect(&BonkEncGUI::QueryCDDB, this);
	entry->SetShortcut(SC_CTRL, 'Q', mainWnd);
	entry = menu_database->AddEntry(i18n->TranslateString("Submit CDDB data..."), ImageLoader::Load("BonkEnc.pci:27"));
	entry->onAction.Connect(&BonkEncGUI::SubmitCDDBData, this);
	entry->SetShortcut(SC_CTRL, 'S', mainWnd);
	menu_database->AddEntry();
	menu_database->AddEntry(i18n->TranslateString("Query CDDB database later"))->onAction.Connect(&BonkEncGUI::QueryCDDBLater, this);
	menu_database->AddEntry();
	menu_database->AddEntry(i18n->TranslateString("Show queued CDDB entries..."))->onAction.Connect(&BonkEncGUI::ManageCDDBBatchData, this);
	menu_database->AddEntry(i18n->TranslateString("Show queued CDDB queries..."))->onAction.Connect(&BonkEncGUI::ManageCDDBBatchQueries, this);
	menu_database->AddEntry();
	menu_database->AddEntry(i18n->TranslateString("Enable CDDB cache"), NIL, NIL, &BoCA::Config::Get()->enable_cddb_cache);
	menu_database->AddEntry(i18n->TranslateString("Manage CDDB cache entries..."))->onAction.Connect(&BonkEncGUI::ManageCDDBData, this);
	menu_database->AddEntry();
	menu_database->AddEntry(i18n->TranslateString("Automatic CDDB queries"), NIL, NIL, &BoCA::Config::Get()->enable_auto_cddb);

	menu_database_query->AddEntry(i18n->TranslateString("Query CDDB database"), ImageLoader::Load("BonkEnc.pci:26"))->onAction.Connect(&BonkEncGUI::QueryCDDB, this);
	menu_database_query->AddEntry(i18n->TranslateString("Query CDDB database later"))->onAction.Connect(&BonkEncGUI::QueryCDDBLater, this);

	entry = menu_help->AddEntry(i18n->TranslateString("Help topics..."), ImageLoader::Load("BonkEnc.pci:34"));
	entry->onAction.Connect(&BonkEncGUI::ShowHelp, this);
	entry->SetShortcut(0, VK_F1, mainWnd);
	menu_help->AddEntry();
	entry = menu_help->AddEntry(String(i18n->TranslateString("Show Tip of the Day")).Append("..."));
	entry->onAction.Connect(&BonkEncGUI::ShowTipOfTheDay, this);
	entry->SetShortcut(0, VK_F10, mainWnd);

	if (currentConfig->enable_eUpdate)
	{
		menu_help->AddEntry();
		menu_help->AddEntry(String(i18n->TranslateString("Check for updates now")).Append("..."))->onAction.Connect(&BonkEncGUI::CheckForUpdates, this);
		menu_help->AddEntry(i18n->TranslateString("Check for updates at startup"), NIL, NIL, &BoCA::Config::Get()->checkUpdatesAtStartup);
	}

	menu_help->AddEntry();
	menu_help->AddEntry(String(i18n->TranslateString("About %1")).Replace("%1", BonkEnc::appName).Append("..."), ImageLoader::Load("BonkEnc.pci:35"))->onAction.Connect(&BonkEncGUI::About, this);

	mainWnd_menubar->RemoveAllEntries();

	mainWnd_menubar->AddEntry(i18n->TranslateString("File"), NIL, menu_file);

	if (currentConfig->enable_cdrip && BoCA::Config::Get()->cdrip_numdrives >= 1) mainWnd_menubar->AddEntry(i18n->TranslateString("Database"), NIL, menu_database);

	mainWnd_menubar->AddEntry(i18n->TranslateString("Options"), NIL, menu_options);
	mainWnd_menubar->AddEntry(i18n->TranslateString("Encode"), NIL, menu_encode);
	mainWnd_menubar->AddEntry()->SetOrientation(OR_RIGHT);
	mainWnd_menubar->AddEntry(i18n->TranslateString("Help"), NIL, menu_help)->SetOrientation(OR_RIGHT);

	mainWnd_iconbar->RemoveAllEntries();

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:1"), menu_files);
	entry->onAction.Connect(&JobList::AddTrackByDialog, joblist);
	entry->SetTooltipText(i18n->TranslateString("Add audio file(s) to the joblist"));

	if (currentConfig->enable_cdrip && BoCA::Config::Get()->cdrip_numdrives >= 1)
	{
		entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:2"), BoCA::Config::Get()->cdrip_numdrives > 1 ? menu_drives : NIL);
		entry->onAction.Connect(&BonkEncGUI::ReadCD, this);
		entry->SetTooltipText(i18n->TranslateString("Add audio CD contents to the joblist"));
	}

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:3"));
	entry->onAction.Connect(&JobList::RemoveSelectedTrack, joblist);
	entry->SetTooltipText(i18n->TranslateString("Remove the selected entry from the joblist"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:4"));
	entry->onAction.Connect(&JobList::StartJobRemoveAllTracks, joblist);
	entry->SetTooltipText(i18n->TranslateString("Clear the entire joblist"));

	if (currentConfig->enable_cdrip && BoCA::Config::Get()->cdrip_numdrives >= 1)
	{
		mainWnd_iconbar->AddEntry();

		entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:5"), menu_database_query);
		entry->onAction.Connect(&BonkEncGUI::QueryCDDB, this);
		entry->SetTooltipText(i18n->TranslateString("Query CDDB database"));

		entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:6"));
		entry->onAction.Connect(&BonkEncGUI::SubmitCDDBData, this);
		entry->SetTooltipText(i18n->TranslateString("Submit CDDB data..."));
	}

	mainWnd_iconbar->AddEntry();

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:7"));
	entry->onAction.Connect(&BonkEncGUI::ConfigureSettings, this);
	entry->SetTooltipText(i18n->TranslateString("Configure general settings"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:8"));
	entry->onAction.Connect(&BonkEncGUI::ConfigureEncoder, this);
	entry->SetTooltipText(i18n->TranslateString("Configure the selected audio encoder"));

	mainWnd_iconbar->AddEntry();

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:9"), Registry::Get().GetNumberOfComponentsOfType(COMPONENT_TYPE_ENCODER) > 0 ? menu_encoders : NIL);
	entry->onAction.Connect(&BonkEncGUI::Encode, this);
	entry->SetTooltipText(i18n->TranslateString("Start the encoding process"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:10"));
	entry->onAction.Connect(&BonkEncGUI::PauseResumeEncoding, this);
	entry->SetTooltipText(i18n->TranslateString("Pause/resume encoding"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load("BonkEnc.pci:11"));
	entry->onAction.Connect(&BonkEncGUI::StopEncoding, this);
	entry->SetTooltipText(i18n->TranslateString("Stop encoding"));

	mainWnd_menubar->Show();
	mainWnd_iconbar->Show();
}

Void BonkEnc::BonkEncGUI::EncodeSpecific()
{
	BoCA::Config	*config = BoCA::Config::Get();
	Registry	&boca = Registry::Get();

	config->encoderID = boca.GetComponentID(clicked_encoder);

	tab_layer_joblist->UpdateEncoderText();

	clicked_encoder = -1;

	Encode();
}

Void BonkEnc::BonkEncGUI::Encode()
{
	encoder->Encode(joblist);
}

Void BonkEnc::BonkEncGUI::PauseResumeEncoding()
{
	if (!encoder->IsEncoding()) return;

	if (encoder->IsPaused()) encoder->Resume();
	else			 encoder->Pause();
}

Void BonkEnc::BonkEncGUI::StopEncoding()
{
	if (encoder == NIL) return;

	encoder->Stop();
}

Void BonkEnc::BonkEncGUI::AddFilesByPattern()
{
	AddPatternDialog	*dialog = new AddPatternDialog();

	if (dialog->ShowDialog() == Success()) joblist->AddTracksByPattern(dialog->GetDirectory(), dialog->GetPattern());

	DeleteObject(dialog);
}

Void BonkEnc::BonkEncGUI::AddFilesFromDirectory()
{
	AddDirectoryDialog	*dialog = new AddDirectoryDialog();

	if (dialog->ShowDialog() == Success()) joblist->AddTrackByDragAndDrop(dialog->GetDirectory());

	DeleteObject(dialog);
}

Void BonkEnc::BonkEncGUI::ToggleUseInputDirectory()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (config->writeToInputDir) allowOverwriteMenuEntry->Activate();
	else			     allowOverwriteMenuEntry->Deactivate();
}

Void BonkEnc::BonkEncGUI::ToggleEncodeToSingleFile()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (config->encodeToSingleFile) config->enc_onTheFly = True;
}

Void BonkEnc::BonkEncGUI::ConfirmDeleteAfterEncoding()
{
	if (currentConfig->deleteAfterEncoding)
	{
		if (IDNO == QuickMessage(i18n->TranslateString("This option will remove the original files from your computer\nafter the encoding process!\n\nAre you sure you want to activate this function?"), i18n->TranslateString("Delete original files after encoding"), MB_YESNO, IDI_QUESTION)) currentConfig->deleteAfterEncoding = False;
	}
}

Void BonkEnc::BonkEncGUI::ShowHelp()
{
#ifdef __WIN32__
	ShellExecuteA(NIL, "open", String("file://").Append(GetApplicationDirectory()).Append("manual/").Append(i18n->TranslateString("index_en.html")), NIL, NIL, 0);
#endif
}

Void BonkEnc::BonkEncGUI::ShowTipOfTheDay()
{
	BoCA::Config	*config = BoCA::Config::Get();

	Bool		 showTips = config->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, Config::SettingsShowTipsDefault);
	TipOfTheDay	*dialog = new TipOfTheDay(&showTips);

	dialog->AddTip(String(i18n->TranslateString("BonkEnc is available in %1 languages. If your language is\nnot available, you can easily translate BonkEnc using the\n\'smooth Translator\' application.")).Replace("%1", String::FromInt(Math::Max(36, i18n->GetNOfLanguages()))));
	dialog->AddTip(String(i18n->TranslateString("BonkEnc comes with support for the LAME, Ogg Vorbis, FAAC,\nFLAC and Bonk encoders. An encoder for the VQF format is\navailable at the BonkEnc website: %1")).Replace("%1", "http://www.bonkenc.org/"));
	dialog->AddTip(i18n->TranslateString("BonkEnc can use Winamp 2 input plug-ins to support more file\nformats. Copy the in_*.dll files to the BonkEnc/plugins directory to\nenable BonkEnc to read these formats."));
	dialog->AddTip(i18n->TranslateString("With BonkEnc you can submit freedb CD database entries\ncontaining Unicode characters. So if you have any CDs with\nnon-Latin artist or title names, you can submit the correct\nfreedb entries with BonkEnc."));
	dialog->AddTip(i18n->TranslateString("To correct reading errors while ripping you can enable\nJitter correction in the CDRip tab of BonkEnc's configuration\ndialog. If that does not help, try using one of the Paranoia modes."));
	dialog->AddTip(String(i18n->TranslateString("Do you have any suggestions on how to improve BonkEnc?\nYou can submit any ideas through the Tracker on the BonkEnc\nSourceForge project page - %1\nor send an eMail to %2.")).Replace("%1", "http://sf.net/projects/bonkenc").Replace("%2", "suggestions@bonkenc.org"));
	dialog->AddTip(String(i18n->TranslateString("Do you like BonkEnc? BonkEnc is available for free, but you can\nhelp fund the development by donating to the BonkEnc project.\nYou can send money to %1 through PayPal.\nSee %2 for more details.")).Replace("%1", "donate@bonkenc.org").Replace("%2", "http://www.bonkenc.org/donating.php"));

	dialog->SetMode(TIP_ORDERED, config->GetIntValue(Config::CategorySettingsID, Config::SettingsNextTipID, Config::SettingsNextTipDefault), config->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, Config::SettingsShowTipsDefault));

	dialog->ShowDialog();

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, showTips);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsNextTipID, dialog->GetOffset());

	DeleteObject(dialog);
}

String BonkEnc::BonkEncGUI::GetSystemLanguage()
{
	String	 language = "internal";

	if (i18n->GetNOfLanguages() == 1) return language;

#ifdef __WIN32__

#ifndef SUBLANG_CROATIAN_CROATIA
#  define SUBLANG_CROATIAN_CROATIA 0x01
#endif

	switch (PRIMARYLANGID(GetUserDefaultLangID()))
	{
		default:
		case LANG_ARABIC:
			language = "bonkenc_ar.xml";
			break;
		case LANG_CATALAN:
			language = "bonkenc_ca.xml";
			break;
		case LANG_CHINESE:
			language = "bonkenc_zh_CN.xml";

			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_CHINESE_SIMPLIFIED) language = "bonkenc_zh_CN.xml";
			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_CHINESE_TRADITIONAL) language = "bonkenc_zh_TW.xml";
			break;
		case LANG_CZECH:
			language = "bonkenc_cz.xml";
			break;
		case LANG_DANISH:
			language = "bonkenc_dk.xml";
			break;
		case LANG_DUTCH:
			language = "bonkenc_nl.xml";
			break;
		case LANG_ENGLISH:
			language = "internal";
			break;
		case LANG_ESTONIAN:
			language = "bonkenc_ee.xml";
			break;
		case LANG_FINNISH:
			language = "bonkenc_fi.xml";
			break;
		case LANG_FRENCH:
			language = "bonkenc_fr.xml";
			break;
		case LANG_GALICIAN:
			language = "bonkenc_gl.xml";
			break;
		case LANG_GERMAN:
			language = "bonkenc_de.xml";
			break;
		case LANG_GREEK:
			language = "bonkenc_gr.xml";
			break;
		case LANG_HEBREW:
			language = "bonkenc_he.xml";
			break;
		case LANG_HUNGARIAN:
			language = "bonkenc_hu.xml";
			break;
		case LANG_ITALIAN:
			language = "bonkenc_it.xml";
			break;
		case LANG_JAPANESE:
			language = "bonkenc_ja.xml";
			break;
		case LANG_KOREAN:
			language = "bonkenc_ko.xml";
			break;
		case LANG_LITHUANIAN:
			language = "bonkenc_lt.xml";
			break;
		case LANG_NORWEGIAN:
			language = "bonkenc_no.xml";
			break;
		case LANG_POLISH:
			language = "bonkenc_pl.xml";
			break;
		case LANG_PORTUGUESE:
			language = "bonkenc_pt.xml";

			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_PORTUGUESE) language = "bonkenc_pt.xml";
			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_PORTUGUESE_BRAZILIAN) language = "bonkenc_pt_BR.xml";
			break;
		case LANG_ROMANIAN:
			language = "bonkenc_ro.xml";
			break;
		case LANG_RUSSIAN:
			language = "bonkenc_ru.xml";
			break;
		case LANG_SERBIAN:
			language = "bonkenc_sr.xml";

			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_CROATIAN_CROATIA) language = "bonkenc_hr.xml";
			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_SERBIAN_LATIN) language = "bonkenc_sr.xml";
			break;
		case LANG_SLOVAK:
			language = "bonkenc_sk.xml";
			break;
		case LANG_SPANISH:
			language = "bonkenc_es.xml";

			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_SPANISH) language = "bonkenc_es.xml";
			if (SUBLANGID(GetUserDefaultLangID()) == SUBLANG_SPANISH_ARGENTINA) language = "bonkenc_es_AR.xml";
			break;
		case LANG_SWEDISH:
			language = "bonkenc_sv.xml";
			break;
		case LANG_TURKISH:
			language = "bonkenc_tr.xml";
			break;
		case LANG_UKRAINIAN:
			language = "bonkenc_ua.xml";
			break;
		case LANG_VIETNAMESE:
			language = "bonkenc_vi.xml";
			break;
	}
#endif

	if (language != "internal")
	{
		if (!File(GUI::Application::GetApplicationDirectory().Append("lang").Append(Directory::GetDirectoryDelimiter()).Append(language)).Exists()) language = "internal";
	}

	return language;
}

Void BonkEnc::BonkEncGUI::CheckForUpdates()
{
	(new JobCheckForUpdates(False))->Schedule();
}
