#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <codecvt>
#include "curl/curl_utils.h"
#include "json.h"
#include "base64.h"
#include "ntdll.h"

typedef struct
{
    USHORT Length;
    USHORT MaxLength;
    DWORD Buffer;
} UNICODE_STRING32;

typedef struct
{
    BYTE Reserved1[16];
    DWORD Reserved2[5];
    UNICODE_STRING32 CurrentDirectoryPath;
    DWORD CurrentDirectoryHandle;
    UNICODE_STRING32 DllPath;
    UNICODE_STRING32 ImagePathName;
    UNICODE_STRING32 CommandLine;
    DWORD env;
} RTL_USER_PROCESS_PARAMETERS32;

typedef struct
{
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[1];
    DWORD Reserved3[2];
    DWORD Ldr;
    DWORD ProcessParameters;
    // more fields...
} PEB32;

namespace fs = std::filesystem;

class c_riot
{
public:
    c_riot( const std::string& user, const std::string& pass )
    {
        this->username = user;
        this->password = pass;

        this->start_process();
        
        this->grab_riot_infos();

        this->login();
    }

    ~c_riot()
    {
        this->close_process();
    }

    void close_process()
    {
        // force close riot client services
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

    void close_ux()
    {
        // force close riot client services
        if( this->ux_pid )
        {
            HANDLE process_handle{ };
            CLIENT_ID cid = { reinterpret_cast<HANDLE>(this->ux_pid), NULL };
            OBJECT_ATTRIBUTES oa = {};
            InitializeObjectAttributes( &oa, 0, 0, 0, 0 );

            if( !NT_SUCCESS( NtOpenProcess( &process_handle, GENERIC_ALL, &oa, &cid ) ) )
                return;

            DWORD exit_code{};
            GetExitCodeProcess( process_handle, &exit_code );
            TerminateProcess( process_handle, exit_code );
        }
    }

    bool start_process()
    {
        auto appdata_local = fs::temp_directory_path().parent_path().parent_path();
        auto appdata_riot_client = appdata_local /= "Riot Games\\Riot Client\\Logs\\Riot Client Logs";

        if( !fs::exists( appdata_riot_client ) )
        {
            throw std::runtime_error( "Failed finding Riot Client path\n" );
            return false;
        }

        fs::path log_file_path{};

        auto it = fs::directory_iterator( appdata_riot_client );
        for( auto i = fs::begin( it ); fs::begin( i ) != fs::end( it ); )
        {
            auto entry = *(i++);
            if( fs::begin( i ) == fs::end( it ) )
            {
                log_file_path = entry.path();
            }
        }

        // read the log file
        std::ifstream logfile( log_file_path );

        // find --app-root= param
        std::string app_root{};
        std::string line{};
        while( std::getline( logfile, line ) )
        {
            size_t pos = line.find( "--app-root=" );
            if( pos != std::string::npos )
            {
                app_root = line.substr( pos ).substr( 11 );
                break;
            }
        }

        if( app_root == std::string{} )
        {
            throw std::runtime_error( "Failed finding app_root\n" );
            return false;
        }

        app_root += "\\RiotClientServices.exe";

        // create league of legends client
        PROCESS_INFORMATION pi = { 0 };
        STARTUPINFOA si = { 0 };
        si.cb = sizeof( STARTUPINFOA );

        if( !CreateProcessA( app_root.c_str(), (char*) ("\"RiotClientServices.exe\"  \"--allow-multiple-clients\""), nullptr, nullptr, FALSE, NULL, nullptr, nullptr, &si, &pi ) )
        {
            throw std::runtime_error( "Failed creating Riot Client process\n" );
            return false;
        }

        this->pid = pi.dwProcessId;

        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
        return true;
    }

    void grab_riot_infos()
    {
        while( this->ux_pid == 0 )
        {
            PROCESSENTRY32 pe{};
            pe.dwSize = sizeof( PROCESSENTRY32 );

            HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, NULL );

            if( Process32First( snapshot, &pe ) )
            {
                do
                {
                    if( pe.th32ParentProcessID == this->pid && _wcsicmp( pe.szExeFile, L"RiotClientUx.exe" ) == 0 )
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

        std::string cmdline = get_cmdline( this->ux_pid );
        cmdline = cmdline.substr( cmdline.find_first_of( "--" ) ); // skip exe path

        auto splitted_cmds = split( cmdline, " " );
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
        this->headers.push_back( "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) RiotClient/40.15.0 (CEF 74) Safari/537.36" );
        this->url = "https://127.0.0.1:" + this->port;
    }

    bool login()
    {
        json::jobject data{};

        data["clientId"] = "riot-client";
        data["trustLevels"] = std::vector<std::string>( { "always_trusted" } );

        curl_ret ret = curl->post( this->url + "/rso-auth/v2/authorizations", this->headers, data );
        auto parsed_json = json::jobject::parse( ret.body );
        if( parsed_json.get( "type" ).find( "needs_authentication" ) != std::string::npos )
        {
            data = {};
            data["username"] = this->username;
            data["password"] = this->password;
            data["persistLogin"].set_boolean( false );

            ret = curl->put( this->url + "/rso-auth/v1/session/credentials", this->headers, data );
            if( ret.response_code != 201 )
            {
                printf( "Unknown error, response code: %i\n", ret.response_code );
                return false;
            }

            parsed_json = json::jobject::parse( ret.body );
            auto error = parsed_json.get( "error" );
            if( error.find( "auth_failure" ) != std::string::npos )
            {
                printf( "Incorrect user/pass (%s:%s)\n", this->username.c_str(), this->password.c_str() );
                return false;
            }

            // if empty, error will be "" so we need to check for it
            if( error != "\"\"" )
            {
                printf( "Error logging in: %s\n", error.c_str() );
                return false;
            }
        }
        else
        {
            printf( "Already logged in as %s, skipping login\n", this->username.c_str() );
        }

        // check if agreement status
        ret = curl->get( this->url + "/eula/v1/agreement", this->headers );

        parsed_json = json::jobject::parse( ret.body );

        if( parsed_json.get( "acceptance" ).find( "Accepted" ) == std::string::npos ) // did not accept yet
        {
            curl->put( this->url + "/eula/v1/agreement/acceptance", this->headers ); // accept the agreement
            printf( "Accepted the agreement\n" );
        }

        ret = curl->get( this->url + "/rso-auth/v1/authorization/userinfo", this->headers ); // accept the agreement
        parsed_json = json::jobject::parse( ret.body );

        auto userInfo = parsed_json.get( "userInfo" );
        userInfo.pop_back();
        userInfo.erase( userInfo.begin() );
        userInfo.erase( std::remove( userInfo.begin(), userInfo.end(), '\\' ), userInfo.end() );
        parsed_json = json::jobject::parse( userInfo );

        auto ban_json = json::jobject::parse( parsed_json.get( "ban" ) );
        auto expiration_str = ban_json.get( "exp" );

        // check if user has no recent bans / has never been banned
        if( expiration_str != "null" )
        {
            expiration_str.erase( std::remove( expiration_str.begin(), expiration_str.end(), '"' ), expiration_str.end() );

            auto expiration_time = std::stoull( expiration_str );
            unsigned long long milliseconds_since_epoch =
                std::chrono::system_clock::now().time_since_epoch() /
                std::chrono::milliseconds( 1 );

            if( expiration_time > milliseconds_since_epoch )
            {
                printf( "Account is currently banned: %s\n", this->username.c_str() );
                return false;
            }
        }

        // start league client
        curl->post( this->url + "/product-launcher/v1/products/league_of_legends/patchlines/live", this->headers );

        return true;
    }

    int ux_pid{};
    int pid{};

    std::string username{};
    std::string password{};

    std::string port{};
    std::string token{};
    std::string url{};

    std::vector<std::string> headers{};

    std::unique_ptr<curl_utils> curl = std::make_unique<curl_utils>();
};