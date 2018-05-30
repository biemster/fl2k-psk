/*
 * osmo-fl2k, turns FL2000-based USB 3.0 to VGA adapters into
 * low cost DACs
 * 
 * On Ubunutu: sudo sh -c 'echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb'
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


int main(int argc, char **argv) {
	struct sigaction sigact, sigign;
	uint32_t freq_carrier = 14000000;
	uint32_t samplerate = freq_carrier * 2; // sample rate twice the frequency gives the smoothest output (TODO: experiment)

	init_txbuffer();
	fl2k_open(&fl2k_dev, fl2k_dev_idx);
	
	if (!fl2k_dev) {
		cout << "Failed to open fl2k device #" << fl2k_dev_idx << endl;
		exit(0);
	}
	cout << "Opened device" << endl;

	int r = fl2k_start_tx(fl2k_dev, fl2k_callback, nullptr, 0);

	// Set the sample rate
	r = fl2k_set_sample_rate(fl2k_dev, samplerate);
	if (r < 0) {
		cout << "WARNING: Failed to set sample rate. " << r << endl;
	}

	/* read back actual frequency */
	samplerate = fl2k_get_sample_rate(fl2k_dev);
	cout << "Actual sample rate = " << samplerate << endl;
	
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigign.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigact, nullptr);
	sigaction(SIGTERM, &sigact, nullptr);
	sigaction(SIGQUIT, &sigact, nullptr);
	sigaction(SIGPIPE, &sigign, nullptr);

	return 0;
}
