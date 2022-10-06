/*****************************************************************************
 * play_wavfiles.c
 *
 * Plays one of a set of WAV files to speaker using PortAudio
 * Selection is via keyboard input using Ncurses
 * Wav file access uses Sndfile
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h> 	/* malloc() */
#include <unistd.h>     /* sleep() */
#include <stdbool.h>	/* true, false */
#include <string.h>	/* memset() */
#include <ctype.h>	/* lolower() */
#include <math.h>	/* sin() */
#include <sndfile.h>	/* libsndfile */
#include <portaudio.h>	/* portaudio */
#include <stdatomic.h>  /* permits write/read of "slection" to be atomic */
#include "paUtils.h"

#define MAX_PATH_LEN        256
#define MAX_FILES	    	8
#define MAX_CHN	            2
#define LINE_LEN			80
#define FRAMES_PER_BUFFER   1024 

/* data structure to pass to callback */
typedef struct {
	atomic_int selection;	/* so selection is thread-safe */
	unsigned int channels;
	unsigned int samplerate;
	float *x[MAX_FILES];
	float *next_sample[MAX_FILES];
	float *last_sample[MAX_FILES];
} Buf;

/* PortAudio callback function protoype */
static int paCallback( const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData );

/* Clears screen using ANSI escape sequences. */
void clear()
{
    printf("\033[2J");
    printf("\033[%d;%dH", 0, 0);
}

void user_input(char ifilename[MAX_FILES][MAX_PATH_LEN], int n, 
	int selection, PaStream *stream)
{
    clear();
	printf("Select input file by number:\n"); //line 0
	for (int i=0; i<n; i++) {
	  	printf("%2d %s\n", i, ifilename[i]); //line 1-num_input_files
	}
	printf("M to mute, Q to quit\n"); 
	printf("CPU Load: %lf\n", Pa_GetStreamCpuLoad (stream) );
	printf("Playing %2d, New selection: ", selection);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
  	char ifilename[MAX_FILES][MAX_PATH_LEN], ch, line[LINE_LEN];
  	int selection, num_input_files,count;
  	FILE *fp;
  	/* my data structure */
  	Buf buf, *p = &buf;
  	/* ibsndfile structures */
	SNDFILE *sndfile; 
	SF_INFO sfinfo;
  	/* PortAudio stream */
	PaStream *stream;

  	/* zero libsndfile structures */
	memset(&sfinfo, 0, sizeof(sfinfo));

	/* initialize selection: -1 indicates don't play any file */
	p->selection = -1;

  	/* 
  	 * Parse command line and open all files 
  	 */
	//Your code here

	//Usage printout
    if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file_list\n", argv[0]);
		return -1;
	}
	/* open list of files */
	fp = fopen(argv[1], "r");
	if ( fp == NULL ) {
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		return -1;
	}

	num_input_files = 0;
	/* read WAV filenames from the list in ifile_list.txt */
	/* print information about each WAV file */

    // get new wav file path from new line
    while (!(fscanf(fp, "%s", ifilename[num_input_files]) == EOF)  && num_input_files < MAX_FILES){
		
		//store the new line to file name array
		//sf_open wav file
		if((sndfile = sf_open(ifilename[num_input_files], SFM_READ, &sfinfo)) != NULL){

		}else{ //print error if cannot read file
			fprintf(stderr, "Cannot read %s\n", ifilename[num_input_files]);
			return -1;
		}
		
		//print wav header message
		printf("%i Frames: %lli, Channels: %i, Sample Rate: %i %s\n",num_input_files, sfinfo.frames, sfinfo.channels, sfinfo.samplerate, line);
		//store channels and sample rate when the first wav file is read
		if(num_input_files == 0){
			p->channels = sfinfo.channels;
			p->samplerate = sfinfo.samplerate;
		} 
		/* check compatibility of input WAV files
		* If sample rates don't match, print error and exit 
		* If number of channels don't match, print error and exit
		* if too many channels, print error and exit
		*/
		//If channels and sample rate match former wav files
		if(sfinfo.channels == p->channels && sfinfo.samplerate == p->samplerate){
			
			/* malloc storage and read audio data into buffer */
			p->x[num_input_files] = (float *)malloc(sfinfo.frames*sfinfo.channels*sizeof(float));
			if(p->x[num_input_files] == NULL){
				fprintf(stderr, "Error using malloc %s\n", ifilename[num_input_files]);
				return -1;
			}
			count = sf_readf_float(sndfile, p->x[num_input_files], sfinfo.frames);
			if(count != sfinfo.frames){
				fprintf(stderr, "Error loading %s, incorrect frame size\n", ifilename[num_input_files]);
				return -1;
			}

			p->next_sample[num_input_files] = p->x[num_input_files];
			p->last_sample[num_input_files] = p->x[num_input_files] + sfinfo.frames * sfinfo.channels -1;

		}else{ //return -1 if new wav file have different sample rate or channels
			fprintf(stderr, "Different sample rate or channels %s\n", ifilename[num_input_files]);
			return -1;
		}
		
		/* Close WAV file */
		sf_close(sndfile);
		num_input_files++;
	}
	
  	/* close input filelist */
	  fclose(fp);

	sleep(2);

	/* start up Port Audio */
	stream = startupPa(2, p->channels, p->samplerate, 
		FRAMES_PER_BUFFER, paCallback, &buf);

  	/* User Input */
	user_input(ifilename, num_input_files, p->selection, stream);

	ch = 0;
	while (ch != 'q') {
		fgets(line, LINE_LEN, stdin); 
		ch = tolower(line[0]);
		if (ch == 'm') {
			p->selection = -1;
		}
		else if (ch >= '0' && ch < '0'+num_input_files) {
			selection = ch-'0';
			/* write information to be read in callback
			 * this "one-way" communication of a single item
			 * eliminated the possibility of race condition due to
			 * asynchronous threads
			 */
			p->selection = selection;
		}
		user_input(ifilename, num_input_files, p->selection, stream);
  	}
  	clear();

	/* shut down Port Audio */
	shutdownPa(stream);
	/* free storage */
	for (int i=0; i<num_input_files; i++) {
		free(p->x[i]);
	}
	return 0;
}


/* This routine will be called by the PortAudio engine when audio is needed.
 * It will be called in the "real-time" thread, so don't do anything
 * in the routine that requires significant time or resources.
 */
static int paCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	Buf *p = (Buf *)userData; /* Cast data passed through stream to our structure. */
	//float *input = (float *)inputBuffer; /* input not used in this code */
	float *po = (float *)outputBuffer;
 
	/* if selection is -1, then fill output buffer with zeros
	 * otherwise, copy from selected WAV data buffer into output buffer
	 * if not enough samples in buffer, then reset to start of buffer 
	 * and read remaining samples
	 */
	//Your code here

	//read atomic variable 
	int selection = p->selection;
	int samplePerBuffer = framesPerBuffer * p->channels;
	
	if(selection ==-1){
		for(int i = 0; i < samplePerBuffer; i++){
				*po++ = 0.0;
			}
	}else{
		float *pn = p->next_sample[selection];
		float *pe = p->last_sample[selection];

		for(int i = 0; i< samplePerBuffer; i++){
			if(pn >= pe ){
				pn = p->x[selection];
			}
			*po++ = *pn++;
		}
		p->next_sample[selection] = pn;
	}
	




	return 0;
}
