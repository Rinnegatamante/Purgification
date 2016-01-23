#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <3ds.h>
#include "khax.h"

#define MAX_MESSAGE 0x1780

Handle newsHandle;
bool openedNews = false;
bool skipPurge = false;
PrintConsole topConsole;
PrintConsole botConsole;
u32 tot;

typedef struct {
	bool dataSet;
	bool unread;
	bool enableJPEG;
	bool isSpotPass;
	bool isOptedOut;
	u8 unkData[3];
	u64 programID;
	u8 unkData2[8];
	u64 jumpParam;
	u8 unkData3[8];
	u64 datetime;
	u16 title[32];
} NewsHeader;

void utf2ascii(char* dst, u16* src){
	if(!src || !dst)return;
	while(*src)*(dst++)=(*(src++))&0xFF;
	*dst=0x00;
}

Result NEWS_GetTotalNotifications(u32* num)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x5,0,0);
	
	if(R_FAILED(ret = svcSendSyncRequest(newsHandle))) return ret;
	
	*num = cmdbuf[2];
	return (Result)cmdbuf[1];
}

Result NEWS_SetNotificationHeader(u32 id, NewsHeader header)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x7,2,2);
	cmdbuf[1] = id;
	cmdbuf[2] = sizeof(NewsHeader);
	cmdbuf[3] = IPC_Desc_Buffer(sizeof(NewsHeader),IPC_BUFFER_R);
	cmdbuf[4] = (u32)&header;
	
	if(R_FAILED(ret = svcSendSyncRequest(newsHandle))) return ret;
	return (Result)cmdbuf[1];	
}

Result NEWS_GetNotificationImage(u32 id, u8* buffer, u32* size)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0xD,2,2);
	cmdbuf[1] = id;
	cmdbuf[2] = 0x10000;
	cmdbuf[3] = IPC_Desc_Buffer((size_t)0x10000,IPC_BUFFER_W);
	cmdbuf[4] = (u32)buffer;
	
	if(R_FAILED(ret = svcSendSyncRequest(newsHandle))) return ret;
	*size = cmdbuf[2];
	return (Result)cmdbuf[1];	
}

Result NEWS_GetNotificationMessage(u32 id, u16* message)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0xC,2,2);
	cmdbuf[1] = id;
	cmdbuf[2] = 0x1780;
	cmdbuf[3] = IPC_Desc_Buffer((size_t)0x1780,IPC_BUFFER_W);
	cmdbuf[4] = (u32)message;
	
	if(R_FAILED(ret = svcSendSyncRequest(newsHandle))) return ret;
	return (Result)cmdbuf[1];
}

Result NEWS_GetNotificationHeader(u32 id, NewsHeader* header)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0xB,2,2);
	cmdbuf[1] = id;
	cmdbuf[2] = sizeof(NewsHeader);
	cmdbuf[3] = IPC_Desc_Buffer(sizeof(NewsHeader),IPC_BUFFER_W);
	cmdbuf[4] = (u32)header;
	
	if(R_FAILED(ret = svcSendSyncRequest(newsHandle))) return ret;
	return (Result)cmdbuf[1];
}

struct NewsList{
	u32 delete_id;
	char title[64];
	bool hasImage;
	NewsList* next;
};

void exit(){

	while (!(hidKeysDown() & KEY_A)) hidScanInput();
	
	// killing services
	printf("Exiting...");
	
}

NewsList* createVoice(u32 id){
	NewsList* voice = (NewsList*)malloc(sizeof(NewsList));
	NewsHeader header = { 0 };
	Result ret = NEWS_GetNotificationHeader(id, &header);
	utf2ascii(voice->title, header.title);
	voice->next = NULL;
	voice->delete_id = id;
	voice->hasImage = header.enableJPEG;
	return voice;
}

void closeNews(){
	if (!skipPurge){
		consoleSelect(&topConsole);
		consoleClear();
		if (openedNews) openedNews = false;
		else printf("* Purgification v.1.1 *\n\nA: Open News\nY: Dump News\nX: Delete News\nSELECT: Delete All News\nSTART: Exit Homebrew");
		consoleSelect(&botConsole);
	}else skipPurge = false;
}

void printMenu(int idx, NewsList* list){
	closeNews();
	consoleClear();
	int i = 0;
	while (i < idx){
		list = list->next;
		i++;
	}
	i = 0;
	while (list != NULL){
		if (i > 25) break;
		else if (i == 0) printf(">>");
		printf("%s\n",list->title);
		list = list->next;
		i++;
	}
}

void purge(NewsList* list){
	printf("Clearing notifications database...\n");
	int errors = 0;
	NewsHeader header = { 0 };
	while (list != NULL){
		Result ret = NEWS_SetNotificationHeader(list->delete_id, header);
		if (ret != 0) errors++;
		list = list->next;
	}
	printf("%i notifications deleted!\n",tot - errors);	
}

void closeServices(){
	hidExit();
	gfxExit();
	fsExit();
}

void openNews(u32 id){
	openedNews = true;
	closeNews();
	consoleSelect(&topConsole);
	u16 tmp[MAX_MESSAGE];
	NEWS_GetNotificationMessage(id, tmp);
	char message[MAX_MESSAGE * 2];
	utf2ascii(message, tmp);
	printf(message);
	consoleSelect(&botConsole);
}

NewsList* deleteNews(u32 id, NewsList* news){
	NewsList* ret = news;
	NewsList* tmp = news;
	if (id > 0){
		int i = 1;
		while (i < id){
			tmp = tmp->next;
			i++;
		}
		NewsList* tmp2 = tmp;
		tmp = tmp->next;
		tmp2->next = tmp->next;
	}else ret = ret->next;
	NewsHeader header = { 0 };
	NEWS_SetNotificationHeader(tmp->delete_id, header);
	free(tmp);
	closeNews();
	consoleSelect(&topConsole);
	printf("\n\n\nNews deleted successfully!");
	consoleSelect(&botConsole);
	return ret;
}	
	
void dumpNews(u32 id, NewsList* news){

	// getting selected news
	int i = 0;
	while (i < id){
		news = news->next;
		i++;
	}
	
	// generating filenames
	u64 time = (osGetTime() / 1000) % 86400;
	u8 hours = time / 3600;
	u8 minutes = (time % 3600) / 60;
	u8 seconds = time % 60;
	char filename[32];
	char filename2[32];
	sprintf(filename,"/%u_%u_%u_newsdump.txt",hours,minutes,seconds);
	sprintf(filename2,"/%u_%u_%u_newsdump.mpo",hours,minutes,seconds);
	
	// getting message
	u16 tmp[MAX_MESSAGE];
	NEWS_GetNotificationMessage(news->delete_id, tmp);
	char message[MAX_MESSAGE * 2];
	utf2ascii(message, tmp);
	
	// writing message file
	u32 bytes;
	Handle fileHandle;
	FS_Archive sdmcArchive=(FS_Archive){ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (u8*)""}};
	FS_Path filePath=fsMakePath(PATH_ASCII, filename);
	FSUSER_OpenFileDirectly( &fileHandle, sdmcArchive, filePath, FS_OPEN_WRITE | FS_OPEN_CREATE, 0x00000000);
	FSFILE_Write(fileHandle, &bytes, 0, message, strlen(message), 0);
	FSFILE_Close(fileHandle);
	svcCloseHandle(fileHandle);
	
	// writing image file if exists
	if (news->hasImage){
		u32 size;
		Handle fileHandle2;
		u8* buffer = (u8*)malloc(0x20000);
		Result ret = NEWS_GetNotificationImage(news->delete_id, buffer, &size);
		FS_Path filePath2=fsMakePath(PATH_ASCII, filename2);
		FSUSER_OpenFileDirectly( &fileHandle2, sdmcArchive, filePath2, FS_OPEN_WRITE | FS_OPEN_CREATE, 0x00000000);
		FSFILE_Write(fileHandle2, &bytes, 0, buffer, size, 0);
		FSFILE_Close(fileHandle2);
		svcCloseHandle(fileHandle2);
		free(buffer);
	}

	// updating top screen
	closeNews();
	consoleSelect(&topConsole);
	printf("\n\n\nNews dumped successfully!");
	consoleSelect(&botConsole);
	
}

int main(){

	// Start basic stuffs
	gfxInit(GSP_RGB565_OES,GSP_RGB565_OES,false);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSet3D(false);
	consoleInit(GFX_TOP, &topConsole);
	consoleInit(GFX_BOTTOM, &botConsole);
	consoleSelect(&botConsole);
	khaxInit();
	consoleClear();
	hidInit();
	fsInit();
	
	// initializing news:s
	Result ret = srvGetServiceHandle(&newsHandle, "news:s");
	if (R_FAILED(ret)){
		printf("An error occurred...\nPress A to exit...\n\n\n");
		exit();
		closeServices();
		return -1;
	}else printf("news:s successfully initialized...\n");
		
	// launching syscall (GetTotalNotifications)
	NEWS_GetTotalNotifications(&tot);
	printf("Detected %u notifications...\n",tot);

	// exiting if any notification is detected
	if (tot <= 0){
		printf("Press A to exit...\n\n\n");
		exit();
		closeServices();
		return 0;
	}
	
	// getting news titles
	int i = 1;
	NewsList* news = createVoice(0);
	NewsList* list = news;
	while (i < tot){
		NewsList* voice = createVoice(i);
		list->next = voice;
		list = list->next;
		i++;
	}
	
	// initializing browser
	int idx = 0;
	printMenu(idx, news);
	bool callPurge = false;
	
	// main loop
	while(tot > 0){
		
		// checking if a relevant button is pressed
		hidScanInput();
		if ((hidKeysDown() & KEY_DUP) == KEY_DUP){ // Navigate Up
			idx--;
			if (idx < 0) idx = tot - 1;
			printMenu(idx, news);
		}else if ((hidKeysDown() & KEY_DDOWN) == KEY_DDOWN){ // Navigate Down
			idx++;
			if (idx >= tot) idx = 0;
			printMenu(idx, news);
		}else if ((hidKeysDown() & KEY_START) == KEY_START){ // Exit Homebrew
			break;
		}else if ((hidKeysDown() & KEY_SELECT) == KEY_SELECT){ // Purge News
			callPurge = true;
			break;
		}else if ((hidKeysDown() & KEY_A) == KEY_A){ // Open News
			openNews(idx);
		}else if ((hidKeysDown() & KEY_Y) == KEY_Y){ // Dump News
			dumpNews(idx, news);
		}else if ((hidKeysDown() & KEY_X) == KEY_X){ // Delete News
			news = deleteNews(idx, news);
			tot--;
			if (idx >= tot) idx--;
			skipPurge = true;
			printMenu(idx, news);
		}
		
		// wait a bit
		svcSleepThread(8000000);
		gfxFlushBuffers();
		
	}
	
	// clearing console
	consoleClear();
	
	// clearing news.db
	if (callPurge) purge(news);
	
	// freeing memory
	NewsList* tmp = news;
	while (tmp != NULL){
		news = news->next;
		free(tmp);
		tmp = news;
	}
	
	// exiting services
	printf("Closing news:s service\n");
	svcCloseHandle(newsHandle);
	printf("Exiting...\n\n\n");
	closeServices();
	
}