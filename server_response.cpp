#include "server.h"

static const int _uri_buf_size = 400;		// Buffer size for uri
static const int _html_file_path_buf_size = SRV_MAX_HTML_ROOT_PATH + 1 + _uri_buf_size + 1;

static const int _file_to_serve_buf_size = 10000;
static char _file_to_serve_buf[ _file_to_serve_buf_size+1];

static const char _http_ok_header[] = "HTTP/1.1 200 OK\r\n\r\n";
static const char _http_ok_header_template[] = "HTTP/1.1 200 OK\r\nContent-Length:%d\r\n\r\n";
static const char _http_not_found_header[] = "HTTP/1.1 404 Not found\r\n\r\n";
static const char _http_failed_to_serve_header[] = "HTTP/1.1 501 Internal Server Error\r\nVersion: HTTP/1.1\r\nContent-Length:0\r\n\r\n";
static const char _http_empty_message[] = "HTTP/1.1 200 OK\r\nContent-Length:0\r\n\r\n";
static const char _http_bad_request_message[] = "HTTP/1.1 400 Bad Request\r\nVersion: HTTP/1.1\r\nContent-Length:0\r\n\r\n";
static const char _http_not_found_message[] = "HTTP/1.1 404 Not Found\r\nVersion: HTTP/1.1\r\nContent-Length:0\r\n\r\n";
static const char _http_failed_to_serve_message[] = "HTTP/1.1 501 Internal Server Error\r\nVersion: HTTP/1.1\r\nContent-Length:0\r\n\r\n";

static const char _http_ok_json_header[] = "HTTP/1.1 200 OK\r\nContent-Type:application/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
static const char _http_ok_json_header_template[] = "HTTP/1.1 200 OK\r\nContent-Type:application/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\nContent-Length:%d\r\n\r\n";
static const char _invalid_request_json[] = "{\"error\":\"Invalid request\"}";
static const char _error_json[] = "{\"error\":\"An error occured...\"}";
static const char _http_logged_in_json_template[] = 
    "HTTP/1.1 200 OK\r\nContent-Type:text/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"error\":\"\",\"sess_id\":\"%s\",\"user\":\"%s\"}";
static const char _http_login_error_json_message[] = 
    "HTTP/1.1 200 OK\r\nContent-Type:application/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"error\":\"Invalid login or password\",\"sess_id\":\"\"}";
static const char _http_logged_out_json_message[] = 
    "HTTP/1.1 200 OK\r\nContent-Type:text/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"error\":\"\",\"sess_id\":\"\",\"user\":\"\"}";
static const char _http_log_out_failed_json_message[] = 
    "HTTP/1.1 200 OK\r\nContent-Type:text/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"error\":\"Failed to log out. Not authorized?\"}";
static const char _http_auth_error_json_message[] = 
    "HTTP/1.1 200 OK\r\nContent-Length:26\r\nContent-Type:application/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"error\":\"Not authorized\"}";

static const char _http_ok_options_header[] = 
    "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n\r\n";

static constexpr int _max_response_size = 99999999;

static constexpr int _http_header_buf_size = sizeof(_http_ok_json_header_template) + 10;
static char _http_header_buf[_http_header_buf_size + 1];


#define ALLOWED_URI_NUM 4
static char *_allowed_uri[] = { "/", "/index.html", "/index_bundle.js", "/favicon.ico" }; 

#define MAX_ID 100
#define MAX_USER 100
#define MAX_PASS 100


class ResponseWrapper {
	public:
	const char* header;
	const char *body;
	char *body_allocated;
	int body_len;

	ResponseWrapper(): header(nullptr), body(nullptr), body_allocated(nullptr), body_len(0) {
        ;
	}	

	~ResponseWrapper() {
		if( body_allocated != nullptr ) {
			delete [] body;
		}
	}	
};

class ServerDataWrapper {
	public:

	ServerData sd;
	
	ServerDataWrapper() {
		sd.user = nullptr;
		sd.message = nullptr;
		sd.sp_response_buf = nullptr;
		sd.sp_free_response_buf = false;
  		sd.sp_response_is_file = false;
	}

	~ServerDataWrapper() {
		if( sd.sp_free_response_buf == true ) {
			delete [] sd.sp_response_buf;
		}
	}
};

static void read_html_file_and_prepare_response( char *file_name, char *html_source_dir, bool is_browser_request, ResponseWrapper &response );

static void request_spider( callback_ptr _callback, char *post, ResponseWrapper &response, ServerDataWrapper &sdw );

void server_response( int client_socket, char *socket_request_buf, int socket_request_buf_size, 
    char *html_source_dir, callback_ptr callback )
{
    char uri[_uri_buf_size+1];
    char *post;
    bool get;
    bool options;
    int uri_status = get_uri_to_serve(socket_request_buf, socket_request_buf_size, uri, _uri_buf_size, &get, &post, &options);

    if (uri_status != 0) { 	// Failed to parse uri - closing socket...
        send(client_socket, _http_bad_request_message, strlen(_http_bad_request_message), 0);
        //send(client_socket, _invalid_request_json, strlen(_invalid_request_json), 0);
        return;
    }

    error_message( std::string("server_response(): requested uri=") + uri );

    if( strcmp(uri,"/check_connection") == 0 ) { 	// A system message simply to check availability of the server.
        send(client_socket, _http_empty_message, strlen(_http_empty_message), 0);
        return;
    }				

    if( options ) { 	// An OPTIONS request - allowing all
        error_message( std::string("server_response(): sending OPTIONS:\n") + _http_ok_options_header );
        send(client_socket, _http_ok_options_header, strlen(_http_ok_options_header), 0);
        return;
    }

    ResponseWrapper response;
    ServerDataWrapper sdw;

    if( strcmp( uri, "/api_list" ) == 0 ) { 	// A GET request to serve the list of API function
        error_message( "server_response(): requested api list..." );
        request_spider( callback, nullptr, response, sdw );
    }
    else if( get ) 
    { 	// A GET request from a browser
        if( strcmp( uri, "/" ) == 0 || strcmp(uri, "/index") == 0 ) {
            strcpy( uri, "/index.html" );
            error_message( std::string("server_response(): redirected to ") + uri );
        }
        // Is the requested file an allowed one?
        bool allowed = false; 
        for( int i = 0 ; i < ALLOWED_URI_NUM ; i++ ) {
            if( strcmp( _allowed_uri[i], uri ) == 0 ) {
                allowed = true; 
                break;
            }
        } 
        if( !allowed ) {    // If the file is not allowed to serve - sending an error message...
            send(client_socket, _http_bad_request_message, strlen(_http_bad_request_message), 0);
            return;
        }
        try {   // Reading the file requested... 
            read_html_file_and_prepare_response( &uri[1], html_source_dir, true, response );
        }
        catch (...) {   // If error...
            error_message( "server_response(): Failed to create a response..." );
            send(client_socket, _http_failed_to_serve_message, strlen(_http_failed_to_serve_message), 0);
            return;
        }
    }
    else 	// Post != nullptr - an API entry is requested
    { 	
        request_spider( callback, post, response, sdw );
    }
	
    int header_sending_result = send(client_socket, response.header, strlen(response.header), 0);
    if (header_sending_result == SOCKET_ERROR) { 	// If error...
        error_message( "server_response(): header send failed: " + std::to_string(WSAGetLastError()) );
    } else {
        error_message( "****\nserver_response():\nresponse header sent:\n" + std::string(response.header) + "\n" );
        const char *body_ptr = nullptr;
        if( response.body != nullptr ) {
            body_ptr = response.body;
        } else if( response.body_allocated != nullptr ) {
            body_ptr = response.body_allocated;
        }
        if (body_ptr != nullptr) {
            // Sending the file to client...
            int body_sending_result = send(client_socket, body_ptr, response.body_len, 0);
            if (body_sending_result == SOCKET_ERROR) { 	// If error...
                error_message( "server_response(): send failed, " + std::to_string(WSAGetLastError()) );
            } else {
                error_message( "****\nserver_response():\nresponse body sent:\n" + std::string(body_ptr) + "\n" );
            }
        }
    }
}


static void request_spider( callback_ptr callback, char *post, ResponseWrapper &response, ServerDataWrapper &sdw )
{
    int callback_return; 	// 
    sdw.sd.user = nullptr;
    if( post == nullptr ) {
        sdw.sd.message_id = SERVER_API_LIST;
    } else {			
        sdw.sd.message_id = SERVER_API_COMMAND;
    }
    sdw.sd.message = post;
    sdw.sd.sp_response_buf = nullptr;
    error_message( "request_spider(): calling callback\n" );
    callback_return = callback( &sdw.sd );

    if( callback_return < 0 || sdw.sd.sp_response_buf_size == 0 ) { 	// An error 
	    error_message( "request_spider(): error!\n" );
        response.header = _http_ok_json_header;
        response.body = _invalid_request_json;
        response.body_len = strlen(_invalid_request_json);
    } 
    else { 	// Ok
        if( sdw.sd.sp_response_is_file ) {
		    error_message( "request_spider(): reading from a file...\n");
            read_html_file_and_prepare_response( sdw.sd.sp_response_buf, nullptr, false, response );
		    error_message( "request_spider(): read from a file.\n");
            return;
        }
		snprintf( _http_header_buf, _http_header_buf_size, _http_ok_json_header_template, sdw.sd.sp_response_buf_size );
        //response.header = _http_ok_json_header;			
		response.header = _http_header_buf;			
        response.body = sdw.sd.sp_response_buf;
        response.body_len = sdw.sd.sp_response_buf_size;
    }
}


static void read_html_file_and_prepare_response( char *file_name, char *html_source_dir, bool is_browser_request, ResponseWrapper &response )
{
    char file_path[ _html_file_path_buf_size ];
    
    if( html_source_dir != nullptr ) {
	    error_message( "html_source_dir=" + std::string(html_source_dir) );
        strcpy( file_path, html_source_dir );
    } else {
	    error_message( "html_source_dir=nullptr" );
        file_path[0] = '\x0';
    }
    strcat( file_path, file_name );
    error_message( "read_html_file...(): opening " + std::string(file_path) );

    enum class FileServingErr { ok, file_not_found, failed_to_serve };
    FileServingErr error = FileServingErr::failed_to_serve;

    std::ifstream fin(file_path, std::ios::in | std::ios::binary);
    if (fin) {
        error_message( "read_html_file...(): opened!" );

        // Reading http response body
        fin.seekg(0, std::ios::end);
        long int file_size = static_cast<long int>(fin.tellg());
        fin.seekg(0, std::ios::beg);

        if( file_size > 0 ) {
            if( file_size <= _file_to_serve_buf_size ) { 	// The static buffer is big enough...
                fin.read(_file_to_serve_buf, file_size); 
                if( fin.gcount() == file_size ) {
                    if( is_ext_json(file_path) ) {  // All json text is nothing but an "utf-8" text
		                snprintf( _http_header_buf, _http_header_buf_size, _http_ok_json_header_template, file_size );
                    } else {
		                snprintf( _http_header_buf, _http_header_buf_size, _http_ok_header_template, file_size );
                    }
                    response.header = _http_header_buf;
                    response.body = _file_to_serve_buf;
                    response.body_len = file_size;
                    error = FileServingErr::ok;
                } 
            } else if( file_size < _max_response_size ) {
                try { 
                    response.body_allocated = new char[file_size + 1];
                } catch(...) { ; }	
                if( response.body_allocated != nullptr ) {
                    fin.read(response.body_allocated, file_size); 	// Adding the file to serve
                    if( fin.gcount() == file_size ) {
                        if( is_ext_json(file_path) ) {  // All json text is nothing but an "utf-8" text
                            snprintf( _http_header_buf, _http_header_buf_size, _http_ok_json_header_template, file_size );
                        } else {
                            snprintf( _http_header_buf, _http_header_buf_size, _http_ok_header_template, file_size );
                        }
                        response.header = _http_header_buf;
                        response.body_len = file_size; 
                        error = FileServingErr::ok;
                    } 
                }
            }
        }
        fin.close();
    } else {
        error = FileServingErr::file_not_found;
    }

    if( error != FileServingErr::ok ) {
        if( is_browser_request ) {
            if( error == FileServingErr::file_not_found )
                response.header = _http_not_found_header;
            else 
                response.header = _http_failed_to_serve_header;
        } else {
            response.header = _http_ok_header;
            if( error == FileServingErr::file_not_found ) {
                response.body = _invalid_request_json;
                response.body_len = strlen(_invalid_request_json);
            }
            else {	
                response.body = _error_json;
                response.body_len = strlen(_error_json);
            }
        } 
    }
}
