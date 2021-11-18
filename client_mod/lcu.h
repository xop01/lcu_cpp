#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include "curl/curl_utils.h"
#include <vector>
#include <codecvt>
#include <chrono>
#include <thread>
#include "base64.h"
#include "ntdll.h"

using namespace std::chrono_literals;

class c_lcu
{
public:
    c_lcu( int riot_pid = 0 )
    {
        this->riot_pid = riot_pid;

        printf( "waiting for lcu to be available..\n" );

        this->grab_lcu_infos();

        while( !this->lcu_available() )
        {
            std::this_thread::sleep_for( 50ms );
        }

        printf( "found lcu\n" );
    }

    ~c_lcu()
    {
        this->close_process();
    }

    void close_process()
    {
        // force close league client
        if( this->pid )
        {
            HANDLE process_handle{ };
            CLIENT_ID cid = { reinterpret_cast<HANDLE>(this->pid), NULL };
            OBJECT_ATTRIBUTES oa = {};
            InitializeObjectAttributes( &oa, 0, 0, 0, 0 );

            if( !NT_SUCCESS( NtOpenProcess( &process_handle, GENERIC_ALL, &oa, &cid ) ) )
                return;

            DWORD exit_code{};
            GetExitCodeProcess( process_handle, &exit_code );
            TerminateProcess( process_handle, exit_code );
        }
    }

    void grab_lcu_infos()
    {
        // grab LeagueClientUx.exe PID
        while( this->ux_pid == 0 )
        {
            PROCESSENTRY32 pe{};
            pe.dwSize = sizeof( PROCESSENTRY32 );

            HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, NULL );

            if( Process32First( snapshot, &pe ) )
            {
                do
                {
                    if( ( this->riot_pid == 0 || pe.th32ParentProcessID == this->riot_pid ) && _wcsicmp( pe.szExeFile, L"LeagueClient.exe" ) == 0 )
                        this->pid = pe.th32ProcessID;

                    if( pe.th32ParentProcessID == this->pid && _wcsicmp( pe.szExeFile, L"LeagueClientUx.exe" ) == 0 )
                        this->ux_pid = pe.th32ProcessID;
                }
                while( Process32Next( snapshot, &pe ) );
            }

            CloseHandle( snapshot );
        }

        auto split = []( std::string c, std::string cc ) -> std::vector<std::string>
        {
            std::vector<std::string> v{};
            size_t pos = 0;
            std::string token = {};
            while( (pos = c.find( cc )) != std::string::npos )
            {
                token = c.substr( 0, pos );
                v.emplace_back( token );
                c.erase( 0, pos + cc.length() );
            }
            v.emplace_back( c );
            return v;
        };

        auto get_cmdline = []( int pid ) -> std::string
        {
            HANDLE process_handle{ };
            CLIENT_ID cid = { reinterpret_cast<HANDLE>(pid), NULL };
            OBJECT_ATTRIBUTES oa = {};
            InitializeObjectAttributes( &oa, 0, 0, 0, 0 );

            if( !NT_SUCCESS( NtOpenProcess( &process_handle, GENERIC_ALL, &oa, &cid ) ) )
            {
                printf( "Could not open process!\n" );
                return {};
            }

            HMODULE ntll_handle = LoadLibraryA( "ntdll.dll" );
            if( ntll_handle == NULL )
                throw std::runtime_error( "Failed loading ntdll.dll\n" );

            auto NtQueryInformationProcess_fn = reinterpret_cast<decltype(&NtQueryInformationProcess)>(GetProcAddress( ntll_handle, "NtQueryInformationProcess" ));

            LPVOID ppeb32 = NULL;
            LONG status = NtQueryInformationProcess_fn( process_handle, ProcessWow64Information, &ppeb32, sizeof( LPVOID ), NULL );

            if( ppeb32 == NULL )
            {
                printf( "Error target is not 32 bit??\n" );
                return {};
            }

            PEB32 peb32;
            RTL_USER_PROCESS_PARAMETERS32 proc_params_32;

            if( !ReadProcessMemory( process_handle, ppeb32, &peb32, sizeof( peb32 ), NULL ) )
            {
                printf( "Error RPM peb32 %d\n", GetLastError() );
                return {};
            }

            // read process parameters
            if( !ReadProcessMemory( process_handle,
                UlongToPtr( peb32.ProcessParameters ),
                &proc_params_32,
                sizeof( proc_params_32 ),
                NULL ) )
            {
                printf( "Error RPM proc_params_32 %d\n", GetLastError() );
                return {};
            }

            LPCVOID src = UlongToPtr( proc_params_32.CommandLine.Buffer );
            SIZE_T size = proc_params_32.CommandLine.Length;

            WCHAR* buffer = (WCHAR*) calloc( size + 2, 1 );
            if( !buffer || !ReadProcessMemory( process_handle, src, buffer, size, NULL ) )
            {
                printf( "Error could not read CommandLine buffer %d\n", GetLastError() );
                return {};
            }

            return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes( buffer );
        };

        // get the cmdline from LeagueClientUx.exe
        std::string cmdline = get_cmdline( this->ux_pid );
        cmdline = cmdline.substr( cmdline.find_first_of( "--" ) ); // skip exe path

        // find the app port and auth token for LCU
        auto splitted_cmds = split( cmdline, "\" \"" );
        for( auto& cmd : splitted_cmds )
        {
            if( cmd.find( "--app-port=" ) != std::string::npos )
            {
                this->port = cmd.substr( 11 );
            }

            if( cmd.find( "--remoting-auth-token=" ) != std::string::npos )
            {
                this->token = base64::encode( std::string( "riot:" ) + cmd.substr( 22 ) );
            }
        }

        this->headers.push_back( "Authorization: Basic " + this->token );
        this->headers.push_back( "User-Agent: Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36" );
        this->url = "https://127.0.0.1:" + this->port;
    }

    bool lcu_available()
    {
        auto ret = curl->get( this->url + "/lol-summoner/v1/current-summoner", this->headers );

        // summoner will be validated on response_code 200 and all LCU apis will be available
        return ret.response_code == 200 ? true : false;
    }

    void change_icon( int icon_id )
    {
        json::jobject data{};

        data["icon"] = icon_id;

        curl->put( this->url + "/lol-chat/v1/me", this->headers, data );
    }

    void change_rank()
    {
        json::jobject lol{};
        json::jobject data{};

        lol["rankedLeagueTier"] = "CHALLENGER";
        lol["rankedLeagueQueue"] = "RANKED_SOLO_5x5";
        lol["rankedLeagueDivision"] = "I";
        lol["rankedPrevSeasonDivision"] = "I";
        lol["rankedPrevSeasonTier"] = "CHALLENGER";
        lol["rankedSplitRewardLevel"] = "3";

        data["lol"] = lol;

        auto ret = curl->put( this->url + "/lol-chat/v1/me", this->headers, data );

        std::string body = "{\"preferredBannerType\":\"lastSeasonHighestRank\",\"preferredCrestType\":\"ranked\"}";

        curl->put( this->url + "/lol-regalia/v2/current-summoner/regalia", this->headers, body );
    }

    void change_status( const std::string status )
    {
        json::jobject data{};

        data["statusMessage"] = status;

        auto ret = curl->put( this->url + "/lol-chat/v1/me", this->headers, data );
    }

    int riot_pid{};
    int ux_pid{};
    int pid{};

    std::string port{};
    std::string token{};
    std::string url{};

    std::vector<std::string> headers{};

    std::unique_ptr<curl_utils> curl = std::make_unique<curl_utils>();
};