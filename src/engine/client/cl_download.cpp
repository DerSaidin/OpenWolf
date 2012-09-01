/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2009 Cyril Gantin
Copyright (C) 2011 Dusan Jocic <dusanjocic@msn.com>

OpenWolf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

OpenWolf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

/*
INFO: Integration of libcurl for requesting maps from an online repository.

Usage:
  \download <mapname>     - blocking download ( hold ESC to abort )
  \download <mapname> &   - background download
  \download -             - abort current background download
  \download               - show help or background download progress

Cvar dl_source defines the url from which to query maps, eg: http://someserver/somescript.php?q=%m
The %m token is replaced with the actual map name in the query.

The server MUST return an appropriate content-type. Accepted content-type values are either application/zip
or application/octet-stream. Other content-type values will be treated as errors or queries that didn't
yield results.

The server MAY return a content-disposition header with the actual name of the pk3 file. In the absence
of a content-disposition header, the client will write the pack to a default <mapname>.pk3 location. The
filename MUST have a pk3 extension. The filename MUST NOT have directory path information - slashes (/),
backslashes (\) and colons (:) are not accepted. Finally the filename MUST consist of us-ascii characters
only (chars 32 to 126, included). A filename that doesn't comply to these specifications will raise an
error and abort the transfer.

The server MAY redirect the query to another url. Multiple redirections are permitted - limit depends on
libcurl's default settings. The end query MUST return a "200 OK" http status code.

It is desirable that the server returns a content-length header with the size of the pk3 file.

The server MAY return a custom openwolf-motd header. Its value is a string that MUST NOT exceed 127
chars. The motd string will be displayed after the download is complete. This is the place
where you take credits for setting up a server. :)

Downloaded files are written to the current gamedir of the home directory - eg. C:\quake3\mymod\ in
windows; ~/.q3a/mymod/ in linux. Name collision with an existing pk3 file will result in a failure and
be left to the user to sort out.
*/

#include "../idLib/precompiled.h"
#include <curl/curl.h>
#include "client.h"

static cvar_t *dl_verbose;      // 1: show http headers; 2: http headers +curl debug info
static cvar_t *dl_showprogress; // show console progress
static cvar_t *dl_showmotd;     // show server message
static cvar_t *dl_source;       // url to query maps from; %m token will be replaced by mapname
static cvar_t *dl_usemain;      // whether to download pk3 files in BASEGAME (default is off)

static qboolean curl_initialized;
static char useragent[256];
static CURL *curl;
static fileHandle_t f;
static char path[MAX_OSPATH];
static char dl_error[1024];     // if set, will be used in place of libcurl's error message.
static char motd[128];

static size_t Curl_WriteCallbackFromNet_f(void *ptr, size_t size, size_t nmemb, void *stream) {
    if (!f) {
        char dir[MAX_OSPATH];
        char *c;
        // make sure Content-Type is either "application/octet-stream" or "application/zip".
        if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &c) != CURLE_OK || !c || (Q_stricmp(c, "application/octet-stream") && Q_stricmp(c, "application/zip"))) {
            Q_strncpyz(dl_error, "No pk3 returned - requested map is probably unknown.", sizeof(dl_error));
            return 0;
        }
        // make sure the path doesn't have directory information.
        for (c=path; *c; c++) {
            if (*c == '\\' || *c == '/' || *c == ':') {
                Com_sprintf(dl_error, sizeof(dl_error), "Destination filename \"%s\" is not valid.", path);
                return 0;
            }
        }
        // make sure the file has an appropriate extension.
        c = path +strlen(path) -4;
        if (c <= path || strcmp(c, ".pk3")) {
            Com_sprintf(dl_error, sizeof(dl_error), "Returned file \"%s\" has wrong extension.", path);
            return 0;
        }
        // make out the directory in which to place the file
		//
		// Dushan - FIX ME
		//
        Q_strncpyz(dir, (dl_usemain->integer)?BASEGAME:FS_GetGameDir(), sizeof(dir));
        if (strlen(path) +strlen(dir) +1 >= sizeof(path)) {
            Com_sprintf(dl_error, sizeof(dl_error), "Returned filename is too large.");
            return 0;
        }
		Com_sprintf(path, sizeof(path), "%s/%s", dir, path);
		//
		// Dushan - FIX ME
		//
        // in case of a name collision, just fail - leave it to the user to sort out.
        if (FS_SV_FileExists(path)) {
            Com_sprintf(dl_error, sizeof(dl_error), "Failed to download \"%s\", a pk3 by that name exists locally.", path);
            return 0;
        }
        // change the extension to .tmp - it will be changed back once the download is complete.
        c = path +strlen(path) -4;
        strcpy(c, ".tmp");

        // FS should write the file in the appropriate gamedir and catch unsanitary paths.
        f = FS_SV_FOpenFileWrite(path);
        if (!f) {
            Com_sprintf(dl_error, sizeof(dl_error), "Failed to open \"%s\" for writing.\n", path);
            return 0;
        }
        Com_Printf("Writing to: %s\n", path);
    }
    return FS_Write(ptr, size*nmemb, f);
}

static size_t Curl_HeaderCallbackFromNet_f(const char *ptr, size_t size, size_t nmemb, void *stream) {
    char buf[1024];
    char *c;

    // make a copy and remove the trailing crlf chars.
    if (size*nmemb >= sizeof(buf)) {
        Q_strncpyz(dl_error, "Curl_HeaderCallbackFromNet_f() overflow.", sizeof(dl_error));
        return (size_t)-1;
    }
    Q_strncpyz(buf, ptr, size*nmemb+1);
    c = buf +strlen(buf)-1;
    while (c>buf && (*c == '\r' || *c == '\n')) {
        *(c--) = 0;
    }

    // make sure it's not empty.
    if (c <= buf) {
        return size*nmemb;
    }

    // verbose output
    if (dl_verbose->integer > 0) {
        Com_Printf("< %s\n", buf);
    }
    /**
        * Check whether this is a content-disposition header.
        * Apparently RFC2183 has precise rules for the presentation of the filename attribute.
        * No one seems to respect those, though.
        * Accepted presentations:
        *      filename="somefile.pk3"
        *      filename='somefile.pk3'
        *      filename=somefile.pk3
        * Quoted strings won't support escaping (eg. "some\"file.pk3").
        * Malformed quoted strings that miss the trailing quotation mark will pass.
        * Only us-ascii chars are accepted.
        * The actual filename will be validated later, when the transfer is started.
        */
    if (!idStr::Icmpn(buf, "content-disposition:", 20)) {
        const char *c = strstr(buf, "filename=") +9;
        if (c != (const char*)9) {
            const char *e;
            char token=0;
            if (*c == '"' || *c == '\'') {
                token = *c++;
            }
            for (e=c; *e && *e != token; e++) {
                if (*e<32 || *e > 126) {
                    Q_strncpyz(dl_error, "Server returned an invalid filename.", sizeof(dl_error));
                    return (size_t)-1;
                }
            }
            if (e == c || e-c >= sizeof(path)) {
                Q_strncpyz(dl_error, "Server returned an invalid filename.", sizeof(dl_error));
                return (size_t)-1;
            }
            Q_strncpyz(path, c, e-c+1);     // +1 makes room for the trailing \0
        }
    }

    // catch openwolf headers
    if (!idStr::Icmpn(buf, "openwolf-motd: ", 17)) {
        if (strlen(buf) >= 17+sizeof(motd)) {
            if (dl_showmotd->integer) {
                Com_Printf("Warning: server motd string too large.\n");
            }
        } else {
            Q_strncpyz(motd, buf+17, sizeof(motd));
        }
    }
    return size*nmemb;
}

static size_t Curl_VerboseCallbackFromNet(CURL *curl, curl_infotype type, char *data, size_t size, void *userptr) {
    char buf[1024];
    char *c, *l;

    if ((type != CURLINFO_HEADER_OUT || dl_verbose->integer < 1) && (type != CURLINFO_TEXT || dl_verbose->integer < 2)) {
        return 0;
    }
    if (size >= sizeof(buf)) {
        Com_Printf("Curl_VerboseCallbackFromNet() warning: overflow.\n");
        return 0;
    }
    Q_strncpyz(buf, data, size+1);  // +1 makes room for the trailing \0
    if (type == CURLINFO_HEADER_OUT) {
        for (l=c=buf; c-buf<size; c++) {
            // header lines should have linefeeds.
            if (*c == '\n' || *c == '\r') {
				*c = 0;
				if (c>l) {
					Com_Printf("> %s\n", l);
				}
				l = c+1;
			}
        }
        return 0;
    }
    // CURLINFO_TEXT (has its own linefeeds)
    Com_Printf("%s", buf);  // Com_Printf(buf) would result in random output/segfault if buf has % chars.
    return 0;
}

/**
 * This callback is called on regular intervals, whether data is being transferred or not.
 */
static int Curl_ProgressCallbackFromNet_f(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    static double stransferred = -1.0;

    if (dl_showprogress->integer && dlnow != stransferred && f) { // don't show progress if file hasn't been initialized (might be downloading an error page).
        double speed;
        if (dltotal != 0.0      // content-size is known
                && dlnow <= dltotal) {  // and appropriate
            if (dltotal > 1024.0 * 1024.0) {        // MB range
                Com_Printf("%.1f/%.1fMB", dlnow/1024.0/1024.0, dltotal/1024.0/1024.0);
            } else if (dltotal > 10240.0) {         // KB range (>10KB)
                Com_Printf("%.1f/%.1fKB", dlnow/1024.0, dltotal/1024.0);
            } else {                                                        // byte range
                Com_Printf("%.0f/%.0fB", dlnow, dltotal);
            }
        } else {        // unknown content-size
            if (dlnow > 1024.0 * 1024.0) {          // MB range
                Com_Printf("%.1fMB", dlnow/1024.0/1024.0);
            } else if (dlnow > 10240.0) {           // KB range (>10KB)
                Com_Printf("%.1fKB", dlnow/1024.0);
            } else {                                                        // byte range
                Com_Printf("%.0fB", dlnow);     // uZu was dlnow, dltotal
            }
        }
        if (!curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed)) {
            Com_Printf(" @%.1fKB/s", speed/1024.0);
        }
        if (dltotal != 0.0 && dlnow <= dltotal) {
            Com_Printf(" (%2.1f%%)", 100.0*dlnow/dltotal);
        }
        Com_Printf("\n");
        stransferred = dlnow;
    }

    // pump events and refresh screen
    Com_EventLoop();
    SCR_UpdateScreen();
    if (idKeyInput::IsDown(K_ESCAPE)) {
        Q_strncpyz(dl_error, "Download aborted.", sizeof(dl_error));
        return -1;
    }
    return 0;
}

static void Curl_RepoDownload_f(void) {
    char url[1024];
    CURLcode res;
    char errorbuf[CURL_ERROR_SIZE];
    char *c;

    if (Cmd_Argc() < 2) {
        Com_Printf("usage: download <mapname>\n");
        return;
    }
    if (FS_FileIsInPAK(va("maps/%s.bsp", Cmd_Argv(1)), NULL) != -1) {
        Com_Printf("Map already exists locally.\n");
        return;
    }
    if (idStr::Icmpn(dl_source->string, "http://", 7)) {
        if (strstr(dl_source->string, "://")) {
            Com_Printf("Invalid dl_source.\n");
            return;
        }
        Cvar_Set("dl_source", va("http://%s", dl_source->string));
    }
    if ((c = strstr(dl_source->string, "%m")) == 0) {
        Com_Printf("Cvar dl_source is missing a %%m token.\n");
        return;
    }
    if (strlen(dl_source->string) -2 +strlen(Cmd_Argv(1)) >= sizeof(url)) {
        Com_Printf("Cvar dl_source too large.\n");
        return;
    }

    Q_strncpyz(url, dl_source->string, c-dl_source->string +1);     // +1 makes room for the trailing 0
    Com_sprintf(url, sizeof(url), "%s%s%s", url, Cmd_Argv(1), c+2);

    // set a default destination filename; Content-Disposition headers will override.
    Com_sprintf(path, sizeof(path), "%s.pk3", Cmd_Argv(1));

    curl = curl_easy_init();
    if (!curl) {
        Com_Printf("Download failed to initialize.\n");
        return;
    }
    *dl_error = 0;
    *motd = 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1); // fail if http returns an error code
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl_WriteCallbackFromNet_f);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, Curl_HeaderCallbackFromNet_f);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, Curl_VerboseCallbackFromNet);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, Curl_ProgressCallbackFromNet_f);
    Com_Printf("Attempting download: %s\n", url);
    SCR_UpdateScreen();
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        Com_Printf("%s\n", (*dl_error) ? dl_error : errorbuf);
    } else {
        Com_Printf("Download complete, restarting filesystem.\n");
    }
    curl_easy_cleanup(curl);
    curl = NULL;
    if (f) {
        FS_FCloseFile(f);
        f = 0;
        if (res == CURLE_OK) {  // download succeeded
            char dest[MAX_OSPATH];
            Q_strncpyz(dest, path, strlen(path)-3); // -4 +1 for the trailing \0
            Q_strcat(dest, sizeof(dest), ".pk3");
            if (!FS_SV_FileExists(dest)) {
                FS_SV_Rename(path, dest);
                FS_Restart(clc.checksumFeed);
                if (dl_showmotd->integer && *motd) {
                        Com_Printf("Server motd: %s\n", motd);
                }
            } else {
                // normally such errors should be caught upon starting the transfer. Anyway better do
                // it here again - the filesystem might have changed, plus this may help contain some
                // bugs / exploitable flaws in the code.
                Com_Printf("Failed to copy downloaded file to its location - file already exists.\n");
                FS_OW_RemoveFile(path);
            }
        } else {
            FS_OW_RemoveFile(path);
        }
    }
}

//
// Interface
//
void OWDL_Init( void ) {
    if (!curl_global_init(CURL_GLOBAL_ALL)) {
        char *c;
        Cmd_AddCommand("download", Curl_RepoDownload_f);
        curl_initialized = qtrue;

        Q_strncpyz(useragent, Q3_VERSION, sizeof(useragent));
        for (c=useragent; *c; c++) {
            if (*c == ' ') *c = '/';
        }
        Com_sprintf(useragent, sizeof(useragent), "%s (%s) ", useragent, curl_version());
    } else {
        Com_Printf("Failed to initialize libcurl.\n");
    }
    dl_verbose = Cvar_Get("dl_verbose", "0", 0);
    dl_source = Cvar_Get("dl_source", "http://localhost/getpk3bymapname.php/%m", CVAR_ARCHIVE);
    dl_showprogress = Cvar_Get("dl_showprogress", "1", CVAR_ARCHIVE);
    dl_showmotd = Cvar_Get("dl_showmotd", "1", CVAR_ARCHIVE);
    dl_usemain = Cvar_Get("dl_usemain", "1", CVAR_ARCHIVE);
}

qboolean OWDL_Active( void ) {
    return (qboolean)(f);
}
