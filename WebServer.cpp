#include <string>
#include <iostream>
#include <fstream>
#include <windows.h>
//#include <boost/regex.hpp>
#include "WebServer.hpp"

static SERVER_DLL_START p_server_start;

using namespace std;

char _callback_error_code;
#define RESPONSE_BUFFER 100000
char _callback_response[RESPONSE_BUFFER+1];

int callback ( ServerData *sd ) {
    _callback_error_code = -1;
    _callback_response[0] = '\x0';
    sd->sp_response_buf = _callback_response;
    sd->sp_free_response_buf = false;

    if( sd->message_id == SERVER_API_LIST ) {
        std::ifstream fin("api-json.txt", std::ios::in | std::ios::binary);
        if (fin) {
            // Reading http response body
            fin.seekg(0, std::ios::end);
            uintmax_t file_size = fin.tellg();
            fin.seekg(0, std::ios::beg);

            if( file_size < RESPONSE_BUFFER ) {
                fin.read(_callback_response, file_size); 	// Adding the file to serve
                sd->sp_response_buf_size = file_size;
                _callback_error_code = 0;
            }
            fin.close();
        }   
    } else if( sd->message_id == SERVER_API_COMMAND ) {
		//boost::regex xRegEx { R"dlm(.*"command": *"graphs".*)dlm" };
		//boost::regex xRegEx{"\\w+\\s\\w+"};
		//bool matched = boost::regex_match( std::string(sd->message), xRegEx );
		if( true /*matched*/ ) {
	        strcpy( _callback_response, "{ \"graphs\" : [ { \"name\":\"График 1\", \"array\" : [ [939600000, 888.982777777778], [940204800, 1253.44527777778], [940809600, 1386.01861111111], [941414400, 1571.91916666667], [942019200, 1901.13], [942624000, 2208.10333333333], [943228800, 3564.05805555556], [943833600, 3212.96301587302], [944438400, 3566.29009379509], [945043200, 3631.50736652237], [945648000, 3187.34841269841], [946252800, 2992.49254202408], [946857600, 3049.85439351786] ] }, { \"name\":\"График 2\", \"array\" : [ [939600000, 888.982777777778], [940204800, 1153.44527777778], [940809600, 1486.01861111111], [941414400, 1671.91916666667], [942019200, 2101.13], [942624000, 2208.10333333333], [943228800, 3564.05805555556], [943833600, 3612.96301587302], [944438400, 3666.29009379509], [945043200, 3631.50736652237], [945648000, 3487.34841269841], [946252800, 3192.49254202408], [946857600, 3149.85439351786] ] }, { \"name\":\"График 3\", \"array\" : [ [939600000, 988.982777777778], [940204800, 1253.44527777778], [940809600, 1686.01861111111], [941414400, 1671.91916666667], [942019200, 1701.13], [942624000, 2108.10333333333], [943228800, 3564.05805555556], [943833600, 2912.96301587302], [944438400, 2966.29009379509], [945043200, 2531.50736652237], [945648000, 2487.34841269841], [946252800, 2192.49254202408], [946857600, 2149.85439351786] ] } ] }" );
		} else {
	        strcpy( _callback_response, "{\"error\":\"\", \"response\":\"a response\"}" );
		}
        sd->sp_response_buf_size = strlen(_callback_response);
        _callback_error_code = 0;
    }
    return _callback_error_code;
}

static StartServerData Data;

int main (int argc, char** argv)
{
    HINSTANCE hServerDLL;

		Data.IP = nullptr;
    Data.Port = "8080";
    Data.ExePath = nullptr;
    Data.HtmlPath = "html\\";

    hServerDLL = LoadLibrary ("serverapi");
    if (hServerDLL != NULL)
    {
        std::cout << "Starting!" << std::endl;
        Data.Message = ssd_Start;
        p_server_start = (SERVER_DLL_START) GetProcAddress (hServerDLL, "start");

        if (p_server_start != NULL) {
            int status = p_server_start (&Data, callback);
            if( status < 0 ) {
                cerr << "The server has NOT been started (" << status << ")! Exiting..."  << endl;
                FreeLibrary(hServerDLL);
                return 0;                
            }
            cerr << "The server has started! Press <ENTER> to stop the server..."  << endl;
            cin.get();
            Data.Message = ssd_Stop;
            p_server_start (&Data, callback);
            cerr << "The server is stopped! Press <ENTER> to exit the program..."  << endl;
            cin.get();
        } else {
            cerr << "Failed to obtain a pointer to the \"start\" function!" << endl;
        }
    } else {
      cerr << "The server has not started!" << endl;
    }
    FreeLibrary(hServerDLL);
    return 0;
}
