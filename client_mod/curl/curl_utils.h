#pragma once
#include <string>
#include <stdio.h>

#define CURL_STATICLIB
#include "curl.h"

#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")

struct curl_ret
{
    std::string header = "";
    std::string body = "";
    long response_code = 0;
};

class curl_utils
{
public:
    curl_utils() {}
    ~curl_utils() {}

    curl_ret get( const std::string& url, const std::vector<std::string>& headers = {})
    {
        curl = curl_easy_init();

        curl_ret ret;
        if( curl )
        {
            struct curl_slist* headers_list = NULL;
            //curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
            curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
            curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
            curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
            if( headers != std::vector<std::string>{} )
            {
                for( auto header : headers )
                    headers_list = curl_slist_append( headers_list, header.c_str() );
            }

            headers_list = curl_slist_append( headers_list, "Content-Type: application/json" );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers_list );

            curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "GET" );

            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, &ret.body );

            curl_easy_setopt( curl, CURLOPT_HEADERDATA, &ret.header );

            res = curl_easy_perform( curl );

            if( res != CURLE_OK )
            {
                //fprintf( stderr, "curl_easy_perform() failed: %s\n",
                //         curl_easy_strerror( res ) );
                return curl_ret{ "invalid", "invalid", -1 };
            }
            else
            {
                curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &ret.response_code );
            }
        }

        curl_easy_cleanup( curl );

        return ret;
    }

    curl_ret post( const std::string& url, const std::vector<std::string>& headers = {}, const std::string& data = {} )
    {
        curl = curl_easy_init();

        curl_ret ret;
        if( curl )
        {
            struct curl_slist* headers_list = NULL;

            //curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
            curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
            curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
            curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
            if( headers != std::vector<std::string>{} )
            {
                for( auto header : headers )
                    headers_list = curl_slist_append( headers_list, header.c_str() );
            }

            curl_easy_setopt( curl, CURLOPT_POST, 1 );

            if( data != "" )
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, data.c_str() );
            else
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, "{}");

            headers_list = curl_slist_append( headers_list, "Content-Type: application/json" );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers_list );

            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, &ret.body );

            curl_easy_setopt( curl, CURLOPT_HEADERDATA, &ret.header );

            res = curl_easy_perform( curl );

            if( res != CURLE_OK )
            {
                fprintf( stderr, "curl_easy_perform() failed: %s\n",
                         curl_easy_strerror( res ) );
                return curl_ret{ "invalid", "invalid", -1 };
            }
            else
            {
                curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &ret.response_code );
            }
        }

        curl_easy_cleanup( curl );

        return ret;
    }
    
    curl_ret put( const std::string& url, const std::vector<std::string>& headers = {}, const std::string& data = {})
    {
        curl = curl_easy_init();

        curl_ret ret;
        if( curl )
        {
            struct curl_slist* headers_list = NULL;

            //curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
            curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
            curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
            curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
            if( headers != std::vector<std::string>{} )
            {
                for( auto header : headers )
                    headers_list = curl_slist_append( headers_list, header.c_str() );
            }

            curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "PUT" );

            if( data != "" )
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, data.c_str() );
            else
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, "{}" );

            headers_list = curl_slist_append( headers_list, "Content-Type: application/json" );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers_list );

            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, &ret.body );

            curl_easy_setopt( curl, CURLOPT_HEADERDATA, &ret.header );

            res = curl_easy_perform( curl );

            if( res != CURLE_OK )
            {
                fprintf( stderr, "curl_easy_perform() failed: %s\n",
                         curl_easy_strerror( res ) );
                return curl_ret{ "invalid", "invalid", -1 };
            }
            else
            {
                curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &ret.response_code );
            }
        }

        curl_easy_cleanup( curl );

        return ret;
    }

    curl_ret patch( const std::string& url, const std::vector<std::string>& headers = {}, const std::string& data = {} )
    {
        curl = curl_easy_init();

        curl_ret ret;
        if( curl )
        {
            struct curl_slist* headers_list = NULL;

            //curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
            curl_easy_setopt( curl, CURLOPT_SSL_VERIFYPEER, 0L );
            curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
            curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
            curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
            if( headers != std::vector<std::string>{} )
            {
                for( auto header : headers )
                    headers_list = curl_slist_append( headers_list, header.c_str() );
            }

            curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "PATCH" );

            if( data != "" )
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, data.c_str() );
            else
                curl_easy_setopt( curl, CURLOPT_POSTFIELDS, "{}" );

            headers_list = curl_slist_append( headers_list, "Content-Type: application/json" );
            curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers_list );

            curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );
            curl_easy_setopt( curl, CURLOPT_WRITEDATA, &ret.body );

            curl_easy_setopt( curl, CURLOPT_HEADERDATA, &ret.header );

            res = curl_easy_perform( curl );

            if( res != CURLE_OK )
            {
                fprintf( stderr, "curl_easy_perform() failed: %s\n",
                         curl_easy_strerror( res ) );
                return curl_ret{ "invalid", "invalid", -1 };
            }
            else
            {
                curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &ret.response_code );
            }
        }

        curl_easy_cleanup( curl );

        return ret;
    }

private:
    static size_t write_callback( void* contents, size_t size, size_t nmemb, void* userp )
    {
        reinterpret_cast<std::string*>(userp)->append( reinterpret_cast<char*>(contents), size * nmemb );
        return size * nmemb;
    }

    CURL* curl;
    CURLcode res;
};