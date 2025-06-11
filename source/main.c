#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int cursorX, cursorY;

struct saveFile {
        char fileName[16];
        bool exists;
};

bool waitForButton(u32 key_yes, u32 key_no) {
    svcSleepThread(1000000000LL);
    while (true) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & key_yes) {
            return true;
        }
        if (kDown & key_no) {
            return false;
        }
    }
}

u64 getCartID() 
{
    u32 count = 1;
    u64 buf;
    u32 titlesread;
    amInit();
    AM_GetTitleList(&titlesread, MEDIATYPE_GAME_CARD, count, &buf);
    amExit();
    if (titlesread != 1) {
        return 0;
    }
    return buf;
}

void printStatus(struct saveFile saveFiles[], int selected, bool payloadExists) {
    printf("\x1b[7;1H%s slot1: %s | %s\n", selected == 1 ? ">" : " ", saveFiles[0].fileName, saveFiles[0].exists ? "exists" : "inactive");
    printf("%s slot2: %s | %s\n", selected == 2 ? ">" : " ", saveFiles[1].fileName, saveFiles[1].exists ? "exists" : "inactive");
    printf("%s slot3: %s | %s\n", selected == 3 ? ">" : " ", saveFiles[2].fileName, saveFiles[2].exists ? "exists" : "inactive");
    if (saveFiles[3].exists) {
        printf("backup: %s | %s\n", saveFiles[3].fileName, "present");
    }
    if (payloadExists) {
        printf("payload: exists\n");
    }
    printf("\n");
}

void saveCursorPos(PrintConsole *console) {
    cursorX = console->cursorX;
    cursorY = console->cursorX;
}

void restoreCursorPos(PrintConsole *console) {
    console->cursorX = cursorX;
    console->cursorY = cursorY;
}


int main(int argc, char *argv[]) {
    gfxInitDefault();
    PrintConsole consoleTop;
    consoleInit(GFX_TOP, &consoleTop);
    romfsInit();
start:
    printf("oot3dhax installer - made by gruetzig\n");
    printf("detecting cart... \n");
    u64 tid = getCartID();
    
    switch (tid) {
        case 0:
            printf("no cart detected :(");
            goto end;
        case 0x000400000008f800:
            printf("found kor oot3d, mounting...\n");
            break;
        case 0x000400000008f900:
            printf("found twn oot3d, mounting...\n");
            break;
        case 0x0004000000033400:
            printf("found jpn oot3d, mounting...\n");
            break;
        case 0x0004000000033500:
            printf("found usa oot3d, mounting...\n");
            break;
        case 0x0004000000033600:
            printf("found eur oot3d, mounting...\n");
            break;
        default:
            printf("found unknown cart: %llx, :(\n", tid);
            goto end;
    }
    FS_Archive gameArchive;
    u32 pathData[3] = {MEDIATYPE_SD, tid & 0xFFFFFFFF, 0x00040000};
    FS_Path archivePath = {PATH_BINARY, 12, (const void*)pathData};
    Result ret = FSUSER_OpenArchive(&gameArchive, ARCHIVE_USER_SAVEDATA, archivePath);
    if (R_FAILED(ret)) {
        printf("mount OpenArchive failed: 0x%08lx\n", ret);
        goto end;
    }
    Handle archiveDir;
    ret = FSUSER_OpenDirectory(&archiveDir, gameArchive, fsMakePath(PATH_ASCII, "/"));
    if (R_FAILED(ret)) {
        printf("mount OpenDirectory failed: 0x%08lx\n", ret);
        FSDIR_Close(archiveDir);
        goto end;
    }
    FS_DirectoryEntry entries[6];
    memset(entries, 0, sizeof(entries));
    u32 entriesRead = 0;
    ret = FSDIR_Read(archiveDir, &entriesRead, 6, entries);
    if (R_FAILED(ret)) {
        printf("mount Read failed: 0x%08lx\n", ret);
        FSDIR_Close(archiveDir);
        goto end;
    }
    
    struct saveFile saveFiles[4];
    memset(saveFiles, 0, sizeof(saveFiles));
    bool payloadExists = false;
    for (int i = 0;i<entriesRead;i++) {
        char str[16] = "\0";
        utf16_to_utf8(str, entries[i].name, 16);
        if (strncmp(str, "save0", 5) == 0) {
            saveFiles[str[5]-'0'].exists = true;
            sprintf(saveFiles[str[5]-'0'].fileName, "/%s", str);
        }
        if (strncmp(str, "payload", 7) == 0) {
            payloadExists = true;
        }
    }
    FSDIR_Close(archiveDir);
    printf("mount successful\nx to restore, y to inject, b to cancel\n");
    int selected = 0;
    printStatus(saveFiles, selected, payloadExists);
    int mode = 0;
    while (true) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        
        if (kDown & KEY_B) {
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        if (kDown & KEY_X) {
            mode = 1; //restore
            break;
        }
        if (kDown & KEY_Y) {
            mode = 0; //inject
            break;
        }
    }
    if (mode == 0) {//inject 
        printf("hit a to create/overwrite backup, b for no backup\n");
        if(waitForButton(KEY_A, KEY_B)) {
            mode = 2; //backup + inject
        } else {
            mode = 3; //inject no backup
        }
    }
    if (mode == 1) {
        printf("select slot to restore to\n");
    }
    if (mode == 2 || mode == 3) {
        printf("select slot to inject\n");
    }
    selected = 1;
    while (true) {
        
        printStatus(saveFiles, selected, payloadExists);
        
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_UP) {
            if (selected > 1) selected--;
        }
        if (kDown & KEY_DOWN) {
            if (selected < 3) selected++;
        }
        if (kDown & KEY_A) {
            break;
        }
        if (kDown & KEY_B) {
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
    }
    printf("\x1b[15;1Hselected slot%d\n", selected);
    printf("Please confirm your selection:\nSlot: %d\nAction: %s\nPress A to confirm, Press B to cancel\n", selected, (mode == 1) ? "Restore backup" : ((mode == 3) ? "Inject without creating backup" : "Inject and create/overwrite backup"));
    if (!waitForButton(KEY_A, KEY_B)) {
        FSUSER_CloseArchive(gameArchive);
        goto end;
    }
    if (mode == 2 || mode == 3) {
        FILE *pf;
        pf = fopen("sdmc:/oot3dhax_payload.bin", "rb");
        if (!pf) {
            printf("Payload file not found. Stopping\n");
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        if (mode == 2) {
            printf("Backing up...\n");
            Handle srcFile;
            ret = FSUSER_OpenFile(&srcFile, gameArchive, fsMakePath(PATH_ASCII, saveFiles[selected-1].fileName), FS_OPEN_READ | FS_OPEN_WRITE, 0);
            if (R_FAILED(ret)) {
                printf("Error while backing up: srcOpenFile: %08lx\n", ret);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u64 size = 0;
            ret = FSFILE_GetSize(srcFile, &size);
            if (R_FAILED(ret)) {
                printf("Error while backing up: srcGetSize: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u8* buffer = malloc(size);
            if (buffer == NULL) {
                printf("Error while backing up: malloc failed\n");
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u32 bytesRead;
            ret = FSFILE_Read(srcFile, &bytesRead, 0, buffer, size);
            if (R_FAILED(ret)) {
                printf("Error while backing up: srcRead: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            Handle dstFile;
            ret = FSUSER_OpenFile(&dstFile, gameArchive, fsMakePath(PATH_ASCII, "/save03.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
            if (R_FAILED(ret)) {
                printf("Error while backing up: dstOpenFile: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u32 bytesWritten;
            ret = FSFILE_Write(dstFile, &bytesWritten, 0, buffer, size, FS_WRITE_FLUSH);
            if (R_FAILED(ret)) {
                printf("Error while backing up: dstWrite: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSFILE_Close(dstFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            ret = FSFILE_Close(dstFile);
            if (R_FAILED(ret)) {
                printf("Error while backing up: dstClose: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            ret = FSFILE_Close(srcFile);
            if (R_FAILED(ret)) {
                printf("Error while backing up: srcClose: %08lx\n", ret);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            free(buffer);
            printf("Backup finished.\n");
        }
        printf("Injecting...\n");
        char fopenbuf[128];
        sprintf(fopenbuf, "romfs:/%016llx/save.bin", tid);
        FILE *f = fopen(fopenbuf, "rb");
        if (!f) {
            printf("Error while injecting: romfs fopen\n");
        }
        fseek(f, 0, SEEK_END);
        u32 size = ftell(f);
        fseek(f, 0, SEEK_SET);
        u8* buffer = malloc(size);
        if (buffer == NULL) {
            printf("Error while injecting: malloc failed\n");
            fclose(f);
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        fread(buffer, 1, size, f);
        fclose(f);
        Handle dstFile;
        ret = FSUSER_OpenFile(&dstFile, gameArchive, fsMakePath(PATH_ASCII, saveFiles[selected-1].fileName), FS_OPEN_READ | FS_OPEN_WRITE, 0);
        if (R_FAILED(ret)) {
            printf("Error while injecting: dstOpenFile: %08lx\n", ret);
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        u32 bytesWritten;
        ret = FSFILE_Write(dstFile, &bytesWritten, 0, buffer, size, FS_WRITE_FLUSH);
        if (R_FAILED(ret)) {
            printf("Error while injecting: dstWrite: %08lx\n", ret);
            FSFILE_Close(dstFile);
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        ret = FSFILE_Close(dstFile);
        if (R_FAILED(ret)) {
            printf("Error while injecting: dstClose: %08lx\n", ret);
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        free(buffer);
        printf("Inject finished.\n");
        printf("Adding payload...\n");
        Handle payloadFile;
        ret = FSUSER_OpenFile(&payloadFile, gameArchive, fsMakePath(PATH_ASCII, "/payload.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
        if (R_FAILED(ret)) {
            printf("Error while adding payload: dstOpenFile: %08lx\n", ret);
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        
        fseek(pf, 0, SEEK_END);
        size = ftell(pf);
        fseek(pf, 0, SEEK_SET);
        buffer = malloc(size);
        if (!buffer) {
            printf("Error while adding payload: buffer malloc failed\n");
            FSFILE_Close(payloadFile);
            FSUSER_CloseArchive(gameArchive);
            goto end;
        }
        fread(buffer, 1, size, pf);
        fclose(pf);
        ret = FSFILE_Write(payloadFile, NULL, 0, buffer, size, FS_WRITE_FLUSH);
        if (R_FAILED(ret)) {
            printf("Error while adding payload: dstWrite: %08lx\n", ret);
            FSFILE_Close(payloadFile);
            FSUSER_CloseArchive(gameArchive);
            free(buffer);
            goto end;
        }
        ret = FSFILE_Close(payloadFile);
        if (R_FAILED(ret)) {
            printf("Error while adding payload: dstClose: %08lx\n", ret);
            FSUSER_CloseArchive(gameArchive);
            free(buffer);
            goto end;
        }
        printf("Adding payload finished.\n");
        free(buffer);
    }
    else if (mode == 1) {
        if (saveFiles[selected-1].fileName[0] == 0) {
            printf("Save file doesn't exist, preparing for creation...\n");
            sprintf(saveFiles[selected-1].fileName, "/save0%d.bin", selected-1);
        }
        printf("Restoring...\n");
            Handle srcFile;
            ret = FSUSER_OpenFile(&srcFile, gameArchive, fsMakePath(PATH_ASCII, "/save03.bin"), FS_OPEN_READ | FS_OPEN_WRITE, 0);
            if (R_FAILED(ret)) {
                printf("Error while restoring: srcOpenFile: %08lx\n", ret);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u64 size = 0;
            ret = FSFILE_GetSize(srcFile, &size);
            if (R_FAILED(ret)) {
                printf("Error while restoring: srcGetSize: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u8* buffer = malloc(size);
            if (buffer == NULL) {
                printf("Error restoring: malloc failed\n");
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u32 bytesRead;
            ret = FSFILE_Read(srcFile, &bytesRead, 0, buffer, size);
            if (R_FAILED(ret)) {
                printf("Error while restoring: srcRead: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            Handle dstFile;
            ret = FSUSER_OpenFile(&dstFile, gameArchive, fsMakePath(PATH_ASCII, saveFiles[selected-1].fileName), FS_OPEN_CREATE | FS_OPEN_WRITE, 0);
            if (R_FAILED(ret)) {
                printf("Error while restoring: dstOpenFile: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            u32 bytesWritten;
            ret = FSFILE_Write(dstFile, &bytesWritten, 0, buffer, size, FS_WRITE_FLUSH);
            if (R_FAILED(ret)) {
                printf("Error while restoring: dstWrite: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSFILE_Close(dstFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            ret = FSFILE_Close(dstFile);
            if (R_FAILED(ret)) {
                printf("Error while restoring: dstClose: %08lx\n", ret);
                FSFILE_Close(srcFile);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            ret = FSFILE_Close(srcFile);
            if (R_FAILED(ret)) {
                printf("Error while restoring: srcClose: %08lx\n", ret);
                FSUSER_CloseArchive(gameArchive);
                goto end;
            }
            free(buffer);
            printf("Restore finished.\n");
    }
    FSUSER_ControlArchive(gameArchive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
    FSUSER_CloseArchive(gameArchive);
end:
    printf("Press START to exit, X to start again\n");
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) {
            break;
        }
        if (kDown & KEY_X) {
            consoleClear();
            goto start;
        }
    }
    romfsExit();
    gfxExit();
}
