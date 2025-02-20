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

#ifndef DCPLUSPLUS_DCPP_DOWNLOADMANAGERLISTENER_H_
#define DCPLUSPLUS_DCPP_DOWNLOADMANAGERLISTENER_H_

#include "typedefs.h"
#include "noexcept.h"

namespace dcpp {

/**
 * Use this listener interface to get progress information for downloads.
 *
 * @remarks All methods are sending a pointer to a Download but the receiver
 * (TransferView) is not using any of the methods in Download, only methods
 * from its super class, Transfer. The listener functions should send Transfer
 * objects instead.
 *
 * Changing this will will cause a problem with Download::List which is used
 * in the on Tick function. One solution is reimplement on Tick to call once
 * for every Downloads, sending one Download at a time. But maybe updating the
 * GUI is not DownloadManagers problem at all???
 */
class DownloadManagerListener {
public:
	virtual ~DownloadManagerListener() { }
	template<int I>	struct X { enum { TYPE = I }; };

	typedef X<0> Complete;
	typedef X<1> Failed;
	typedef X<2> Starting;
	typedef X<3> Tick;
	typedef X<4> Requesting;
	typedef X<5> Status;
	typedef X<7> TargetChanged;
	typedef X<8> BundleWaiting;
	typedef X<9> BundleTick;

	/**
	 * This is the first message sent before a download starts.
	 * No other messages will be sent before this.
	 */
	virtual void on(Requesting, const Download*, bool /*hubChanged*/) noexcept { }

	/**
	 * This is the first message sent before a download starts.
	 */
	virtual void on(Starting, const Download*) noexcept { }

	/**
	 * Sent once a second if something has actually been downloaded.
	 */
	virtual void on(Tick, const DownloadList&) noexcept { }

	/**
	 * This is the last message sent before a download is deleted.
	 * No more messages will be sent after it.
	 */
	virtual void on(Complete, const Download*, bool) noexcept { }

	/* format: target, token, bundleToken */
	virtual void on(TargetChanged, const string&, const string&, const string&) noexcept { }

	virtual void on(BundleWaiting, const BundlePtr) noexcept { }
	virtual void on(BundleTick, const BundleList&, uint64_t) noexcept { }

	/**
	 * This indicates some sort of failure with a particular download.
	 * No more messages will be sent after it.
	 *
	 * @remarks Should send an error code instead of a string and let the GUI
	 * display an error string.
	 */
	virtual void on(Failed, const Download*, const string&) noexcept { }
	virtual void on(Status, const UserConnection*, const string&) noexcept { }
};

} // namespace dcpp

#endif /*DOWNLOADMANAGERLISTENER_H_*/
