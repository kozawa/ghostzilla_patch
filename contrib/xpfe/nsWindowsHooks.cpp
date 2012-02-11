/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Bill Law       <law@netscape.com>
 *  Syd Logan      <syd@netscape.com> added turbo mode stuff
 *  Joe Elwell     <jelwell@netscape.com>
 *  H�kan Waara    <hwaara@chello.se>
 *  Aaron Kaluszka <ask@swva.net>
 *  Jeremy Morton  <jez9999@runbox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

// Implementation utilities.
#include "nsIDOMWindowInternal.h"
#include "nsIServiceManager.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsIAllocator.h"
#include "nsICmdLineService.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsWindowsHooksUtil.cpp"
#include "nsWindowsHooks.h"
#include <windows.h>
#include <shlobj.h>
#include <shlguid.h>

// for set as wallpaper
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIImageLoadingContent.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"

#define RUNKEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"

// Objects that describe the Windows registry entries that we need to tweak.
static ProtocolRegistryEntry
    http( "http" ),
    https( "https" ),
    ftp( "ftp" ),
    chrome( "chrome" ),
    gopher( "gopher" );
const char *jpgExts[]  = { ".jpg", ".jpe", ".jpeg", ".jfif", ".pjpeg", ".pjp", 0 };
const char *gifExts[]  = { ".gif", 0 };
const char *pngExts[]  = { ".png", 0 };
const char *mngExts[]  = { ".mng", 0 };
const char *xbmExts[]  = { ".xbm", 0 };
const char *bmpExts[]  = { ".bmp", ".rle", ".dib", 0 };
const char *icoExts[]  = { ".ico", 0 };
const char *xmlExts[]  = { ".xml", 0 };
const char *xhtmExts[] = { ".xht", ".xhtml", 0 };
const char *xulExts[]  = { ".xul", 0 };
const char *htmExts[]  = { ".htm", ".html", ".shtml", 0 };

static FileTypeRegistryEntry
    jpg(   jpgExts,  "MozillaJPEG",  "JPEG Image",           "jpegfile", "jpeg-file.ico"),
    gif(   gifExts,  "MozillaGIF",   "GIF Image",            "giffile",  "gif-file.ico"),
    png(   pngExts,  "MozillaPNG",   "PNG Image",            "pngfile",  "image-file.ico"),
    mng(   mngExts,  "MozillaMNG",   "MNG Image",            "",         "image-file.ico"),
    xbm(   xbmExts,  "MozillaXBM",   "XBM Image",            "xbmfile",  "image-file.ico"),
    bmp(   bmpExts,  "MozillaBMP",   "BMP Image",            "",         "image-file.ico"),
    ico(   icoExts,  "MozillaICO",   "Icon",                 "icofile",  "%1"),
    xml(   xmlExts,  "MozillaXML",   "XML Document",         "xmlfile",  "xml-file.ico"),
    xhtml( xhtmExts, "MozillaXHTML", "XHTML Document",       "",         "misc-file.ico"),
    xul(   xulExts,  "MozillaXUL",   "Mozilla XUL Document", "",         "xul-file.ico");

static EditableFileTypeRegistryEntry
    mozillaMarkup( htmExts, "MozillaHTML", "HTML Document", "htmlfile",  "html-file.ico");

// Implementation of the nsIWindowsHooksSettings interface.
// Use standard implementation of nsISupports stuff.
NS_IMPL_ISUPPORTS1( nsWindowsHooksSettings, nsIWindowsHooksSettings )

nsWindowsHooksSettings::nsWindowsHooksSettings() {
}

nsWindowsHooksSettings::~nsWindowsHooksSettings() {
}

// Generic getter.
NS_IMETHODIMP
nsWindowsHooksSettings::Get( PRBool *result, PRBool nsWindowsHooksSettings::*member ) {
    NS_ENSURE_ARG( result );
    NS_ENSURE_ARG( member );
    *result = this->*member;
    return NS_OK;
}

// Generic setter.
NS_IMETHODIMP
nsWindowsHooksSettings::Set( PRBool value, PRBool nsWindowsHooksSettings::*member ) {
    NS_ENSURE_ARG( member );
    this->*member = value;
    return NS_OK;
}

// Macros to define specific getter/setter methods.
#define DEFINE_GETTER_AND_SETTER( attr, member ) \
NS_IMETHODIMP \
nsWindowsHooksSettings::Get##attr ( PRBool *result ) { \
    return this->Get( result, &nsWindowsHooksSettings::member ); \
} \
NS_IMETHODIMP \
nsWindowsHooksSettings::Set##attr ( PRBool value ) { \
    return this->Set( value, &nsWindowsHooksSettings::member ); \
}

// Define all the getter/setter methods:
DEFINE_GETTER_AND_SETTER( IsHandlingHTML,   mHandleHTML   )
DEFINE_GETTER_AND_SETTER( IsHandlingJPEG,   mHandleJPEG   )
DEFINE_GETTER_AND_SETTER( IsHandlingGIF,    mHandleGIF    )
DEFINE_GETTER_AND_SETTER( IsHandlingPNG,    mHandlePNG    )
DEFINE_GETTER_AND_SETTER( IsHandlingMNG,    mHandleMNG    )
DEFINE_GETTER_AND_SETTER( IsHandlingXBM,    mHandleXBM    )
DEFINE_GETTER_AND_SETTER( IsHandlingBMP,    mHandleBMP    )
DEFINE_GETTER_AND_SETTER( IsHandlingICO,    mHandleICO    )
DEFINE_GETTER_AND_SETTER( IsHandlingXML,    mHandleXML    )
DEFINE_GETTER_AND_SETTER( IsHandlingXHTML,  mHandleXHTML  )
DEFINE_GETTER_AND_SETTER( IsHandlingXUL,    mHandleXUL    )
DEFINE_GETTER_AND_SETTER( IsHandlingHTTP,   mHandleHTTP   )
DEFINE_GETTER_AND_SETTER( IsHandlingHTTPS,  mHandleHTTPS  )
DEFINE_GETTER_AND_SETTER( IsHandlingFTP,    mHandleFTP    )
DEFINE_GETTER_AND_SETTER( IsHandlingCHROME, mHandleCHROME )
DEFINE_GETTER_AND_SETTER( IsHandlingGOPHER, mHandleGOPHER )
DEFINE_GETTER_AND_SETTER( ShowDialog,       mShowDialog   )
DEFINE_GETTER_AND_SETTER( HaveBeenSet,      mHaveBeenSet  )


// Implementation of the nsIWindowsHooks interface.
// Use standard implementation of nsISupports stuff.
NS_IMPL_ISUPPORTS2( nsWindowsHooks, nsIWindowsHooks, nsIWindowsRegistry )

nsWindowsHooks::nsWindowsHooks() {
}

nsWindowsHooks::~nsWindowsHooks() {
}

// Internal GetPreferences.
NS_IMETHODIMP
nsWindowsHooks::GetSettings( nsWindowsHooksSettings **result ) {
    nsresult rv = NS_OK;

    // Validate input arg.
    NS_ENSURE_ARG( result );

    // Allocate prefs object.
    nsWindowsHooksSettings *prefs = *result = new nsWindowsHooksSettings;
    NS_ENSURE_TRUE( prefs, NS_ERROR_OUT_OF_MEMORY );

    // Got it, increment ref count.
    NS_ADDREF( prefs );

    // Get each registry value and copy to prefs structure.
    prefs->mHandleHTTP   = BoolRegistryEntry( "isHandlingHTTP"   );
    prefs->mHandleHTTPS  = BoolRegistryEntry( "isHandlingHTTPS"  );
    prefs->mHandleFTP    = BoolRegistryEntry( "isHandlingFTP"    );
    prefs->mHandleCHROME = BoolRegistryEntry( "isHandlingCHROME" );
    prefs->mHandleGOPHER = BoolRegistryEntry( "isHandlingGOPHER" );
    prefs->mHandleHTML   = BoolRegistryEntry( "isHandlingHTML"   );
    prefs->mHandleJPEG   = BoolRegistryEntry( "isHandlingJPEG"   );
    prefs->mHandleGIF    = BoolRegistryEntry( "isHandlingGIF"    );
    prefs->mHandlePNG    = BoolRegistryEntry( "isHandlingPNG"    );
    prefs->mHandleMNG    = BoolRegistryEntry( "isHandlingMNG"    );
    prefs->mHandleXBM    = BoolRegistryEntry( "isHandlingXBM"    );
    prefs->mHandleBMP    = BoolRegistryEntry( "isHandlingBMP"    );
    prefs->mHandleICO    = BoolRegistryEntry( "isHandlingICO"    );
    prefs->mHandleXML    = BoolRegistryEntry( "isHandlingXML"    );
    prefs->mHandleXHTML  = BoolRegistryEntry( "isHandlingXHTML"  );
    prefs->mHandleXUL    = BoolRegistryEntry( "isHandlingXUL"    );
    prefs->mShowDialog   = BoolRegistryEntry( "showDialog"       );
    prefs->mHaveBeenSet  = BoolRegistryEntry( "haveBeenSet"      );

#ifdef DEBUG_law
NS_WARN_IF_FALSE( NS_SUCCEEDED( rv ), "GetPreferences failed" );
#endif

    return rv;
}

// Public interface uses internal plus a QI to get to the proper result.
NS_IMETHODIMP
nsWindowsHooks::GetSettings( nsIWindowsHooksSettings **_retval ) {
    // Allocate prefs object.
    nsWindowsHooksSettings *prefs;
    nsresult rv = this->GetSettings( &prefs );

    if ( NS_SUCCEEDED( rv ) ) {
        // QI to proper interface.
        rv = prefs->QueryInterface( NS_GET_IID( nsIWindowsHooksSettings ), (void**)_retval );
        // Release (to undo our Get...).
        NS_RELEASE( prefs );
    }

    return rv;
}

static PRBool misMatch( const PRBool &flag, const ProtocolRegistryEntry &entry ) {
    PRBool result = PR_FALSE;
    // Check if we care.
    if ( flag ) { 
        // Compare registry entry setting to what it *should* be.
        if ( entry.currentSetting() != entry.setting ) {
            result = PR_TRUE;
        }
    }

    return result;
}

// isAccessRestricted - Returns PR_TRUE iff this user only has restricted access
// to the registry keys we need to modify.
static PRBool isAccessRestricted() {
	/* [ghzil] new name {
    char   subKey[] = "Software\\Mozilla - Test Key";
    } [ghzil] { */
    char   subKey[] = "Software\\Ghostzilla - Test Key";
    /* } [ghzil] */
    PRBool result = PR_FALSE;
    DWORD  dwDisp = 0;
    HKEY   key;
    // Try to create/open a subkey under HKLM.
    DWORD rc = ::RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                                 subKey,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_WRITE,
                                 NULL,
                                 &key,
                                 &dwDisp );

    if ( rc == ERROR_SUCCESS ) {
        // Key was opened; first close it.
        ::RegCloseKey( key );
        // Delete it if we just created it.
        switch( dwDisp ) {
            case REG_CREATED_NEW_KEY:
                ::RegDeleteKey( HKEY_LOCAL_MACHINE, subKey );
                break;
            case REG_OPENED_EXISTING_KEY:
                break;
        }
    } else {
        // Can't create/open it; we don't have access.
        result = PR_TRUE;
    }

    return result;
}



// Implementation of method that checks whether the settings match what's in the
// Windows registry.
NS_IMETHODIMP
nsWindowsHooksSettings::GetRegistryMatches( PRBool *_retval ) {
    NS_ENSURE_ARG( _retval );
    *_retval = PR_TRUE;
    // Test registry for all selected attributes.
    if ( misMatch( mHandleHTTP,   http )
         ||
         misMatch( mHandleHTTPS,  https )
         ||
         misMatch( mHandleFTP,    ftp )
         ||
         misMatch( mHandleCHROME, chrome )
         ||
         misMatch( mHandleGOPHER, gopher )
         ||
         misMatch( mHandleHTML,   mozillaMarkup )
         ||
         misMatch( mHandleJPEG,   jpg )
         ||
         misMatch( mHandleGIF,    gif )
         ||
         misMatch( mHandlePNG,    png )
         ||
         misMatch( mHandleMNG,    mng )
         ||
         misMatch( mHandleXBM,    xbm )
         ||
         misMatch( mHandleBMP,    bmp )
         ||
         misMatch( mHandleICO,    ico )
         ||
         misMatch( mHandleXML,    xml )
         ||
         misMatch( mHandleXHTML,  xhtml )
         ||
         misMatch( mHandleXUL,    xul ) ) {
        // Registry is out of synch.
        *_retval = PR_FALSE;
    }
    return NS_OK;
}

// [ghzil] {
#include "../pref/nsIPref.h"
#include "nsIServiceManagerUtils.h"

// import ie proxies so that ghostzilla works out of the box
static void importIEProxies() {
	nsCOMPtr<nsIPref> pPrefs(do_GetService(NS_PREF_CONTRACTID));
	if (!pPrefs)  {
    	return;
	}
    int everChecked = 0;
    pPrefs->GetIntPref( "network.proxy.ever_checked", &everChecked );
    if (everChecked == 1) {
        return;
    }

	LONG rc;
	HKEY key;
    #define REGPROXY "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"
	rc = RegOpenKey( HKEY_CURRENT_USER, REGPROXY, &key );
	if (rc != ERROR_SUCCESS) {
		return;
	}

    pPrefs->SetIntPref( "network.proxy.ever_checked", 1 );

	DWORD len;
	BYTE  databuf[1000] = { 0 };

	len = 1000;
	rc = RegQueryValueEx( key, "ProxyEnable", NULL, NULL, databuf, &len );
	if (rc != ERROR_SUCCESS) {
		RegCloseKey( key );
		return;
	}

	// check if the proxy is autoconfig
	len = 1000;
	rc = RegQueryValueEx( key, "AutoConfigURL", NULL, NULL, databuf, &len );
	if (rc == ERROR_SUCCESS && len > 0) {
		// autoconfig:
		pPrefs->SetIntPref( "network.proxy.type", 2 );
		pPrefs->SetCharPref( "network.proxy.autoconfig_url", (const char *)databuf );
		RegCloseKey( key );
		return;
	}

	// check if proxy is enabled
	if (*(DWORD *)databuf == 0) {
		pPrefs->SetIntPref( "network.proxy.type", 0 );
		RegCloseKey( key );
		return;
	}

	// it is, unfortunately. (i really don't feel like doing this. mozilla should be
	// importing ie proxies by default on the first installation, but no, why would 
	// we acknowledge the reality that everybody has a properly setup explorer 
	// on their win machines. who knows how many people gave up on trying mozilla
	// because they couldn't get it to work in the first two minutes.)

	// get the proxy server variable
	len = 1000;
	rc = RegQueryValueEx( key, "ProxyServer", NULL, NULL, databuf, &len );
	if (rc != ERROR_SUCCESS) {
		RegCloseKey( key );
		return;
	}

	// parse it now. i wish i had flex here.
	char httpProxy[300] = { 0 };
    int  httpPort = 0;
	char httpsProxy[300] = { 0 };
    int  httpsPort = 0;
	char ftpProxy[300] = { 0 };
    int  ftpPort = 0;
	char socksProxy[300] = { 0 };
    int  socksPort = 0;
	// don't care about gopher (don't even know what it is)

	if (strchr( (const char *)databuf, '=' ) == NULL) {
		// one proxy to find them all
		char proxy[300] = { 0 };
		int  port = 0;
		strcpy( proxy, (const char *)databuf );
		DWORD i;
		for (i = 0; i < len && proxy[i] != ':'; i++);
		if (proxy[i + 1] == '/') {
			for (++i; i < len && proxy[i] != ':'; i++);
		}
		proxy[i] = '\0';
		port = atoi( & proxy[i + 1] );
		strcpy( httpProxy,  proxy ); httpPort  = port;
		strcpy( httpsProxy, proxy ); httpsPort = port;
		strcpy( ftpProxy,   proxy ); ftpPort   = port;
		strcpy( socksProxy, proxy ); socksPort = port;
	} else {
		DWORD  s = 0;
		do {
			// proto=host:port...
			char proto[20] = { 0 };
			char proxy[300] = { 0 };
			char sport[20] = { 0 };
			int  port = 0;
			DWORD i;
			for (i = s; i < len && databuf[i] != '='; i++);
			if (databuf[i] != '=') break;
			int h = ++i;
			for (; i < len && databuf[i] != ':'; i++);
			if (databuf[i] != ':') break;
			if (databuf[i + 1] == '/') {
				for (++i; i < len && databuf[i] != ':'; i++);
			}
			if (databuf[i] != ':') break;
			int p = ++i;
			for (; i < len && databuf[i] != ';'; i++);
			strncpy( proto, (const char *)(databuf + s), h - s - 1 );
			proto[ h - s - 1 ] = '\0';
			strncpy( proxy, (const char *)(databuf + h), p - h - 1 );
			proxy[ p - h - 1 ] = '\0';
			strncpy( sport, (const char *)(databuf + p), i - p );
			sport[ i - p ] = '\0';
			port = atoi( sport );

			if (!strcmp( proto, "http"  ) ) { strcpy( httpProxy,  proxy ); httpPort  = port; }
			if (!strcmp( proto, "https" ) ) { strcpy( httpsProxy, proxy ); httpsPort = port; }
			if (!strcmp( proto, "ftp"   ) ) { strcpy( ftpProxy,   proxy ); ftpPort   = port; }
			if (!strcmp( proto, "socks" ) ) { strcpy( socksProxy, proxy ); socksPort = port; }

			s = i + 1;
		} while (s < len);
	}

	pPrefs->SetIntPref( "network.proxy.type", 1 );

	if (httpProxy[0] != '\0') {
		pPrefs->SetCharPref( "network.proxy.http",       httpProxy );
		pPrefs->SetIntPref(  "network.proxy.http_port",  httpPort  );
	}
	if (httpsProxy[0] != '\0') {
		pPrefs->SetCharPref( "network.proxy.ssl",        httpsProxy );
		pPrefs->SetIntPref(  "network.proxy.ssl_port",   httpsPort  );
	}
	if (ftpProxy[0] != '\0') {
		pPrefs->SetCharPref( "network.proxy.ftp",        ftpProxy   );
		pPrefs->SetIntPref(  "network.proxy.ftp_port",   ftpPort    );
	}
	if (socksProxy[0] != '\0') {
		pPrefs->SetCharPref( "network.proxy.socks",      socksProxy );
		pPrefs->SetIntPref(  "network.proxy.socks_port", socksPort  );
	}

	// check if some urls override the proxy
	len = 1000;
	rc = RegQueryValueEx( key, "ProxyOverride", NULL, NULL, databuf, &len );
	if (rc == ERROR_SUCCESS && len > 0) {
		// autoconfig:
		pPrefs->SetCharPref( "network.proxy.no_proxies_on", (const char *)databuf );
	}

	RegCloseKey( key );
}

void CheckHiding()
{
	nsCOMPtr<nsIPref> pPrefs(do_GetService(NS_PREF_CONTRACTID));
	if (!pPrefs)  {
    	return;
	}
    int hl = 0;
    // pazi vic: kaze ulazi ciga u prodavnicu sportske opreme i rekvizita:
    // Dobar dan, imate nuncaku palice? Prodavacica: Nemamo.
    // Tek ce ciga: Pa sta mi nindze sad da radimo?!
    pPrefs->GetIntPref( "browser.HidingLevel", &hl );
    if (hl > 0 && hl < 7) {
        __declspec(dllimport) void ghzil_SetHidingLevel_26(int);
        ghzil_SetHidingLevel_26( hl );
    }
}
// } [ghzil]

// Implementation of method that checks settings versus registry and prompts user
// if out of synch.
NS_IMETHODIMP
nsWindowsHooks::CheckSettings( nsIDOMWindowInternal *aParent, 
                               PRBool *_retval ) {
    // [ghzil] {
    CheckHiding();
    // } [ghzil]

    nsresult rv = NS_OK;
    *_retval = PR_FALSE;

    // Only do this once!
    static PRBool alreadyChecked = PR_FALSE;
    if ( alreadyChecked ) {
        return NS_OK;
    } else {
        alreadyChecked = PR_TRUE;
        // [ghzil] {
        importIEProxies();
        return NS_OK;
        // } [ghzil]
        // Don't check further if we don't have sufficient access.
        if ( isAccessRestricted() ) {
            return NS_OK;
        }
    }

    // Get settings.
    nsWindowsHooksSettings *settings;
    rv = this->GetSettings( &settings );

    if ( NS_SUCCEEDED( rv ) && settings ) {
        // If not set previously, set to defaults so that they are
        // set properly when/if the user says to.
        if ( !settings->mHaveBeenSet ) {
            settings->mHandleHTTP   = PR_TRUE;
            settings->mHandleHTTPS  = PR_TRUE;
            settings->mHandleFTP    = PR_TRUE;
            settings->mHandleCHROME = PR_TRUE;
            settings->mHandleGOPHER = PR_TRUE;
            settings->mHandleHTML   = PR_TRUE;
            settings->mHandleJPEG   = PR_TRUE;
            settings->mHandleGIF    = PR_TRUE;
            settings->mHandlePNG    = PR_TRUE;
            settings->mHandleMNG    = PR_TRUE;
            settings->mHandleXBM    = PR_TRUE;
            settings->mHandleBMP    = PR_FALSE;
            settings->mHandleICO    = PR_FALSE;
            settings->mHandleXML    = PR_TRUE;
            settings->mHandleXHTML  = PR_TRUE;
            settings->mHandleXUL    = PR_TRUE;

            settings->mShowDialog       = PR_TRUE;
        }

        // If launched with "-installer" then override mShowDialog.
        PRBool installing = PR_FALSE;
        if ( !settings->mShowDialog ) {
            // Get command line service.
            nsCID cmdLineCID = NS_COMMANDLINE_SERVICE_CID;
            nsCOMPtr<nsICmdLineService> cmdLineArgs( do_GetService( cmdLineCID, &rv ) );
            if ( NS_SUCCEEDED( rv ) && cmdLineArgs ) {
                // See if "-installer" was specified.
                nsXPIDLCString installer;
                rv = cmdLineArgs->GetCmdLineValue( "-installer", getter_Copies( installer ) );
                if ( NS_SUCCEEDED( rv ) && installer ) {
                    installing = PR_TRUE;
                }
            }
        }

        // First, make sure the user cares.
        if ( settings->mShowDialog || installing ) {
            // Look at registry setting for all things that are set.
            PRBool matches = PR_TRUE;
            settings->GetRegistryMatches( &matches );
            if ( !matches ) {
                // Need to prompt user.
                // First:
                //   o We need the common dialog service to show the dialog.
                //   o We need the string bundle service to fetch the appropriate
                //     dialog text.
                nsCID bundleCID = NS_STRINGBUNDLESERVICE_CID;
                nsCOMPtr<nsIPromptService> promptService( do_GetService("@mozilla.org/embedcomp/prompt-service;1"));
                nsCOMPtr<nsIStringBundleService> bundleService( do_GetService( bundleCID, &rv ) );

                if ( promptService && bundleService ) {
                    // Next, get bundle that provides text for dialog.
                    nsCOMPtr<nsIStringBundle> bundle;
                    nsCOMPtr<nsIStringBundle> brandBundle;
                    rv = bundleService->CreateBundle( "chrome://global-platform/locale/nsWindowsHooks.properties",
                                                      getter_AddRefs( bundle ) );
                    rv = bundleService->CreateBundle( "chrome://global/locale/brand.properties",
                                                      getter_AddRefs( brandBundle ) );
                    if ( NS_SUCCEEDED( rv ) && bundle && brandBundle ) {
                        nsXPIDLString text, label, shortName;
                        if ( NS_SUCCEEDED( ( rv = brandBundle->GetStringFromName( NS_LITERAL_STRING( "brandShortName" ).get(), 
                                             getter_Copies( shortName ) ) ) ) ) {
                            const PRUnichar* formatStrings[] = { shortName.get() };
                            if ( NS_SUCCEEDED( ( rv = bundle->FormatStringFromName( NS_LITERAL_STRING( "promptText" ).get(), 
                                                  formatStrings, 1, getter_Copies( text ) ) ) )
                                  &&
                                  NS_SUCCEEDED( ( rv = bundle->GetStringFromName( NS_LITERAL_STRING( "checkBoxLabel" ).get(),
                                                                                  getter_Copies( label ) ) ) ) ) {
                                // Got the text, now show dialog.
                                PRBool  showDialog = settings->mShowDialog;
                                PRInt32 dlgResult  = -1;
                                // No checkbox for initial display.
                                const PRUnichar *labelArg = 0;
                                if ( settings->mHaveBeenSet ) {
                                    // Subsequent display uses label string.
                                    labelArg = label;
                                }
                                // Note that the buttons need to be passed in this order:
                                //    o Yes
                                //    o Cancel
                                //    o No
                                rv = promptService->ConfirmEx(aParent, shortName, text,
                                                              (nsIPromptService::BUTTON_TITLE_YES * nsIPromptService::BUTTON_POS_0) +
                                                              (nsIPromptService::BUTTON_TITLE_CANCEL * nsIPromptService::BUTTON_POS_1) +
                                                              (nsIPromptService::BUTTON_TITLE_NO * nsIPromptService::BUTTON_POS_2),
                                                              nsnull, nsnull, nsnull, labelArg, &showDialog, &dlgResult);
                                
                                if ( NS_SUCCEEDED( rv ) ) {
                                    // Dialog was shown
                                    *_retval = PR_TRUE; 

                                    // Did they say go ahead?
                                    switch ( dlgResult ) {
                                        case 0:
                                            // User says: make the changes.
                                            // Remember "show dialog" choice.
                                            settings->mShowDialog = showDialog;
                                            // Apply settings; this single line of
                                            // code will do different things depending
                                            // on whether this is the first time (i.e.,
                                            // when "haveBeenSet" is false).  The first
                                            // time, this will set all prefs to true
                                            // (because that's how we initialized 'em
                                            // in GetSettings, above) and will update the
                                            // registry accordingly.  On subsequent passes,
                                            // this will only update the registry (because
                                            // the settings we got from GetSettings will
                                            // not have changed).
                                            //
                                            // BTW, the term "prefs" in this context does not
                                            // refer to conventional Mozilla "prefs."  Instead,
                                            // it refers to "Desktop Integration" prefs which
                                            // are stored in the windows registry.
                                            rv = SetSettings( settings );
                                            #ifdef DEBUG_law
                                                printf( "Yes, SetSettings returned 0x%08X\n", (int)rv );
                                            #endif
                                            break;

                                        case 2:
                                            // User says: Don't mess with Windows.
                                            // We update only the "showDialog" and
                                            // "haveBeenSet" keys.  Note that this will
                                            // have the effect of setting all the prefs
                                            // *off* if the user says no to the initial
                                            // prompt.
                                            BoolRegistryEntry( "haveBeenSet" ).set();
                                            if ( showDialog ) {
                                                BoolRegistryEntry( "showDialog" ).set();
                                            } else {
                                                BoolRegistryEntry( "showDialog" ).reset();
                                            }
                                            #ifdef DEBUG_law
                                                printf( "No, haveBeenSet=1 and showDialog=%d\n", (int)showDialog );
                                            #endif
                                            break;

                                        default:
                                            // User says: I dunno.  Make no changes (which
                                            // should produce the same dialog next time).
                                            #ifdef DEBUG_law
                                                printf( "Cancel\n" );
                                            #endif
                                            break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            #ifdef DEBUG_law
            else { printf( "Registry and prefs match\n" ); }
            #endif
        }
        #ifdef DEBUG_law
        else { printf( "showDialog is false and not installing\n" ); }
        #endif

        // Release the settings.
        settings->Release();
    }

    return rv;
}

// Utility to set PRBool registry value from getter method.
nsresult putPRBoolIntoRegistry( const char* valueName,
                                nsIWindowsHooksSettings *prefs,
                                nsWindowsHooksSettings::getter memFun ) {
    // Use getter method to extract attribute from prefs.
    PRBool boolValue;
    (void)(prefs->*memFun)( &boolValue );
    // Convert to DWORD.
    DWORD  dwordValue = boolValue;
    // Store into registry.
    BoolRegistryEntry pref( valueName );
    nsresult rv = boolValue ? pref.set() : pref.reset();

    return rv;
}

/* void setPreferences (in nsIWindowsHooksSettings prefs); */
NS_IMETHODIMP
nsWindowsHooks::SetSettings(nsIWindowsHooksSettings *prefs) {
    nsresult rv = NS_ERROR_FAILURE;

    putPRBoolIntoRegistry( "isHandlingHTTP",   prefs, &nsIWindowsHooksSettings::GetIsHandlingHTTP );
    putPRBoolIntoRegistry( "isHandlingHTTPS",  prefs, &nsIWindowsHooksSettings::GetIsHandlingHTTPS );
    putPRBoolIntoRegistry( "isHandlingFTP",    prefs, &nsIWindowsHooksSettings::GetIsHandlingFTP );
    putPRBoolIntoRegistry( "isHandlingCHROME", prefs, &nsIWindowsHooksSettings::GetIsHandlingCHROME );
    putPRBoolIntoRegistry( "isHandlingGOPHER", prefs, &nsIWindowsHooksSettings::GetIsHandlingGOPHER );
    putPRBoolIntoRegistry( "isHandlingHTML",   prefs, &nsIWindowsHooksSettings::GetIsHandlingHTML );
    putPRBoolIntoRegistry( "isHandlingJPEG",   prefs, &nsIWindowsHooksSettings::GetIsHandlingJPEG );
    putPRBoolIntoRegistry( "isHandlingGIF",    prefs, &nsIWindowsHooksSettings::GetIsHandlingGIF );
    putPRBoolIntoRegistry( "isHandlingPNG",    prefs, &nsIWindowsHooksSettings::GetIsHandlingPNG );
    putPRBoolIntoRegistry( "isHandlingMNG",    prefs, &nsIWindowsHooksSettings::GetIsHandlingMNG );
    putPRBoolIntoRegistry( "isHandlingXBM",    prefs, &nsIWindowsHooksSettings::GetIsHandlingXBM );
    putPRBoolIntoRegistry( "isHandlingBMP",    prefs, &nsIWindowsHooksSettings::GetIsHandlingBMP );
    putPRBoolIntoRegistry( "isHandlingICO",    prefs, &nsIWindowsHooksSettings::GetIsHandlingICO );
    putPRBoolIntoRegistry( "isHandlingXML",    prefs, &nsIWindowsHooksSettings::GetIsHandlingXML );
    putPRBoolIntoRegistry( "isHandlingXHTML",  prefs, &nsIWindowsHooksSettings::GetIsHandlingXHTML );
    putPRBoolIntoRegistry( "isHandlingXUL",    prefs, &nsIWindowsHooksSettings::GetIsHandlingXUL );
    putPRBoolIntoRegistry( "showDialog",       prefs, &nsIWindowsHooksSettings::GetShowDialog );

    // Indicate that these settings have indeed been set.
    BoolRegistryEntry( "haveBeenSet" ).set();

    rv = SetRegistry();

    return rv;
}

// Get preferences and start handling everything selected.
NS_IMETHODIMP
nsWindowsHooks::SetRegistry() {
    nsresult rv = NS_OK;

    // Get raw prefs object.
    nsWindowsHooksSettings *prefs;
    rv = this->GetSettings( &prefs );

    NS_ENSURE_TRUE( NS_SUCCEEDED( rv ), rv );

    if ( prefs->mHandleHTML ) {
        (void) mozillaMarkup.set();
    } else {
        (void) mozillaMarkup.reset();
    }
    if ( prefs->mHandleJPEG ) {
        (void) jpg.set();
    } else {
        (void) jpg.reset();
    }
    if ( prefs->mHandleGIF ) {
        (void) gif.set();
    } else {
        (void) gif.reset();
    }
    if ( prefs->mHandlePNG ) {
        (void) png.set();
    } else {
        (void) png.reset();
    }
    if ( prefs->mHandleMNG ) {
        (void) mng.set();
    } else {
        (void) mng.reset();
    }
    if ( prefs->mHandleXBM ) {
        (void) xbm.set();
    } else {
        (void) xbm.reset();
    }
    if ( prefs->mHandleBMP ) {
        (void) bmp.set();
    } else {
        (void) bmp.reset();
    }
    if ( prefs->mHandleICO ) {
        (void) ico.set();
    } else {
        (void) ico.reset();
    }
    if ( prefs->mHandleXML ) {
        (void) xml.set();
    } else {
        (void) xml.reset();
    }
    if ( prefs->mHandleXHTML ) {
        (void) xhtml.set();
    } else {
        (void) xhtml.reset();
    }
    if ( prefs->mHandleXUL ) {
        (void) xul.set();
    } else {
        (void) xul.reset();
    }
    if ( prefs->mHandleHTTP ) {
        (void) http.set();
    } else {
        (void) http.reset();
    }
    if ( prefs->mHandleHTTPS ) {
        (void) https.set();
    } else {
        (void) https.reset();
    }
    if ( prefs->mHandleFTP ) {
        (void) ftp.set();
    } else {
        (void) ftp.reset();
    }
    if ( prefs->mHandleCHROME ) {
        (void) chrome.set();
    } else {
        (void) chrome.reset();
    }
    if ( prefs->mHandleGOPHER ) {
        (void) gopher.set();
    } else {
        (void) gopher.reset();
    }

    // Call SHChangeNotify() to notify the windows shell that file
    // associations changed, and that an update of the icons need to occur.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return NS_OK;
}

NS_IMETHODIMP nsWindowsHooks::GetRegistryEntry( PRInt32 aHKEYConstant, const char *aSubKeyName, const char *aValueName, char **aResult ) {
    NS_ENSURE_ARG( aResult );
    *aResult = 0;
    // Calculate HKEY_* starting point based on input nsIWindowsHooks constant.
    HKEY hKey;
    switch ( aHKEYConstant ) {
        case HKCR:
            hKey = HKEY_CLASSES_ROOT;
            break;
        case HKCC:
            hKey = HKEY_CURRENT_CONFIG;
            break;
        case HKCU:
            hKey = HKEY_CURRENT_USER;
            break;
        case HKLM:
            hKey = HKEY_LOCAL_MACHINE;
            break;
        case HKU:
            hKey = HKEY_USERS;
            break;
        default:
            return NS_ERROR_INVALID_ARG;
    }

    // Get requested registry entry.
    nsCAutoString entry( RegistryEntry( hKey, aSubKeyName, aValueName, 0 ).currentSetting() );

    // Copy to result.
    *aResult = PL_strdup( entry.get() );

    return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

// nsIWindowsHooks.idl for documentation

/*
  * Name: IsOptionEnabled
  * Arguments:
  *     PRUnichar* option - the option line switch we check to see if it is in the registry key
  *
  * Return Value:
  *     PRBool* _retval - PR_TRUE if option is already in the registry key, otherwise PR_FALSE
  *
  * Description:
  *     This function merely checks if the passed in string exists in the (appname) Quick Launch Key or not.
  *
  * Author:
  *     Joseph Elwell 3/1/2002
*/
NS_IMETHODIMP nsWindowsHooks::IsOptionEnabled(const char* option, PRBool *_retval) { 
    NS_ASSERTION(option, "nsWindowsHooks::IsOptionEnabled requires something like \"-turbo\"");
  	*_retval = PR_FALSE;
    RegistryEntry startup ( HKEY_CURRENT_USER, RUNKEY, NS_QUICKLAUNCH_RUN_KEY, NULL );
    nsCString cargs = startup.currentSetting();
    if (cargs.Find(option, PR_TRUE) != kNotFound)
        *_retval = PR_TRUE;
    return NS_OK;
}

/*
  * Name: grabArgs
  * Arguments:
  *     char* optionline  - the full optionline as read from the (appname) Quick Launch registry key
  *
  * Return Value:
  *     char** args - A pointer to the arguments (string in optionline)
  *                   passed to the executable in the (appname) Quick Launch registry key
  *
  * Description:
  *     This function separates out the arguments from the optinline string
  *     Returning a pointer into the first arguments buffer.
  *     This function is used only locally, and is meant to reduce code size and readability.
  *
  * Author:
  *     Joseph Elwell 3/1/2002
*/
void grabArgs(char* optionline, char** args) {
    nsCRT::strtok(optionline, "\"", &optionline);
    if (optionline != NULL)
        *args = nsCRT::strtok(optionline, "\"", &optionline);
}

/*
  * Name: StartupAddOption
  * Arguments:
  *     PRUnichar* option - the option line switch we want to add to the registry key
  *
  * Return Value: none
  *
  * Description:
  *     This function adds the given option line argument to the (appname) Quick Launch registry key
  *
  * Author:
  *     Joseph Elwell 3/1/2002
*/
NS_IMETHODIMP nsWindowsHooks::StartupAddOption(const char* option) {
    NS_ASSERTION(option, "nsWindowsHooks::StartupAddOption requires something like \"-turbo\"");
    PRBool retval;
    IsOptionEnabled(option, &retval);
    if (retval) return NS_OK; //already in there
    
    RegistryEntry startup ( HKEY_CURRENT_USER, RUNKEY, NS_QUICKLAUNCH_RUN_KEY, NULL );
    nsCString cargs = startup.currentSetting();
    nsCAutoString newsetting;
    newsetting.Assign('\"');
    newsetting.Append(thisApplication());
    newsetting.Append('\"');
    if (!cargs.IsEmpty())
    {
        char* args;
        // exploiting the fact that nsString's storage is also a char* buffer.
        // NS_CONST_CAST is safe here because nsCRT::strtok will only modify
        // the existing buffer
        grabArgs(cargs.BeginWriting(), &args);
        if (args != NULL)
            newsetting.Append(args);
        else
        {
            // check for the old style registry key that doesnot quote its executable
            IsOptionEnabled("-turbo", &retval);
            if (retval)
                newsetting.Append(" -turbo");
        }
    }
    newsetting.Append(' ');
    newsetting.Append(option);
    startup.setting = newsetting;
    startup.set();    
    return NS_OK;
}

/*
  * Name: StartupRemoveOption
  * Arguments:
  *     PRUnichar* option - the option line switch we want to remove from the registry key
  *
  * Return Value: none.
  *
  * Description:
  *     This function removes the given option from the (appname) Quick Launch Key.
  *     And deletes the key entirely if no options are left.
  *
  * Author:
  *     Joseph Elwell 3/1/2002
*/
NS_IMETHODIMP nsWindowsHooks::StartupRemoveOption(const char* option) {
    NS_ASSERTION(option, "nsWindowsHooks::StartupRemoveOption requires something like \"-turbo\"");
    PRBool startupFound;
    IsOptionEnabled(option, &startupFound );
    if ( !startupFound )
        return NS_OK;               // already disabled, no need to do anything

    RegistryEntry startup ( HKEY_CURRENT_USER, RUNKEY, NS_QUICKLAUNCH_RUN_KEY, NULL );
    nsCString cargs = startup.currentSetting();
    char* args;
    // exploiting the fact that nsString's storage is also a char* buffer.
    // NS_CONST_CAST is safe here because nsCRT::strtok will only modify
    // the existing buffer
    grabArgs(cargs.BeginWriting(), &args);

    nsCAutoString launchcommand;
    if (args)
    {
        launchcommand.Assign(args);
        PRInt32 optionlocation = launchcommand.Find(option, PR_TRUE);
        // modify by one to get rid of the space we prepended in StartupAddOption
        if (optionlocation != kNotFound)
            launchcommand.Cut(optionlocation - 1, strlen(option) + 1);
    }
	
    if (launchcommand.IsEmpty())
    {
        startup.set();
    }
    else
    {
        nsCAutoString ufileName;
        ufileName.Assign('\"');
        ufileName.Append(thisApplication());
        ufileName.Append('\"');
        ufileName.Append(launchcommand);
        startup.setting = ufileName;
        startup.set();
    }
    return NS_OK;
}

nsresult
WriteBitmap(nsString& aPath, gfxIImageFrame* aImage)
{
  PRInt32 width, height;
  aImage->GetWidth(&width);
  aImage->GetHeight(&height);
  
  PRUint8* bits;
  PRUint32 length;
  aImage->GetImageData(&bits, &length);
  if (!bits) return NS_ERROR_FAILURE;
  
  PRUint32 bpr;
  aImage->GetImageBytesPerRow(&bpr);
  PRInt32 bitCount = bpr/width;
  
  // initialize these bitmap structs which we will later
  // serialize directly to the head of the bitmap file
  LPBITMAPINFOHEADER bmi = (LPBITMAPINFOHEADER)new BITMAPINFO;
  bmi->biSize = sizeof(BITMAPINFOHEADER);
  bmi->biWidth = width;
  bmi->biHeight = height;
  bmi->biPlanes = 1;
  bmi->biBitCount = (WORD)bitCount*8;
  bmi->biCompression = BI_RGB;
  bmi->biSizeImage = 0; // don't need to set this if bmp is uncompressed
  bmi->biXPelsPerMeter = 0;
  bmi->biYPelsPerMeter = 0;
  bmi->biClrUsed = 0;
  bmi->biClrImportant = 0;
  
  BITMAPFILEHEADER bf;
  bf.bfType = 0x4D42; // 'BM'
  bf.bfReserved1 = 0;
  bf.bfReserved2 = 0;
  bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bf.bfSize = bf.bfOffBits + bmi->biSizeImage;

  // get a file output stream
  nsresult rv;
  nsCOMPtr<nsILocalFile> path;
  rv = NS_NewLocalFile(aPath, PR_TRUE, getter_AddRefs(path));
  
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIOutputStream> stream;
  NS_NewLocalFileOutputStream(getter_AddRefs(stream), path);

  // write the bitmap headers and rgb pixel data to the file
  rv = NS_ERROR_FAILURE;
  if (stream) {
    PRUint32 written;
    stream->Write((const char*)&bf, sizeof(BITMAPFILEHEADER), &written);
    if (written == sizeof(BITMAPFILEHEADER)) {
      stream->Write((const char*)bmi, sizeof(BITMAPINFOHEADER), &written);
      if (written == sizeof(BITMAPINFOHEADER)) {
        stream->Write((const char*)bits, length, &written);
        if (written == length)
          rv = NS_OK;
      }
    }
  
    stream->Close();
  }
  
  return rv;
}

NS_IMETHODIMP
nsWindowsHooks::SetImageAsWallpaper(nsIDOMElement* aElement, PRBool aUseBackground)
{
  nsresult rv;
  
  nsCOMPtr<gfxIImageFrame> gfxFrame;
  if (aUseBackground) {
    // XXX write background loading stuff!
  } else {
    nsCOMPtr<nsIImageLoadingContent> imageContent = do_QueryInterface(aElement, &rv);
    if (!imageContent) return rv;
    
    // get the image container
    nsCOMPtr<imgIRequest> request;
    rv = imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                  getter_AddRefs(request));
    if (!request) return rv;
    nsCOMPtr<imgIContainer> container;
    rv = request->GetImage(getter_AddRefs(container));
    if (!request) return rv;
    
    // get the current frame, which holds the image data
    container->GetCurrentFrame(getter_AddRefs(gfxFrame));
  }  
  
  if (!gfxFrame)
    return NS_ERROR_FAILURE;

  // get the windows directory ('c:\windows' usually)
  char winDir[256];
  ::GetWindowsDirectory(winDir, sizeof(winDir));
  nsAutoString winPath;
  winPath.AssignWithConversion(winDir);
  
  // get the product brand name from localized strings
  nsXPIDLString brandName;
  nsCID bundleCID = NS_STRINGBUNDLESERVICE_CID;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(bundleCID));
  if (bundleService) {
    nsCOMPtr<nsIStringBundle> brandBundle;
    rv = bundleService->CreateBundle("chrome://global/locale/brand.properties",
                                     getter_AddRefs(brandBundle));
    if (NS_SUCCEEDED(rv) && brandBundle) {
      if (NS_FAILED(rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                            getter_Copies(brandName))))
        return rv;                              
    }
  }
  
  // build the file name
  winPath.Append(NS_LITERAL_STRING("\\").get());
  winPath.Append(brandName);
  winPath.Append(NS_LITERAL_STRING(" Wallpaper.bmp").get());
  
  // write the bitmap to a file in the windows dir
  rv = WriteBitmap(winPath, gfxFrame);

  // if the file was written successfully, set it as the system wallpaper
  if (NS_SUCCEEDED(rv))
    ::SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, ToNewCString(winPath), SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);

  return rv;
}
