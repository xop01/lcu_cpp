#include <Windows.h>
#include <iostream>
#include <consoleapi2.h>
#include "riot.h"
#include "lcu.h"
#include <thread>

// never called, just here as an example
// what this does: login into rito client automatically, start league, close rito client ux, wait for lcu to be available and change status then close everything down
void automation_example()
{
    std::unique_ptr<c_riot> riot = std::make_unique<c_riot>( "user", "pass" );

    riot->close_ux();

    // IMPORTANT, pass riot->pid if you want to start multiple clients at the same time, otherwise you will not be able to grab the proper league client instance
    std::unique_ptr<c_lcu> lcu = std::make_unique<c_lcu>( riot->pid );

    lcu->change_status( "just automated status change wohoo!" );
}

void routine()
{
    std::unique_ptr<c_lcu> lcu = std::make_unique<c_lcu>();

    lcu->change_icon( 1669 );

    printf( "icon set!\n" );

    // need to loop unless you want the client to close
    // c_lcu deconstructor will close the process (see lcu.h)
    while( true )
    {
        std::this_thread::sleep_for( 50ms );
    }
}

int main()
{
    SetConsoleTitle( L"client mod" );

    // init CURL
    curl_global_init( CURL_GLOBAL_ALL );

    try
    {
        routine();
    }
    catch( std::exception ex )
    {
        printf_s( "[error] %s\n", ex.what() );
        system( "pause" );
    }
}