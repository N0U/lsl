//SpringUnitSync& usync();

#include <lslunitsync/c_api.h>
#include <lslunitsync/image.h>
#include <lslunitsync/unitsync.h>
#include <boost/format.hpp>
#include <cmath>
#include <iostream>

void dummySync()
{
	LSL::Unitsync usync;
	{
		/*const bool usync_loaded =*/ usync.ReloadUnitSyncLib();
	}
	std::cout << boost::format( "found %d maps and %d games\n") % usync.GetNumMaps() % usync.GetNumMods() ;
	LSL::UnitsyncImage mini = usync.GetMinimap( "Alaska" );
	LSL::UnitsyncImage metal = usync.GetMetalmap( "Alaska" );
	LSL::UnitsyncImage height = usync.GetHeightmap( "Alaska" );
	mini.Save( "/tmp/alaska_mini.png" );
	metal.Save( "/tmp/alaska_metal.png" );
	height.Save( "/tmp/alaska_height.png" );
	LSL::UnitsyncImage heightL( "/tmp/alaska_height.png" );
	heightL.Save( "/tmp/alaska_heightL.png" );
}

/**
Copyright 2012 by The libSpringLobby team. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/
