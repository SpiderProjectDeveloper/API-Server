#pragma once

#define MIME_BUF_SIZE 80
void mimeSetType(char *fn, char *mime_buf, int mime_buf_size);

int get_uri_to_serve(char *b, int b_len, char *uri_buf, int uri_buf_size, bool *get, char **post, bool *options);

bool is_html_request( char *uri );

void parseJSON(char *b, char *user, int max_user, char *pass, int max_pass, char *sess_id, int max_sess_id, char *id, int max_id);

int get_content_read( char *b, int b_len );

int get_content_length( char *b, int b_len );

bool is_ext_json( char *path );

void error_message( std::string s );


