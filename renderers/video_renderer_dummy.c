/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */


#include "video_renderer.h"

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



typedef struct video_renderer_dummy_s {
    video_renderer_t base;
} video_renderer_dummy_t;

static const video_renderer_funcs_t video_renderer_dummy_funcs;

video_renderer_t *video_renderer_dummy_init(logger_t *logger, video_renderer_config_t const *config) {
    video_renderer_dummy_t *renderer;
    renderer = calloc(1, sizeof(video_renderer_dummy_t));
    if (!renderer) {
        return NULL;
    }
    renderer->base.logger = logger;
    renderer->base.funcs = &video_renderer_dummy_funcs;
    renderer->base.type = VIDEO_RENDERER_DUMMY;


    logger_log(( (video_renderer_dummy_t *)renderer )->base.logger, LOGGER_DEBUG, "video_renderer_dummy_init(): INN");

    return &renderer->base;
}


// mgtm
FILE *write_ptr;

#if defined(__x86_64__)
	#define PATH_FIFO "/home/mal/rpiplay.fifo"
#else
	#define PATH_FIFO "/home/truebike/rpiplay.fifo"
#endif  // __x86_64__

int fdFifoWrite = -1;

bool bMpvStarted = false;

pid_t pidChild = -1;

int startMpv( void );




static void video_renderer_dummy_start(video_renderer_t *renderer) {
	printf("video_renderer_dummy_start: INN\n" );

    logger_log(( (video_renderer_dummy_t *)renderer )->base.logger, LOGGER_DEBUG, "video_renderer_dummy_start(): INN");

	// mgtm
/*
	write_ptr = fopen("/home/truebike/dummy.out.h264","ab+");  // w for write, b for binary

	if ( !write_ptr ) {
		printf( "could not open stream output file\n" );
	}
*/

    unlink( PATH_FIFO );

	printf( "unlinked\n");

    mkfifo(PATH_FIFO, S_IFIFO | 0777);
	printf( "mkfifoed\n");

    fdFifoWrite = open(PATH_FIFO, O_RDWR);
	if ( fdFifoWrite ) {
		 printf( "open stream fifo\n" );
	} else {
		printf( "ERROR: could not open fifo\n" );
		}


    logger_log(( (video_renderer_dummy_t *)renderer )->base.logger, LOGGER_DEBUG, "video_renderer_dummy_start(): OUT: pidChilde:%d", pidChild);

	return;

}


static void video_renderer_dummy_conn_init(video_renderer_t *renderer) {

	printf("video_renderer_dummy_conn_init(): INN, starting video\n" );

	// start mpv to read the fifo when we get a connection
	if ( !bMpvStarted ) {
		if ( startMpv() == 0 )
			{
			bMpvStarted = true;
			}
		}	
	}


int startMpv( void ) 
{
	int retError = 0;

	pidChild = fork();
	if ( pidChild == 0 ) {  // child
		printf( "starting mplayer\n" );

		//char* const args[] = { "/usr/bin/mpv", "-geometry", "580x320+20+20", PATH_FIFO, NULL };
		//execv( "/usr/bin/mpv", args );
		//char* const args[] = { "/home/truebike/Downloads/MPlayer-1.3.0/mplayer", "-fps", "25", "-cache", "1024", "-vf", "expand=::::::32", "/home/truebike/rpiplay.fifo", NULL };
		//execv( "/home/truebike/Downloads/MPlayer-1.3.0/mplayer", args );

		char* const args[] = { "/home/truebike/runvideoplayer", NULL };
		execv( "/home/truebike/runvideoplayer.sh", args );
		

		printf( "mplayer returned\n" );

		exit( 0 );
	} else if ( pidChild < 0 ) {  // error
		retError = -1;
	}

	return( retError );
}

static void video_renderer_dummy_render_buffer(video_renderer_t *renderer, raop_ntp_t *ntp, unsigned char *data, int data_len, uint64_t pts, int type) {

	static int iCount = 0;
	if ( iCount++ %5 == 0 ) {
		printf( "." );
	}
	if ( iCount++ %100 == 0 ) {
		printf( "video_renderer_dummy_render() %d\n", iCount);
	}

	if ( data ) {
		//fwrite( data, data_len, 1, write_ptr ); // write all the data we get

        	write( fdFifoWrite, data, data_len );
	}
}

static void video_renderer_dummy_flush(video_renderer_t *renderer) {
	//fflush( write_ptr );
}

static void video_renderer_dummy_conn_destroy(video_renderer_t *renderer) {

	printf("video_renderer_dummy_conn_destroy(): INN\n" );

	// stop the video player
	if ( pidChild != 0 ) { 
		printf( "  stopping mplayer, pid: %d()\n", pidChild );
		
		kill( pidChild, 9 ); 
		pidChild = 0;

		bMpvStarted = false;
	}

	printf("video_renderer_dummy_conn_destroy(): OUT\n" );

	return;
}

static void video_renderer_dummy_destroy(video_renderer_t *renderer) {
	printf( "video_renderer_dummy_destroy() INN\n" );

	//fclose( write_ptr );

    close( fdFifoWrite );

    if (renderer) {
        free(renderer);
    }
	printf( "video_renderer_dummy_destroy() Done\n" );
}

static void video_renderer_dummy_update_background(video_renderer_t *renderer, int type) {

}

static const video_renderer_funcs_t video_renderer_dummy_funcs = {
    .start = video_renderer_dummy_start,
    .render_buffer = video_renderer_dummy_render_buffer,
    .flush = video_renderer_dummy_flush,
    .destroy = video_renderer_dummy_destroy,
    .update_background = video_renderer_dummy_update_background,
	.conn_init = video_renderer_dummy_conn_init,
	.conn_destroy = video_renderer_dummy_conn_destroy,
};

