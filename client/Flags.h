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

#ifndef DCPLUSPLUS_DCPP_FLAGS_H_
#define DCPLUSPLUS_DCPP_FLAGS_H_

#include <stdint.h>

namespace dcpp {

class Flags {
	public:
		typedef uint32_t MaskType;

		class IsSet {
		public:
			IsSet(MaskType compareTo) : a(compareTo) {}

			template<typename T>
			bool operator()(const T& item) { return item->isSet(a); }
		private:
			MaskType a;
		};

		Flags() : flags(0) { }
		Flags(MaskType f) : flags(f) { }
		bool isSet(MaskType aFlag) const { return (flags & aFlag) == aFlag; }
		bool isAnySet(MaskType aFlag) const { return (flags & aFlag) != 0; }
		void setFlag(MaskType aFlag) { flags |= aFlag; }
		void unsetFlag(MaskType aFlag) { flags &= ~aFlag; }
		MaskType getFlags() const { return flags; }
private:
	MaskType flags;
};

} // namespace dcpp

#endif /*FLAGS_H_*/
