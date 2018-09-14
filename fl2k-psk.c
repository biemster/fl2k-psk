/*
 * osmo-fl2k, turns FL2000-based USB 3.0 to VGA adapters into
 * low cost DACs
 * 
 * On Ubuntu: sudo sh -c 'echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb'
 *
 */

#include <iostream>
#include <signal.h>
#include <math.h>

#include "osmo-fl2k.h"

int8_t *tx_buffer = nullptr;
int phase_curr = 0;

using namespace std;

fl2k_dev_t *fl2k_dev = nullptr;
uint32_t fl2k_dev_idx = 0;

// Catches ^C and stops
static void sighandler(int signum) {
	cout << "Signal caught, exiting!" << endl;
	
	fl2k_stop_tx(fl2k_dev);
	fl2k_close(fl2k_dev);

	exit(0);
}

// USB calls back to get the next buffer of data
void fl2k_callback(fl2k_data_info_t *data_info) {
	if (data_info->device_error) {
		cout << "WARNING: device error" << endl;
	}

	// sample rate is twice the frequency, so half a wavelength is in de buffer
	phase_curr = (phase_curr + 180) % 360;
	int idx_txbuf = ((phase_curr/180) * FL2K_BUF_LEN);

	data_info->sampletype_signed = 1;
	data_info->r_buf = (char *)tx_buffer + idx_txbuf;
}

void init_txbuffer() {
	tx_buffer = (int8_t*)malloc(FL2K_BUF_LEN * 3); // put 1.5 wavelengths in buffer
	for(int i = 0; i < FL2K_BUF_LEN; i++) {
		tx_buffer[i] = (sin(((double)i/FL2K_BUF_LEN) * M_PI) * 127) +.5; // add .5 because integers round down
		tx_buffer[i + FL2K_BUF_LEN] = -tx_buffer[i];
		tx_buffer[i + (FL2K_BUF_LEN*2)] = tx_buffer[i];
	}
}

void attach_sighandlers() {
	struct sigaction sigact, sigign;
	
 	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigign.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigact, nullptr);
	sigaction(SIGTERM, &sigact, nullptr);
	sigaction(SIGQUIT, &sigact, nullptr);
	sigaction(SIGPIPE, &sigign, nullptr);
}

void set_freq_carrier(uint32_t freq) {
	uint32_t samplerate = freq * 2; // sample rate twice the frequency gives the smoothest output (TODO: experiment)

	// Set the sample rate
	int r = fl2k_set_sample_rate(fl2k_dev, samplerate);
	if (r < 0) {
		cout << "WARNING: Failed to set sample rate. " << r << endl;
	}

	/* read back actual frequency */
	samplerate = fl2k_get_sample_rate(fl2k_dev);
	cout << "Actual {sample rate,frequency} = {" << samplerate << "," << samplerate/2 << "}" << endl;
}


int main(int argc, char **argv) {
	attach_sighandlers();
	init_txbuffer();
	fl2k_open(&fl2k_dev, fl2k_dev_idx);
	
	if(!fl2k_dev) {
		cout << "Failed to open fl2k device #" << fl2k_dev_idx << endl;
	}
	else {
		cout << "Opened device" << endl;
	
		int r = fl2k_start_tx(fl2k_dev, fl2k_callback, nullptr, 0);
		set_freq_carrier(28000000);
	}

	cout << "Press u,d,q to raise or lower frequency, or quit: " << flush;
	char c;
	while(cin.get(c)) {
		switch(c) {
		case 'u':
			set_freq_carrier((fl2k_get_sample_rate(fl2k_dev) /2) +1000000);
			break;
		case 'd':
			set_freq_carrier((fl2k_get_sample_rate(fl2k_dev) /2) -1000000);
			break;
		case 'q':
			exit(0);
			break;
		default:
			break;
		}
		
		cin.ignore();
		cout << "> " << flush;
	}


	return 0;
}
