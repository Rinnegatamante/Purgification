#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <3ds.h>
#include "khax.h"

Handle newsHandle;

typedef struct {
	bool dataSet;
	bool unread;
	bool enableJPEG;
	u8 unkFlag1;
	u8 unkFlag2;
	u64 processID;
	u8 unkData[24];
	u64 time;
	u16 title[32];
} NewsHeader;

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

void exit(){

	while (!(hidKeysDown() & KEY_A)) hidScanInput();
	
	// killing services
	printf("Exiting...");
	hidExit();
	gfxExit();
	
}

int main(){

	// Start basic stuffs
	gfxInit(GSP_RGB565_OES,GSP_RGB565_OES,false);
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSet3D(false);
	consoleInit(GFX_TOP, NULL);
	khaxInit();
	consoleClear();
	hidInit();
	
	// initializing news:s
	Result ret = srvGetServiceHandle(&newsHandle, "news:s");
	if (R_FAILED(ret)){
		printf("An error occurred...\nPress A to exit...");
		exit();
		return -1;
	}else printf("news:s successfully initialized...\n");
		
	// launching syscall
	printf("Executing 0x00050000 syscall...\n");
	u32 tot;
	ret = NEWS_GetTotalNotifications(&tot);
	printf("Result: %u\n", ret);
	printf("Detected %u notifications...\n",tot);
	printf("Clearing notifications database...\n");
	
	// clearing news.db
	int i = 0;
	int errors = 0;
	NewsHeader header = { 0 };
	while (i < tot){
		ret = NEWS_SetNotificationHeader(i, header);
		if (ret != 0) errors++;
		i++;
	}
	printf("%i notifications deleted!\n",tot - errors);
	
	// exiting news:s
	printf("Closing news:s service\n");
	svcCloseHandle(newsHandle);
	
	printf("Press A to exit...\n\n\n");
	exit();
	
}