// This is the main DLL file.

#include "stdafx.h"

#include "OSD.h"

#define TICKS_PER_MICROSECOND 10
#define RTSS_VERSION(x, y) ((x << 16) + y)

namespace RTSSSharedMemoryNET {

    ///<param name="entryName">
    ///The name of the OSD entry. Should be unique and not more than 255 chars once converted to ANSI.
    ///</param>
    OSD::OSD(String^ entryName)
    {
        if( String::IsNullOrWhiteSpace(entryName) )
            throw gcnew ArgumentException("Entry name cannot be null, empty, or whitespace", "entryName");

        m_entryName = (LPCSTR)Marshal::StringToHGlobalAnsi(entryName).ToPointer();
        if( strlen(m_entryName) > 255 )
            throw gcnew ArgumentException("Entry name exceeds max length of 255 when converted to ANSI", "entryName");

        //just open/close to make sure RTSS is working
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);
        closeSharedMemory(hMapFile, pMem);

        m_osdSlot = 0;
        m_disposed = false;
    }

    OSD::~OSD()
    {
        if( m_disposed )
            return;

        

        //delete managed, if any

        this->!OSD();
        m_disposed = true;
    }

    OSD::!OSD()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        //find entries and zero them out
        for(DWORD i=1; i < pMem->dwOSDArrSize; i++)
        {
            //calc offset of entry
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)( (LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize) );

            if( STRMATCHES(strcmp(pEntry->szOSDOwner, m_entryName)) )
            {
                SecureZeroMemory(pEntry, pMem->dwOSDEntrySize); //won't get optimized away
                pMem->dwOSDFrame++; //forces OSD update
            }
        }

        closeSharedMemory(hMapFile, pMem);
        Marshal::FreeHGlobal(IntPtr((LPVOID)m_entryName));
    }

    System::Version^ OSD::Version::get()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        auto ver = gcnew System::Version(pMem->dwVersion >> 16, pMem->dwVersion & 0xFFFF);

        closeSharedMemory(hMapFile, pMem);
        return ver;
    }

    ///<summary>
    ///Text should be no longer than 4095 chars once converted to ANSI. Lower case looks awful.
    ///</summary>
    void OSD::Update(String^ text)
    {
        if( text == nullptr )
            throw gcnew ArgumentNullException("text");

        LPCSTR lpText = (LPCSTR)Marshal::StringToHGlobalAnsi(text).ToPointer();
        if( strlen(lpText) > 4095 )
            throw gcnew ArgumentException("Text exceeds max length of 4095 when converted to ANSI", "text");

        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        //start at either our previously used slot, or the top
        for(DWORD i=(m_osdSlot == 0 ? 1 : m_osdSlot); i < pMem->dwOSDArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_OSD_ENTRY)( (LPBYTE)pMem + pMem->dwOSDArrOffset + (i * pMem->dwOSDEntrySize) );

            //if we need a new slot and this one is unused, claim it
            if( m_osdSlot == 0 && !strlen(pEntry->szOSDOwner) )
            {
                m_osdSlot = i;
                strcpy_s(pEntry->szOSDOwner, m_entryName);
            }

            //if this is our slot
            if( STRMATCHES(strcmp(pEntry->szOSDOwner, m_entryName)) )
            {
                //use extended text slot for v2.7 and higher shared memory, it allows displaying 4096 symbols instead of 256 for regular text slot
                if( pMem->dwVersion >= RTSS_VERSION(2,7) )
                    strncpy_s(pEntry->szOSDEx, lpText, sizeof(pEntry->szOSDEx)-1);
                else
                    strncpy_s(pEntry->szOSD, lpText, sizeof(pEntry->szOSD)-1);

                pMem->dwOSDFrame++; //forces OSD update
                break;
            }

            //in case we lost our previously used slot or something, let's start over
            if( m_osdSlot != 0 )
            {
                m_osdSlot = 0;
                i = 1;
            }
        }

        closeSharedMemory(hMapFile, pMem);
        Marshal::FreeHGlobal(IntPtr((LPVOID)lpText));
    }

   
    array<AppEntry^>^ OSD::GetAppEntries()
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        openSharedMemory(&hMapFile, &pMem);

        auto list = gcnew List<AppEntry^>;

        //include all slots
        for(DWORD i=0; i < pMem->dwAppArrSize; i++)
        {
            auto pEntry = (RTSS_SHARED_MEMORY::LPRTSS_SHARED_MEMORY_APP_ENTRY)( (LPBYTE)pMem + pMem->dwAppArrOffset + (i * pMem->dwAppEntrySize) );
            if( pEntry->dwProcessID )
            {
                auto entry = gcnew AppEntry;
                
                //basic fields
                entry->ProcessId = pEntry->dwProcessID;
                entry->Name = Marshal::PtrToStringAnsi(IntPtr(pEntry->szName));
                entry->Flags = (AppFlags)pEntry->dwFlags;

                //instantaneous framerate fields
                entry->InstantaneousTimeStart = pEntry->dwTime0;
                entry->InstantaneousTimeEnd = pEntry->dwTime1;
                entry->InstantaneousFrames = pEntry->dwFrames;                
                list->Add(entry);
            }
        }

        closeSharedMemory(hMapFile, pMem);
        return list->ToArray();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void OSD::openSharedMemory(HANDLE* phMapFile, LPRTSS_SHARED_MEMORY* ppMem)
    {
        HANDLE hMapFile = NULL;
        LPRTSS_SHARED_MEMORY pMem = NULL;
        try
        {
            hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"RTSSSharedMemoryV2");
            if( !hMapFile )
                THROW_LAST_ERROR();

            pMem = (LPRTSS_SHARED_MEMORY)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if( !pMem )
                THROW_LAST_ERROR();

            if( !(pMem->dwSignature == 'RTSS' && pMem->dwVersion >= RTSS_VERSION(2,0)) )
                throw gcnew System::IO::InvalidDataException("Failed to validate RTSS Shared Memory structure");

            *phMapFile = hMapFile;
            *ppMem = pMem;
        }
        catch(...)
        {
            closeSharedMemory(hMapFile, pMem);
            throw;
        }
    }

    void OSD::closeSharedMemory(HANDLE hMapFile, LPRTSS_SHARED_MEMORY pMem)
    {
        if( pMem )
            UnmapViewOfFile(pMem);

        if( hMapFile )
            CloseHandle(hMapFile);

    }

}