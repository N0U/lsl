/**
	This file is part of SpringLobby,
	Copyright (C) 2007-2011

	SpringLobby is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License version 2 as published by
	the Free Software Foundation.

	SpringLobby is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SpringLobby.  If not, see <http://www.gnu.org/licenses/>.
**/


#ifndef LSL_HEADERGUARD_MRU_CACHE_H
#define LSL_HEADERGUARD_MRU_CACHE_H

#include <utils/debug.h>

#include <string>
#include <boost/thread/mutex.hpp>
#include <list>
#include <map>

namespace LSL {

/// Thread safe MRU cache (works like a std::map but has maximum size)
template<typename TKey, typename TValue>
class MostRecentlyUsedCache
{
public:
	//! name parameter might be used to identify stats in dgb output
	MostRecentlyUsedCache(int max_size, const std::string& name = "" )
		: m_size(0),
		m_max_size(max_size),
		m_cache_hits(0),
		m_cache_misses(0),
		m_name(name)
	{
	}

	~MostRecentlyUsedCache()
	{
		LslDebug( "%s - cache hits: %d", m_name, m_cache_hits );
		LslDebug( "%s - cache misses: %d", m_name, m_cache_misses );
	}

	void Add( const TKey& name, const TValue& img )
	{
		boost::mutex::scoped_lock lock(m_lock);
		while ( m_size >= m_max_size ) {
			--m_size;
			m_iterator_map.erase( m_items.back().first );
			m_items.pop_back();
		}
		++m_size;
		m_items.push_front( CacheItem( name, img ) );
		m_iterator_map[name] = m_items.begin();
	}

	bool TryGet( const TKey& name, TValue& img )
	{
		boost::mutex::scoped_lock lock(m_lock);
		typename IteratorMap::iterator it = m_iterator_map.find( name );
		if ( it == m_iterator_map.end() ) {
			++m_cache_misses;
			return false;
		}
		// reinsert at front, so that most recently used items are always at front
		m_items.push_front( *it->second );
		m_items.erase( it->second );
		it->second = m_items.begin();
		// return image
		img = it->second->second;
		++m_cache_hits;
		return true;
	}

	void Clear()
	{
		boost::mutex::scoped_lock lock(m_lock);
		m_size = 0;
		m_items.clear();
		m_iterator_map.clear();
	}

private:
	typedef std::pair<TKey, TValue> CacheItem;
	typedef std::list<CacheItem> CacheItemList;
	typedef std::map<TKey, typename CacheItemList::iterator> IteratorMap;

	mutable boost::mutex m_lock;
	CacheItemList m_items;
	IteratorMap m_iterator_map;
	int m_size;
	const int m_max_size;
	int m_cache_hits;
	int m_cache_misses;
	const std::string m_name;
};

class UnitsyncImage;
class MapInfo;
typedef MostRecentlyUsedCache<std::string,UnitsyncImage> MostRecentlyUsedImageCache;
typedef MostRecentlyUsedCache<std::string,MapInfo> MostRecentlyUsedMapInfoCache;
typedef MostRecentlyUsedCache<std::string,std::vector<std::string> > MostRecentlyUsedArrayStringCache;

} // namespace LSL

#endif // MRU_CACHE_H
