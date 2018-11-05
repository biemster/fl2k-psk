/*
 * osmo-fl2k, turns FL2000-based USB 3.0 to VGA adapters into
 * low cost DACs
 * 
 * On Ubuntu: sudo sh -c 'echo 0 > /sys/module/usbcore/parameters/usbfs_memory_mb'
 *
 */

#include <iostream>
#include <signal.h>
#include <math.h>
#include <map>
#include <vector>

#include "osmo-fl2k.h"

#define SIGNAL_MAX 99			// 99 * 0.70710678 = 70.00.... keeps rounding error in signal to minimum
								// 17 * 0.70710678 = 12.02.... keeps rounding error low and low signal keeps rest of the spectrum relatively clean
#define SIGNAL_MIN -SIGNAL_MAX
#define SIGNAL_FREQ 28000000
#define SF_RATIO 4				// {2,4,6,8,10} samplerate / signal frequency ratio (max sample rate is about 140 MS/s on USB3)

#define SIN_1_4_PI 0.707106781		// sin(1/4*pi)
#define SIN_1_6_PI 0.500000000		// sin(1/6*pi)
#define SIN_1_10_PI 0.309016994		// sin(1/10*pi)
#define SIN_3_10_PI 0.809016994		// sin(3/10*pi)

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

	data_info->sampletype_signed = 1;
	data_info->r_buf = (char *)tx_buffer;
}

void init_txbuffer() {
	int8_t sin14pi = (int8_t)(SIGNAL_MAX * SIN_1_4_PI);
	int8_t sin16pi = (int8_t)(SIGNAL_MAX * SIN_1_6_PI);
	int8_t sin110pi = (int8_t)(SIGNAL_MAX * SIN_1_10_PI);
	int8_t sin310pi = (int8_t)(SIGNAL_MAX * SIN_3_10_PI);
	map<int,vector<int8_t>> sines;
	sines[2] = {SIGNAL_MAX, SIGNAL_MIN};
	sines[4] = {0, SIGNAL_MAX, 0, SIGNAL_MIN, 0, SIGNAL_MAX};
	sines[6] = {sin16pi, SIGNAL_MAX, sin16pi, (int8_t)-sin16pi, SIGNAL_MIN, (int8_t)-sin16pi, sin16pi, SIGNAL_MAX, sin16pi};
	sines[8] = {0, sin14pi, SIGNAL_MAX, sin14pi, 0, (int8_t)-sin14pi, SIGNAL_MIN, (int8_t)-sin14pi, 0, sin14pi, SIGNAL_MAX, sin14pi, 0};
	sines[10] = {sin110pi, sin310pi, SIGNAL_MAX, sin310pi, sin110pi, (int8_t)-sin110pi, (int8_t)-sin310pi, SIGNAL_MIN, (int8_t)-sin310pi, (int8_t)-sin110pi, sin110pi, sin310pi, SIGNAL_MAX, sin310pi, sin110pi};

	tx_buffer = (int8_t*)malloc(FL2K_BUF_LEN);
	auto sine = sines[SF_RATIO];
	int phase_shift = 0;
	for(int i = 0; i < FL2K_BUF_LEN; i++) {
		if(SF_RATIO == 4 && !(i%40000)) {
			phase_shift = !phase_shift ? 2 : 0;
		}
		tx_buffer[i] = sine[(i % SF_RATIO) +phase_shift];
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
	uint32_t samplerate = freq * SF_RATIO;

	// Set the sample rate
	int r = fl2k_set_sample_rate(fl2k_dev, samplerate);
	if (r < 0) {
		cout << "WARNING: Failed to set sample rate. " << r << endl;
	}

	/* read back actual frequency */
	samplerate = fl2k_get_sample_rate(fl2k_dev);
	cout << "Actual {sample rate,frequency} = {" << samplerate << "," << samplerate/SF_RATIO << "}" << endl;
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
		set_freq_carrier(SIGNAL_FREQ);

		cout << "Press u,d,q to raise or lower frequency, or quit: " << flush;
		char c;
		while(cin.get(c)) {
			switch(c) {
			case 'u':
				set_freq_carrier((fl2k_get_sample_rate(fl2k_dev) /SF_RATIO) +1000000);
				break;
			case 'd':
				set_freq_carrier((fl2k_get_sample_rate(fl2k_dev) /SF_RATIO) -1000000);
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
	}


	return 0;
}
