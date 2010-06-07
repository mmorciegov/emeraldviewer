/* Copyright (c) 2010 Katharine Berry All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Katharine Berry nor the names of any contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KATHARINE BERRY AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KATHARINE BERRY OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "floaterskinfinder.h"
#include "lluictrlfactory.h"
#include "llmediactrl.h"
#include "llnotifications.h"
#include "llcommandhandler.h"
#include "lldir.h"
#include "llfile.h"
#include "llhttpclient.h"
#include "llbufferstream.h"
#include "llviewercontrol.h"
#include "llfloaterpreference.h"

//TODO: change URL. Be nice if I actually had write access to anything useful.
FloaterSkinfinder::FloaterSkinfinder(const LLSD& key = LLSD()) : LLFloaterHtmlSimple("http://lab.kathar.in/emeraldskins/")
{
	setTrusted(true);
	setTitle("Skin Browser");
	center();
}

void FloaterSkinfinder::show(void*)
{
	FloaterSkinfinder::showInstance(LLSD());
}

class FloaterSkinfinderHandler : public LLCommandHandler
{
public:
	FloaterSkinfinderHandler() : LLCommandHandler("emskin", false) { }
	
	bool handle(const LLSD& tokens, const LLSD& query_map,	LLMediaCtrl* web);
	static void notificationCallback(const LLSD& notification, const LLSD& response);
};

FloaterSkinfinderHandler gFloaterSkinfinderHandler;

bool FloaterSkinfinderHandler::handle(const LLSD& tokens, const LLSD& query_map,	LLMediaCtrl* web)
{
	if(tokens.size() != 2)
		return false;

	if(tokens[0].asString() != "download")
		return false;
	
	std::string skin_name = tokens[1].asString();
	
	std::string display_name;
	if(query_map.has("display_name"))
	{
		display_name = query_map["display_name"].asString();
	}
	else
	{
		display_name = skin_name;
	}
	
	LLSD subs;
	subs["[DISPLAY_NAME]"] = display_name;
	LLSD payload;
	payload["display_name"] = display_name;
	payload["skin_name"] = skin_name;
	
	LLNotifications::instance().add("EmeraldSkinDownloadPrompt", subs, payload, &notificationCallback);
	
	return true;
}

void FloaterSkinfinderHandler::notificationCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if(option != 0)
		return;
	
	std::string skin_name = notification["payload"]["skin_name"];
	std::string display_name = notification["payload"]["display_name"];
	
	std::string user_skin_dir = gDirUtilp->getOSUserAppDir() + gDirUtilp->getDirDelimiter() + "skins";
	
	// Cheat if we already have it.
	if(LLFile::isdir(user_skin_dir + gDirUtilp->getDirDelimiter() + skin_name) || 
	   LLFile::isdir(gDirUtilp->getSkinBaseDir() + gDirUtilp->getDirDelimiter() + skin_name))
	{
		LL_INFOS("SkinDownload") << "Skin already downloaded; using that." << LL_ENDL;
		gSavedSettings.setString("SkinCurrent", skin_name);
		
		LLSD args;
		args["[DISPLAY_NAME]"] = display_name;
		LLNotifications::instance().add("EmeraldSkinDownloadComplete", args);
		return;
	}
	
	//TODO: change URL.
	std::string url = llformat("http://modularsystems.sl/box/skins7z/%s.7z", skin_name.c_str());
	
	LL_INFOS("SkinDownload") << "Downloading '" << display_name << "' (" << skin_name << ") skin from " << url << LL_ENDL;
	
	LLHTTPClient::get(url, new EmeraldSkinDownloader(skin_name, display_name));
	return;
}

EmeraldSkinDownloader::EmeraldSkinDownloader(const std::string& skin_name, const std::string& display_name) : 
	mSkinName(skin_name),
	mDisplayName(display_name)
{

}

void EmeraldSkinDownloader::error(U32 status, const std::string& reason)
{
	LLSD args;
	args["[REASON]"] = reason;
	args["[STATUS]"] = LLSD::Integer(status);
	args["[DISPLAY_NAME]"] = mDisplayName;
	LLNotifications::instance().add("EmeraldSkinDownloadFailed", args);
}

void EmeraldSkinDownloader::completedRaw(U32 status, const std::string& reason, const LLChannelDescriptors& channels, const LLIOPipe::buffer_ptr_t& buffer)
{
	if(status < 200 || status >= 300)
	{
		error(status, reason);
		return;
	}
	
	// There is a getUserSkinDir, but no good way to get its parent (other than "../")
	std::string skin_dir = gDirUtilp->getOSUserAppDir() + gDirUtilp->getDirDelimiter() + "skins";
	
	std::string target = skin_dir + gDirUtilp->getDirDelimiter() + mSkinName + ".7z";
	LL_INFOS("SkinDownload") << "Saving file to " << target << LL_ENDL;	
	
	// Make sure the skin directory actually exists
	LLFile::mkdir(skin_dir);
	
	// Save the file.
	LLBufferStream istr(channels, buffer.get());
	llofstream ostr(target, std::ios::binary);
	
	while(istr.good() && ostr.good())
		ostr << istr.rdbuf();
	ostr.close();
	
	decompressSkin(target, skin_dir);
	
	// Set the skin as active.
	gSavedSettings.setString("SkinCurrent", mSkinName);
	
	LLSD args;
	args["[DISPLAY_NAME]"] = mDisplayName;
	LLNotifications::instance().add("EmeraldSkinDownloadComplete", args, LLSD(), &download_complete_dismissed);
}

// HACK: There's no good way to tell when the decompression is complete (it's asynchronous,
// and waiting for it hangs SL). However, it is very fast. This uses the user clicking "OK"
// on the download dialog to refresh the skin panel (as a hackish non-blocking timer, basically)
void EmeraldSkinDownloader::download_complete_dismissed(const LLSD& notification, const LLSD& response)
{
	LLFloaterPreference::refreshSkinPanel();
}
	

bool EmeraldSkinDownloader::decompressSkin(const std::string& zip, const std::string& dir)
{
	// Forking magic stolen from llvoiceclient.cpp
	// The lzma library is a PITA. And undocumented.
#ifndef LL_WINDOWS
	std::string exe = llformat("%s/7za", gDirUtilp->getExecutableDir().c_str());
	const char *zargv[] = {exe.c_str(), "x", "-y", llformat("-o%s", dir.c_str()).c_str(), zip.c_str(), NULL};
	fflush(NULL);
	pid_t id = vfork();
	if(id == 0)
	{
		execv(exe.c_str(), (char **)zargv);
		_exit(0); // This shouldn't ever be reached.
	}
	return true;
#else
	//PROCESS_INFORMATION pinfo;
	//STARTUPINFOA sinfo;
	//memset(&sinfo, 0, sizeof(sinfo));
	std::string exe_dir = gDirUtilp->getExecutableDir();
	std::string exe_path = "\""+exe_dir +gDirUtilp->getDirDelimiter()+ "7z.exe\"";
	std::string args = llformat(" x -y -o\"%s\" \"%s\"", dir.c_str(), zip.c_str());
	
	// So retarded.  Windows requires that the second parameter to CreateProcessA be a writable (non-const) string...
	/*char *args2 = new char[args.size() + 1];
	strcpy(args2, args.c_str());
	
	if(CreateProcessA(exe_path.c_str(), args2, NULL, NULL, FALSE, 0, NULL, exe_dir.c_str(), &sinfo, &pinfo))
	{
		CloseHandle(pinfo.hThread); // stops leaks - nothing else
	}
	delete[] args2;*/
	std::string theCMD("cmd.exe /c START \"Skin Decompression\" " + exe_path + args + " & exit");
	std::system(theCMD.c_str());

	return true;
#endif
}
