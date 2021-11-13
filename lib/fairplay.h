#ifndef FAIRPLAY_H
#define FAIRPLAY_H

#include "logger.h"

typedef struct fairplay_s fairplay_t;

fairplay_t *fairplay_init(logger_t *logger);
int fairplay_setup(fairplay_t *fp, const unsigned char req[16], unsigned char res[142]);
int fairplay_handshake(fairplay_t *fp, const unsigned char req[164], unsigned char res[32]);
int fairplay_decrypt(fairplay_t *fp, const unsigned char input[72], unsigned char output[16]);
void fairplay_destroy(fairplay_t *fp);

#endif
