#include <lslunitsync/unitsync.h>


void dump(LSL::StringVector& vec){
	LSL::StringVector::iterator it;
	for(it=vec.begin(); it!=vec.end(); ++it) {
		printf("%s\n", (*it).c_str());
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s <unitsync path>\n", argv[0]);
		return 1;
	}
	LSL::usync().LoadUnitSyncLib(argv[1]);
	LSL::StringVector games = LSL::usync().GetModList();
	dump(games);
	LSL::StringVector maps = LSL::usync().GetMapList();
	dump(maps);
	//LSL::usync().
	LSL::usync().FreeUnitSyncLib();
}
