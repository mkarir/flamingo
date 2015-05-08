//AT&T 12.0.1.63 


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bgpdump_lib.h"


int main(int argc, char **argv){
	unsigned int peerip, srcip, originas, i, j=0;
	char buffer[100];
	struct in_addr tmpaddr;
	FILE * outfile;
	BGPDUMP *my_dump;
    BGPDUMP_ENTRY *my_entry = NULL;
	BGPDUMP_MRTD_TABLE_DUMP *dump;
	struct aspath *asp;
	

	if(argc != 4){
		printf("Usage: mrt2txt <input filename> <output filename> <peer ip>\n");
		exit(0);
	}

	my_dump = bgpdump_open_dump(argv[1]);
	
    if(my_dump==NULL) {
		printf("Error opening dump file ...\n");
		exit(1);
	}

	outfile = fopen(argv[2], "wt");
	
	if(outfile == NULL){
		printf("Error output dump file ...\n");
		exit(1);
	}

	if( inet_aton(argv[3], &tmpaddr) == 0){
		printf("Invalid peer ip ...\n");
		exit(1);
	}

	peerip = ntohl((unsigned int)tmpaddr.s_addr);

	printf("processing.");

	do{
		if((j++ % 350000) == 0){
			printf(".");
			fflush (stdout); 
		}

		//printf("Offset: %d\n", gztell(my_dump->f));
		my_entry = bgpdump_read_next(my_dump);
	
		if(my_entry != NULL){
			

			if(my_entry->type == BGPDUMP_TYPE_MRTD_TABLE_DUMP && my_entry->subtype == BGPDUMP_SUBTYPE_MRTD_TABLE_DUMP_AFI_IP){
				dump = &(my_entry->body.mrtd_table_dump);
				asp = my_entry->attr->aspath;

				srcip = ntohl((unsigned int)(dump->peer_ip.v4_addr.s_addr));

				if(srcip == peerip){		
					for(i=0, originas = atoi(strtok( asp->str, " ")); i < asp->count-1; i++)
						originas = atoi(strtok( 0, " "));

					tmpaddr.s_addr = dump->prefix.v4_addr.s_addr;
					memset(buffer, 0, 100);
					sprintf(buffer, "%s/%d %d\n", inet_ntoa(tmpaddr), dump->mask, originas);
					fwrite(buffer, 1, strlen(buffer), outfile);

				}
			}


			bgpdump_free_mem(my_entry);
		}
	}while(my_dump->eof==0);

	printf("completed\n");

	fclose(outfile);	
	bgpdump_close_dump(my_dump);

	return 0;
}

