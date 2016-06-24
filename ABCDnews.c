/*
******************************************************************************************************************
rank: For knowing rank of a process
sourceToReporter: For knowing the source which would send the reporter news
sourceBlock: The block in the source news to be accessed by the reporter
reporters,sources,editors: Number of reporters, sources and editors respectively in the system
entities: Total number of processes in the system
blockOffset, sourceOffst: To store the random number generated by the drand calls
MPI_Comm_split: To split the reporters and sources together into separate communicator
threshold: Used in calculating the editor to which a particular eporter reports
editorToSend: The editor id [0,editors-1] to whom reporter sends news
members: Temporary variable to create groups:- stores no of reporters reporting to an editor
countRep: Used to extract rank of a reporter for creating groupRank array
groupRanks: 2-d array to store ranks of reporters per editor
repGroup: Array of MPI_Group
repComm: Array of MPI_Comm
commMembers: Gives the members in the corresponding communicator of a reporter(group which reports to a praticular editor)
repbComm: Stores the index of communicator to which a reporter belongs
category: Array to store category received from other reporters through broadcast
blockId: Array to receive block id of news received from other reporters through broadcast
******************************************************************************************************************
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function.h"
#define NEWS_SIZE 4096
#define BLOCK_SIZE 512

typedef struct ds{
	char news[BLOCK_SIZE];
	int category;
	int blockId;
}dataStruct;

int main(int argc, char* argv[]){

	int rank,sourceToReporter,sourceBlock;
	int reporters,sources,editors;
	char news[4096];
	int entities,members,i,j,countRep,repbComm=-1,commMembers,commRank;	
	int loopFlag=1, initFlag=1,fd,epoch=0;
	double blockOffset, sourceOffset;
	int** groupRanks;
	struct drand48_data rbuff_source, rbuff_offset;
	FILE* fp;
	sources = atoi(argv[2]);
	reporters = atoi(argv[3]);
	editors = atoi(argv[4]);
	groupRanks = (int**)malloc(sizeof(int*)*editors);
	countRep=sources;
	MPI_Win win;
	MPI_Comm commGroup,repComm[editors];
	MPI_Group worldGroup, repGroup[editors];
	MPI_Datatype dataMPI, oldTypes[2];

	MPI_Init(&argc, &argv);

	//initiazing flags for while loop
	int active[sources];
	for(i=0;i<sources;i++)
		active[i]=1;

	//creating a derived datatype
	oldTypes[0] = MPI_CHAR;
	oldTypes[1] = MPI_INT;
	int blockCount[2];
	MPI_Aint offset[2],extent;
	blockCount[0]=BLOCK_SIZE;
	blockCount[1]=2;
	MPI_Type_extent(MPI_CHAR, &extent);
	offset[0]=0;
	offset[1]=BLOCK_SIZE*extent;
	MPI_Type_struct(2, blockCount, offset, oldTypes, &dataMPI);
	MPI_Type_commit(&dataMPI);


	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD,&entities);

	//creating groups using split
	MPI_Comm_split(MPI_COMM_WORLD, rank<(sources+reporters), rank, &commGroup);		
	MPI_Comm_group(MPI_COMM_WORLD, &worldGroup);
	for(i=0;i<editors;i++){
		members = atoi(argv[5+i]);
		groupRanks[i] = (int*)malloc(sizeof(int)*members);
		for(j=0;j<members;j++){
			groupRanks[i][j] = countRep;
			if(rank==countRep){
				repbComm = i;
				commMembers = members;
			}
			countRep++;
		}
		MPI_Group_incl(worldGroup, members, groupRanks[i], &repGroup[i]);
		MPI_Comm_create(MPI_COMM_WORLD, repGroup[i], &repComm[i]);
	}
	if(entities!=atoi(argv[1])){
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	while(loopFlag){
		if(rank>=sources+reporters){
			if(initFlag){
				char filename[20]="editor";
				sprintf(filename,"editor%d.txt",rank-sources-reporters);
				fp=fopen(filename,"w");
				initFlag=0;
				function1();
			}
			fprintf(fp,"EPOCH: %d\n",epoch++);
		}
		loopFlag=0;
		if(rank<sources){
			if(initFlag)
				fp = fopen(argv[rank+5+editors],"r");  
			if(active[rank]){
				int bytesRead = fread(news, sizeof(news[0]), NEWS_SIZE, fp);
				if(bytesRead<NEWS_SIZE){

					active[rank]=0;
					int start=bytesRead/BLOCK_SIZE;
					char latestNews[BLOCK_SIZE];
					fseek(fp,-BLOCK_SIZE,SEEK_END);
					bytesRead = fread(latestNews, sizeof(news[0]), BLOCK_SIZE, fp);
					while(bytesRead<BLOCK_SIZE){
						latestNews[bytesRead++]=' ';
					}
					while(start<NEWS_SIZE/BLOCK_SIZE){
						memcpy( &news[start*BLOCK_SIZE] , latestNews, BLOCK_SIZE-1);
						start++;
					}
				}
				if(initFlag){
					MPI_Win_create(news, NEWS_SIZE, sizeof(char), MPI_INFO_NULL,
								commGroup, &win);
					MPI_Win_fence(0, win);
					initFlag=0;
				}
			}
			MPI_Win_fence(0, win);
		}
		else if(rank<(sources+reporters)){
			if(initFlag){
				MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, 
								commGroup, &win);
				MPI_Win_fence(0, win);
				srand48_r(1202107158 + rank*1999, &rbuff_offset);
				srand48_r(1202107158 + rank*1999, &rbuff_source);
				initFlag=0;
			}
			drand48_r(&rbuff_source, &sourceOffset);
			sourceToReporter = (int)(sourceOffset*sources);
			while(!active[sourceToReporter]){
				drand48_r(&rbuff_source, &sourceOffset);
				sourceToReporter = (int)(sourceOffset*sources);
			}	
			drand48_r(&rbuff_offset, &blockOffset);
			sourceBlock = (int)(blockOffset*(NEWS_SIZE/BLOCK_SIZE));		
			MPI_Get(news, NEWS_SIZE, MPI_CHAR, sourceToReporter,
					sourceBlock*BLOCK_SIZE, BLOCK_SIZE, MPI_CHAR, win);
			MPI_Win_fence(0, win);
			int category[commMembers],blockId[commMembers],sendFlag=1;
			MPI_Comm_rank(repComm[repbComm], &commRank);
			MPI_Allgather(&sourceToReporter, 1, MPI_INT, category, 1, MPI_INT, repComm[repbComm]);
			MPI_Allgather(&sourceBlock, 1, MPI_INT, blockId, 1, MPI_INT, repComm[repbComm]);
			for(i=0;i<commMembers;i++){
				if(category[i]==sourceToReporter && blockId[i]>=sourceBlock){
					if(blockId[i]==sourceBlock){
						if(commRank<i)
							sendFlag=0;
					}
					else{
						sendFlag=0;
					}
				}
			}
			int repBuffer[BLOCK_SIZE];
			MPI_Buffer_attach(&repBuffer, BLOCK_SIZE);
			MPI_Request req1,req2;
			MPI_Status stat1;
			MPI_Ibsend(&sendFlag, 1, MPI_INT, repbComm+sources+reporters, 0, MPI_COMM_WORLD,&req1);
			MPI_Wait(&req1, &stat1);
			if(sendFlag==1){
				dataStruct repMsg;
				repMsg.category = sourceToReporter;
				repMsg.blockId = sourceBlock;
				memcpy(repMsg.news, news, BLOCK_SIZE-1);
				repMsg.news[BLOCK_SIZE-1]='\0';
				MPI_Send(&repMsg, 1, dataMPI, repbComm+sources+reporters, 1, MPI_COMM_WORLD);
			}
		}
		else{
			int numNews = atoi(argv[rank+5-sources-reporters]);
			int sendFlag[numNews];
			MPI_Status stat[numNews];
			int numReports=0, totalReports;
			for(i=0;i<numNews;i++){
				MPI_Recv(&sendFlag[i], 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &stat[i]);
				if(sendFlag[i]==1)
					numReports++;
			}
			dataStruct repData[numReports];
			int blockId[sources],totalBlockId[sources*editors];
			int newsId[sources];
			memset(blockId, -1, sizeof(blockId));
			for(i=0;i<numReports;i++){
				MPI_Recv(&repData[i], 1, dataMPI, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &stat[i]);
				blockId[repData[i].category]=repData[i].blockId;
				newsId[repData[i].category]=i;
			}
			MPI_Allgather(blockId, sources, MPI_INT, totalBlockId, sources, MPI_INT, commGroup);
			int commRank;
			MPI_Comm_rank(commGroup, &commRank);
			for(i=0;i<sources;i++){
				int flag=1;
				if(blockId[i]==-1){
					flag=0;
					continue;
				}
				for(j=0;j<editors;j++){
					if(totalBlockId[j*sources+i]>blockId[i])
						flag=0;
					if(totalBlockId[j*sources+i]==blockId[i] && commRank<j)
						flag=0;
				}
				if(flag){
					repData[i].news[BLOCK_SIZE-1]='\0';
					fprintf(fp,"EDITOR %d Publishing News From Source %d:\n%s\n\n\n", rank-sources-reporters,i,repData[newsId[i]].news);
				}
			}
		}
		MPI_Barrier(MPI_COMM_WORLD);
		for(i=0;i<sources;i++){
			MPI_Bcast(&active[i], 1, MPI_INT, i, MPI_COMM_WORLD);
			if(active[i]>0){
				loopFlag=1;
			}
		}
	}
	if(rank<sources+reporters){
		MPI_Win_free(&win);
		if(rank<sources)
			fclose(fp);
	}
	else{
		fclose(fp);
	}
	MPI_Finalize();
	return 0;
}
