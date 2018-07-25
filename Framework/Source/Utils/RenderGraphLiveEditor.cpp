/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "Falcor.h"
#include "RenderGraphLiveEditor.h"
#include "RenderGraphLoader.h"
#include <string>

namespace Falcor
{
    const char* kEditorExecutableName =
#ifdef _WIN32
        "RenderGraphEditor.exe";
#else
        "./RenderGraphEditor";
#endif
    const size_t kMaxTempFileSize = 0x01400000 / 16;

    RenderGraphLiveEditor::RenderGraphLiveEditor()
    {

    }

    bool RenderGraphLiveEditor::createMemoryMappedFile(const RenderGraph& renderGraph)
    {
        // create a new temporary file through windows
        mTempFileName.resize(510);
        mTempFilePath.resize(510);

        std::string renderGraphScript = RenderGraphLoader::saveRenderGraphAsScriptBuffer(renderGraph);
        if (!renderGraphScript.size())
        {
            logError("No graph data to display in editor.");
            return false;
        }

        GetTempPath(255, (LPWSTR)(&mTempFilePath.front()));
        GetTempFileName((LPCWSTR)mTempFilePath.c_str(), L"PW", 0, (LPWSTR)&mTempFileName.front());
        SECURITY_ATTRIBUTES secAttribs{};
        secAttribs.bInheritHandle = TRUE;

        mTempFileHndl = CreateFile((LPCWSTR)mTempFileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, &secAttribs, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
        if (!mTempFileHndl)
        {
            logError("Unable to create temporary file for graph editor");
            return false;
        }

        // map file so we can do things with it
        mTempFileMappingHndl = CreateFileMapping(mTempFileHndl, NULL, PAGE_READWRITE, 0, kMaxTempFileSize, NULL);
        if (!mTempFileMappingHndl)
        {
            logError("Unable to map temporary file for graph editor");
            if (mTempFileHndl) { CloseHandle(mTempFileHndl); }
            return false;
        }


        // write out the commands to execute
        mpToWrite = (char*)MapViewOfFile(mTempFileMappingHndl, FILE_MAP_WRITE, 0, 0, 0);

        if (!mpToWrite)
        {
            logError("Unable to map view of memory for graph editor.");
            return false;
        }
 
        CopyMemory(mpToWrite, renderGraphScript.data(), renderGraphScript.size());
        FlushViewOfFile(mpToWrite, renderGraphScript.size());

        UnmapViewOfFile(mpToWrite);
        mpToWrite = nullptr;
        
        CloseHandle(mTempFileMappingHndl);
        mTempFileMappingHndl = nullptr;

        return true;
    }

    void RenderGraphLiveEditor::openEditor(const RenderGraph& renderGraph)
    {
        if (mIsOpen)
        {
            logWarning("Render Graph Editor is already open for this graph!");
            return;
        }

        // create mapped memory and launch editor process
#ifdef _WIN32

        if (!createMemoryMappedFile(renderGraph))
        {
            return;
        }

        // load application for the editor given it the name of the mapped file
        std::string commandLine = kEditorExecutableName;
        std::string fileName;
        fileName.resize(mTempFileName.size() / 2);
        wcstombs(&fileName.front(), (wchar_t*)mTempFileName.c_str(), mTempFileName.size());
        commandLine += std::string(" ") + fileName;
        
        STARTUPINFOA startupInfo{}; PROCESS_INFORMATION processInformation{};
        
        if (!CreateProcessA(nullptr, (LPSTR)commandLine.c_str(), nullptr, nullptr, TRUE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startupInfo, &processInformation))
        {
            logError("Unable to execute the render graph editor");
            closeEditor();
            return;
        }
        
        mProcess = processInformation.hProcess;
        
#endif
        
        mIsOpen = true;

    }

#ifdef _WIN32
    static void processFileWatchCompletion(
        _In_    DWORD        dwErrorCode,
        _In_    DWORD        dwNumberOfBytesTransfered,
        _Inout_ LPOVERLAPPED lpOverlapped)
    {
        if (dwErrorCode)
        {
            logError("Failed to process file change");
        }
    }
#endif

    void RenderGraphLiveEditor::forceUpdateGraph(RenderGraph& renderGraph)
    {
        // load changes from the modified graph file
        std::string script;

        assert(mTempFileHndl);

        mTempFileMappingHndl = CreateFileMapping(mTempFileHndl, NULL, PAGE_READWRITE, 0, kMaxTempFileSize, NULL);
        if (!mTempFileMappingHndl)
        {
            logError("Unable to map temporary file for graph editor");
            if (mTempFileHndl) { CloseHandle(mTempFileHndl); }
            return;
        }

        // write out the commands to execute
        mpToWrite = (char*)MapViewOfFile(mTempFileMappingHndl, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
        assert(mpToWrite);

        mSharedMemoryStage.resize(kMaxTempFileSize);
        CopyMemory(&mSharedMemoryStage.front(), mpToWrite, kMaxTempFileSize);

        RenderGraphLoader::runScript(mSharedMemoryStage.data() + sizeof(size_t), *reinterpret_cast<const size_t*>(mSharedMemoryStage.data()), renderGraph);

        const size_t zero = 0;
        CopyMemory(mpToWrite, &zero, sizeof(size_t));

        UnmapViewOfFile(mpToWrite);
        mpToWrite = nullptr;

        CloseHandle(mTempFileMappingHndl);
        mTempFileMappingHndl = (HANDLE)nullptr;
    }

    void RenderGraphLiveEditor::updateGraph(RenderGraph& renderGraph)
    {
        bool status = false;

        // check to see if the editor has closed and react accordingly
        uint32_t exitCode = 0;
        if (GetExitCodeProcess(mProcess, (LPDWORD)&exitCode))
        {
            if (exitCode != STILL_ACTIVE)
            {
                CloseHandle(mProcess);
                mProcess = nullptr;
            }
        }

        return;

#ifdef _WIN32
        
        uint32_t fileInfoBuffer[sizeof(FILE_NOTIFY_INFORMATION)];
        OVERLAPPED overlappedData{};
        uint32_t bytesReturned = 0;

        status = ReadDirectoryChangesW(mTempFileHndl, fileInfoBuffer, sizeof(fileInfoBuffer),
            false, FILE_NOTIFY_CHANGE_LAST_WRITE, (LPDWORD)&bytesReturned, &overlappedData, processFileWatchCompletion);
#endif

        if (status)
        {
#ifdef _WIN32
            if ((*(FILE_NOTIFY_INFORMATION*)fileInfoBuffer).Action == FILE_ACTION_MODIFIED)
            {
                msgBox("File has been modified");
            }
#endif

            msgBox("Test");
        }
    }

    RenderGraphLiveEditor::~RenderGraphLiveEditor()
    {
        if (mIsOpen) {  }
    }

    void RenderGraphLiveEditor::closeEditor()
    {
#ifdef _WIN32
        CloseHandle(mTempFileHndl);
        CloseHandle(mTempFileMappingHndl);
#endif
        if (mProcess)
        {
            TerminateProcess(mProcess, 0);
            CloseHandle(mProcess);
            mProcess = nullptr;
        }

        mSharedMemoryStage.clear();
        mpToWrite = nullptr;
        mIsOpen = false;
    }
}
