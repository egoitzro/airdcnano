/*
 * Copyright (C) 2001-2014 Jacek Sieka, arnetheduck on gmail point com
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "HttpConnection.h"

#include "BufferedSocket.h"
#include "format.h"
#include "SettingsManager.h"
#include "version.h"

#include "ResourceManager.h"
#include "format.h"

namespace dcpp {

static const std::string CORAL_SUFFIX = ".nyud.net";

HttpConnection::HttpConnection(bool coralize, bool aIsUnique, bool aV4only /*false*/) :
port("80"),
size(-1),
done(0),
connState(CONN_UNKNOWN),
coralizeState(coralize ? CST_DEFAULT : CST_NOCORALIZE),
isUnique(aIsUnique),
socket(0),
v4only(aV4only)
{
}

HttpConnection::~HttpConnection() {
	if(socket) {
		abortRequest(true);
	}
}

/**
 * Downloads a file and returns it as a string
 * @todo Report exceptions
 * @todo Abort download
 * @param aUrl Full URL of file
 * @return A string with the content, or empty if download failed
 */
void HttpConnection::downloadFile(const string& aFile) {
	currentUrl = aFile;
	prepareRequest(TYPE_GET);
}

/**
 * Initiates a basic urlencoded form submission
 * @param aFile Fully qualified file URL
 * @param aData StringMap with the args and values
 */
void HttpConnection::postData(const string& aUrl, const StringMap& aData) {
	currentUrl = aUrl;
	coralizeState = CST_NOCORALIZE;
	requestBody.clear();

	for(StringMap::const_iterator i = aData.begin(); i != aData.end(); ++i)
		requestBody += "&" + Util::encodeURI(i->first) + "=" + Util::encodeURI(i->second);

	if (!requestBody.empty()) requestBody = requestBody.substr(1);
	prepareRequest(TYPE_POST);
}

void HttpConnection::prepareRequest(RequestType type) {
	dcassert(Util::findSubString(currentUrl, "http://") == 0 || Util::findSubString(currentUrl, "https://") == 0);
	Util::sanitizeUrl(currentUrl);

	size = -1;
	done = 0;
	connState = CONN_UNKNOWN;
	connType = type;

	// method selection
	method = (connType == TYPE_POST) ? "POST" : "GET";

	// set download type
	if(Util::stricmp(currentUrl.substr(currentUrl.size() - 4).c_str(), ".bz2") == 0) {
		mimeType = "application/x-bzip2";
	} else mimeType.clear();

	string proto, fragment;
	if(SETTING(HTTP_PROXY).empty()) {
		Util::decodeUrl(currentUrl, proto, server, port, file, query, fragment);
		if(file.empty())
			file = "/";
	} else {
		Util::decodeUrl(SETTING(HTTP_PROXY), proto, server, port, file, query, fragment);
		file = currentUrl;
	}

	if(!query.empty())
		file += '?' + query;

	if(SETTING(CORAL) && coralizeState != CST_NOCORALIZE) {
		if(server.length() > CORAL_SUFFIX.length() && server.compare(server.length() - CORAL_SUFFIX.length(), CORAL_SUFFIX.length(), CORAL_SUFFIX) !=0) {
			server += CORAL_SUFFIX;
		} else {
			coralizeState = CST_NOCORALIZE;
		}
	}

	if(port.empty())
		port = "80";

	if(!socket)
		socket = BufferedSocket::getSocket(0x0a, v4only);


	socket->addListener(this);
	try {
		socket->connect(server, port, (proto == "https"), true, false);
	} catch(const Exception& e) {
		fire(HttpConnectionListener::Failed(), this, e.getError() + " (" + currentUrl + ")");
		connState = CONN_FAILED;
		if (isUnique) delete this;
	}
}

void HttpConnection::abortRequest(bool disconnect) {
	dcassert(socket);

	socket->removeListener(this);
	if(disconnect) socket->disconnect();

	BufferedSocket::putSocket(socket);
	socket = nullptr;
}

void HttpConnection::on(BufferedSocketListener::Connected) noexcept {
	dcassert(socket);
	socket->write("GET " + file + " HTTP/1.1\r\n");
	socket->write("User-Agent: Airdcpp/" + static_cast<string>(VERSIONSTRING) + " " + Util::getOsVersion(true) + "\r\n");

	string sRemoteServer = server;
	if(!SETTING(HTTP_PROXY).empty())
	{
		string tfile, tport, proto, query, fragment;
		Util::decodeUrl(file, proto, sRemoteServer, tport, tfile, query, fragment);
	}

	socket->write("Host: " + sRemoteServer + "\r\n");
	socket->write("Connection: close\r\n");	// we'll only be doing one request
	socket->write("Cache-Control: no-cache\r\n\r\n");
	if (connType == TYPE_POST) socket->write(requestBody);
	if (coralizeState == CST_DEFAULT) coralizeState = CST_CONNECTED;
}

void HttpConnection::on(BufferedSocketListener::Line, const string& aLine) noexcept {
	if(connState == CONN_CHUNKED && aLine.size() > 1) {
		string::size_type i;
		string chunkSizeStr;
		if((i = aLine.find(";")) == string::npos) {
			chunkSizeStr = aLine.substr(0, aLine.length() - 1);
		} else chunkSizeStr = aLine.substr(0, i);

		unsigned long chunkSize = strtoul(chunkSizeStr.c_str(), NULL, 16);
		if(chunkSize == 0 || chunkSize == ULONG_MAX) {
			abortRequest(true);

			if(chunkSize == 0) {
				fire(HttpConnectionListener::Complete(), this, currentUrl, SETTING(CORAL) && coralizeState != CST_NOCORALIZE);
				connState = CONN_OK;
			} else {
				fire(HttpConnectionListener::Failed(), this, "Transfer-encoding error (" + currentUrl + ")");
				connState = CONN_FAILED;
			}

			if (isUnique) { delete this; return; }
			coralizeState = CST_DEFAULT;
		} else socket->setDataMode(chunkSize);
	} else if(connState == CONN_UNKNOWN) {
		if(aLine.find("200") != string::npos) {
			connState = CONN_OK;
		} else if(aLine.find("301") != string::npos || aLine.find("302") != string::npos) {
			connState = CONN_MOVED; 
		} else {
			abortRequest(true);

			if(SETTING(CORAL) && coralizeState != CST_NOCORALIZE) {
				fire(HttpConnectionListener::Retried(), this, coralizeState == CST_CONNECTED);		
				coralizeState = CST_NOCORALIZE;
				dcdebug("HTTP error with Coral, retrying : %s\n",currentUrl.c_str());
				downloadFile(currentUrl);
				return;
			}
		
			fire(HttpConnectionListener::Failed(), this, str(boost::format("%1% (%2%)") % aLine % currentUrl));
			if (isUnique) { delete this; return; }
			connState = CONN_FAILED;
			coralizeState = CST_DEFAULT;
		}
	} else if(connState == CONN_MOVED && Util::findSubString(aLine, "Location") != string::npos) {
		abortRequest(true);

		string location = aLine.substr(10, aLine.length() - 10);
		Util::sanitizeUrl(location);

		// make sure we can also handle redirects with relative paths
		if(location.find("://") == string::npos) {
			if(location[0] == '/') {
				//302 doesn't contain the query, use temp one
				string proto, queryTmp, fragment;
				Util::decodeUrl(currentUrl, proto, server, port, file, queryTmp, fragment);
				string tmp = proto + "://" + server;
				if(port != "80" || port != "443")
					tmp += ':' + port;
				location = tmp + location;
			} else {
				string::size_type i = currentUrl.rfind('/');
				dcassert(i != string::npos);
				location = currentUrl.substr(0, i + 1) + location;
			}
		}

		if(location == currentUrl) {
			connState = CONN_FAILED;
			fire(HttpConnectionListener::Failed(), this, STRING_F(ENDLESS_REDIRECTION_LOOP, currentUrl));
			if (isUnique) delete this;
			return;
		}

		if (!query.empty())
			location += "?" + query;
		fire(HttpConnectionListener::Redirected(), this, location);

		if (coralizeState != CST_NOCORALIZE)
			coralizeState = CST_DEFAULT;

		downloadFile(location);
	} else if(aLine[0] == 0x0d) {
		if(size != -1) {
			socket->setDataMode(size);
		} else connState = CONN_CHUNKED;
	} else if(Util::findSubString(aLine, "Content-Length") != string::npos) {
		size = Util::toInt(aLine.substr(16, aLine.length() - 17));
	} else if(mimeType.empty()) {
		if(Util::findSubString(aLine, "Content-Encoding") != string::npos) {
			if(aLine.substr(18, aLine.length() - 19) == "x-bzip2")
				mimeType = "application/x-bzip2";
		} else if(Util::findSubString(aLine, "Content-Type") != string::npos) {
			mimeType = aLine.substr(14, aLine.length() - 15);
		}
	}
}

void HttpConnection::on(BufferedSocketListener::Failed, const string& aLine) noexcept {
	abortRequest(false);
	if(SETTING(CORAL) && coralizeState != CST_NOCORALIZE) {
		fire(HttpConnectionListener::Retried(), this, coralizeState == CST_CONNECTED);
		coralizeState = CST_NOCORALIZE;
		dcdebug("Coralized address failed, retrying : %s\n",currentUrl.c_str());
		downloadFile(currentUrl);
		return;
	}

	connState = CONN_FAILED;
	coralizeState = CST_DEFAULT;
	fire(HttpConnectionListener::Failed(), this, str(boost::format("%1% (%2%)") % aLine % currentUrl));
	if (isUnique) delete this;
}

void HttpConnection::on(BufferedSocketListener::ModeChange) noexcept {
	if(connState != CONN_CHUNKED) {
		abortRequest(true);

		fire(HttpConnectionListener::Complete(), this, currentUrl, SETTING(CORAL) && coralizeState != CST_NOCORALIZE);
		if (isUnique) { delete this; return; }
		coralizeState = CST_DEFAULT;
	}
}
void HttpConnection::on(BufferedSocketListener::Data, uint8_t* aBuf, size_t aLen) noexcept {
	if(size != -1 && static_cast<size_t>(size - done)  < aLen) {
		abortRequest(true);

		connState = CONN_FAILED;
		coralizeState = CST_DEFAULT;
		fire(HttpConnectionListener::Failed(), this, "Too much data in response body (" + currentUrl + ")");
		if (isUnique) delete this;
		return;
	}

	fire(HttpConnectionListener::Data(), this, aBuf, aLen);
	done += aLen;
}

} // namespace dcpp
