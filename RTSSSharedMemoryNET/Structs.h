#pragma once

using namespace System;
using namespace System::Drawing;
using namespace System::Diagnostics;

namespace RTSSSharedMemoryNET {
    [Flags]
    public enum class AppFlags
    {
        None        = 0,
        OpenGL      = APPFLAG_OGL,
        DirectDraw  = APPFLAG_DD,
        Direct3D8   = APPFLAG_D3D8,
        Direct3D9   = APPFLAG_D3D9,
        Direct3D9Ex = APPFLAG_D3D9EX,
        Direct3D10  = APPFLAG_D3D10,
        Direct3D11  = APPFLAG_D3D11,

        ProfileUpdateRequested = APPFLAG_PROFILE_UPDATE_REQUESTED,
        MASK = (APPFLAG_DD | APPFLAG_D3D8 | APPFLAG_D3D9 | APPFLAG_D3D9EX | APPFLAG_OGL | APPFLAG_D3D10  | APPFLAG_D3D11),
    };   

    ///////////////////////////////////////////////////////////////////////////


    [DebuggerDisplay("{ProcessId}:{Name}, {Flags}")]
    public ref struct AppEntry
    {
    public:
        int ProcessId;
        String^ Name;
        AppFlags Flags;
        
        //instantaneous framerate fields
        DWORD InstantaneousTimeStart;
        DWORD InstantaneousTimeEnd;
        DWORD InstantaneousFrames;        
    };
}