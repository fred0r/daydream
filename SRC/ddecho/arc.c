/* 
 * DD-Echo
 * Routines for archiving 
 * Author: Bo Simonsen <bo@geekworld.dk> 
 */

#include "headers.h"

int Arc() {
	bsListItem* item;

	char cmd[1024];  /* Increased buffer size for command */
	char flofn[64];
	char arcfn[64];
	char farcfn[128];
	char buf[64];
	
	ArchList* arc;

	const char* ext[] = {"su", "mo", "tu", "we", "th", "fr", "sa"};
	const char eext[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	               'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
	               'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
	               'u', 'w', 'v', 'y', 'z'};
	const int eext_len = 35;

	struct tm* tmptm;
	time_t tmptime;
	
	struct stat st;
	int i;
	
	int new_file;

	for(item=arc_list.first; item; item=item->next) {
		arc = item->val;

		tmptime = time(NULL);
		tmptm = localtime(&tmptime);

		snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d%d%d%d%d", 
                        arc->Dest.zone, arc->Dest.net,
                        arc->Dest.node, arc->Dest.point,
                        arc->Source.zone, arc->Source.net,
                        arc->Source.node, arc->Source.point,
                        tmptm->tm_mday, tmptm->tm_mon, tmptm->tm_year);
                        
		snprintf(arcfn, sizeof(arcfn), "%08x", StringCRC32(buf));	
	
		new_file = TRUE;
		
		for(i=0; i < eext_len; i++) {

			snprintf(farcfn, sizeof(farcfn), "%s/%s.%s%c", GetDirName(&arc->Dest), arcfn, ext[tmptm->tm_wday], eext[i]);
			
			if(stat(farcfn, &st) < 0) {
			    break;
			}
			
			if(st.st_size < main_cfg.MaxArcSize) {
			    new_file = FALSE;
			    break;
			}
		}

		logit(TRUE, "Packing for %s\n", fidoaddr_to_text(&arc->Dest, buf));
		GetFloName(flofn, &arc->Dest, arc->Flavour);

		if(arc->Packer) {
 	 		snprintf(cmd, sizeof(cmd), arc->Packer->Compress, farcfn, arc->Filename);
			logit(TRUE, "Executing %s\n", cmd);

			if(!system(cmd)) {
				if(new_file) {
	    				AppendToFlo(flofn, farcfn);
	    			}
				unlink(arc->Filename);
			} 
			else {
				AppendToFlo(flofn, arc->Filename);
			}
		}
		else {
			AppendToFlo(flofn, arc->Filename);
		}
	}

	bsList_free(&arc_list);

	return 0;
}

char FlavToChar(int flo, Flav Flavour) {
	switch(Flavour) {
		case HOLD:
			return 'h';
		case CRASH:
			return 'c';
		default:
			if(flo) {
				return 'f';
			}
			return 'o';
	}
}

char* GetDirName(FidoAddr* dest) {
	static char buf[256];
	static char buf2[512];

	if(dest->zone != main_cfg.DefZone) {
		snprintf(buf, sizeof(buf), "%s.%03x", main_cfg.Outbound, dest->zone);
		if(access(buf, R_OK) < 0) {
			mkdir(buf, MKDIR_DEFS);
		}
	} else {
		strncpy(buf, main_cfg.Outbound, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
	}

	if(dest->point != 0) {
		snprintf(buf2, sizeof(buf2), "%s/%04x%04x.pnt", buf, dest->net, dest->node);
		if(access(buf2, R_OK) < 0) {
			mkdir(buf2, MKDIR_DEFS);
		}
		return buf2;
	} 
	else return buf;
}

char* GetBaseName(FidoAddr* dest) {
	static char buf[512];
	char* dirname;

	dirname = GetDirName(dest);

	if(dest->point != 0) {
		snprintf(buf, sizeof(buf), "%s/%08x", dirname, dest->point);	
	}
	else {
		snprintf(buf, sizeof(buf), "%s/%04x%04x", dirname, dest->net, dest->node);	
	}
	
	return buf;
}

void GetPktName(char* pkt, FidoAddr* dest, Flav Flavour, int netmail) {

	Packer* packer;
	ArchList* arch;
	bsListItem* item;
	int found = 0;
	char buf[64];
	struct stat st;
	
	packer = GetPacker(dest);

	if(netmail && !main_cfg.PackNetmail) {
		snprintf(pkt, 256, "%s.%cut", GetBaseName(dest), FlavToChar(0, Flavour));
		return;
	} 

	for(item=arc_list.first; item; item=item->next) {

		arch = item->val;

		if (fidoaddr_compare(&arch->Dest, dest)) {
			if(stat(arch->Filename, &st) < 0) {
				logit(TRUE, "Something wierd happended");
			}
			
			if(st.st_size < main_cfg.MaxPktSize) {
			    strcpy(pkt, arch->Filename);
			    found = 1;
			    break;
			}
		}
	}

	if(!found) {

		RandomFilename(buf, "pkt");
		snprintf(pkt, 256, "%s/%s", main_cfg.Outbound, buf);

		arch = NEW(ArchList);

		strncpy(arch->Filename, pkt, sizeof(arch->Filename)-1);
		arch->Filename[sizeof(arch->Filename)-1] = '\0';
		memcpy(&arch->Dest, dest, sizeof(FidoAddr));
		FindMyAka(&arch->Source, &arch->Dest);
		arch->Packer = packer;
		arch->Flavour = Flavour;
	
		bsList_add(&arc_list, arch);
	}
}

void GetFloName(char* buf, FidoAddr* dest, Flav Flavour) {
	snprintf(buf, 256, "%s.%clo", GetBaseName(dest), FlavToChar(1, Flavour));
}

void AppendToFlo(char* floname, char* file) {
	FILE* fh;

	fh = fopen(floname, "a");
	fprintf(fh, "^%s\n", file);
	fclose(fh);
}


