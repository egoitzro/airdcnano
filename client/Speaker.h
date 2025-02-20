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

#ifndef DCPLUSPLUS_DCPP_SPEAKER_H
#define DCPLUSPLUS_DCPP_SPEAKER_H

#include <boost/range/algorithm/find.hpp>
#include <utility>
#include <vector>

#include "CriticalSection.h"

#include "noexcept.h"

namespace dcpp {

using std::forward;
using std::vector;
using boost::range::find;

template<typename Listener>
class Speaker {
	typedef vector<Listener*> ListenerList;

public:
	Speaker() noexcept { }
	virtual ~Speaker() { }

	template<typename... ArgT>
	void fire(ArgT&&... args) noexcept {
		Lock l(listenerCS);
		tmp = listeners;
		for(auto listener: tmp) {
			listener->on(forward<ArgT>(args)...);
		}
	}

	void addListener(Listener* aListener) {
		Lock l(listenerCS);
		if(find(listeners, aListener) == listeners.end())
			listeners.push_back(aListener);
	}

	void removeListener(Listener* aListener) {
		Lock l(listenerCS);
		auto it = find(listeners, aListener);
		if(it != listeners.end())
			listeners.erase(it);
	}

	void removeListeners() {
		Lock l(listenerCS);
		listeners.clear();
	}
	
protected:
	ListenerList listeners;
	ListenerList tmp;
	CriticalSection listenerCS;
};

} // namespace dcpp

#endif // !defined(SPEAKER_H)