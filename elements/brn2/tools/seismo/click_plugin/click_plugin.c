#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* strlen, strcpy */
#include <assert.h>
#include "../../plugin.h"

#include "brn_socket.h"

/*
 * click plugin clicknstrating the use of plugin libraries
 */

static char *       click_plugin_sta_name;     /* Name of station */ 
static unsigned int click_plugin_channels;     /* Number of channels */
static unsigned int click_plugin_sampling_rate;/* Sampling rate (e.g. 100) */

static struct socket_connection* click_connection = NULL;

struct click_seismo_data_header {
  int gps_lat;
  int gps_long;
  int gps_alt;
  int gps_hdop;

  int sampling_rate;
  int samples;
  int channels;
};

struct click_seismo_data {
  uint64_t time;
  int timing_quality;
};

static struct click_seismo_data_header *curr_click_data; 
static uint8_t *data_index;

static char* buffer = NULL;
static int buffer_size;

static int count_samples_per_block;
static int sample_index;

/*
 * To be called by parser when new sensor data is available.
 */
static void click_plugin_sensor_callback(const uint64_t time, int timing_quality, int32_t data[])
{
  unsigned int i;
  struct click_seismo_data *next_data;
  int32_t *data_p;

  next_data = (struct click_seismo_data *)data_index;

  next_data->time = time;
  next_data->timing_quality = htonl(timing_quality);

  data_index = (uint8_t*)&(next_data[1]);
  data_p = (int32_t *)data_index;

  for ( i = 0; i < click_plugin_channels; i++) data_p[i] = htonl(data[i]);

  data_index += (click_plugin_channels * sizeof(int32_t));

  sample_index++;

  if ( sample_index == count_samples_per_block ) {
    send_to_peer(click_connection, buffer, buffer_size);
    data_index = (uint8_t *)&curr_click_data[1];
    sample_index = 0;
  }
}

/*
 * To be called by parser when new positional data is available.
 */
static void click_plugin_pos_callback(int lng, int lat, int alt, unsigned int hdop)
{
  //printf("click_plugin POS: long=%i lat=%i alt=%i hdop=%u\n", lng, lat, alt, hdop);
  curr_click_data->gps_lat = htonl(lat);
  curr_click_data->gps_long = htonl(lng);
  curr_click_data->gps_alt = htonl(alt);
  curr_click_data->gps_hdop = htonl(hdop);
}

/*
 * Entry point of plugin, called by the parser at startup time.
 * If the return value is != 0, the plugin is unloaded.
 */
int plugin_load(const char* station_name, int ch_num, int sampling_rate)
{
  int user_sample_rate;

  user_sample_rate = 10;

  printf("click_plugin loading ...\n");
  printf("click_plugin station name: %s number of channels: %i sampling rate: %i\n", station_name, ch_num, sampling_rate);
  click_plugin_channels = ch_num;
  click_plugin_sampling_rate = sampling_rate;
  click_plugin_sta_name = malloc((strlen(station_name)+1)*sizeof(char));
  assert(click_plugin_sta_name);
  strcpy(click_plugin_sta_name, station_name);

  /* Register callbacks at server */
  printf("click_plugin Registering callbacks\n");
  plugin_sensors_register(&click_plugin_sensor_callback);
  plugin_position_register(&click_plugin_pos_callback);

  /* open socket*/
  printf("click_plugin open socket\n");

  if ( click_connection == NULL ) {
    click_connection = open_socket_connection("127.0.0.1", 8086, 8087);
  }

  if ( user_sample_rate > sampling_rate ) count_samples_per_block = 1;
  else count_samples_per_block = sampling_rate / user_sample_rate;

  buffer_size = sizeof(struct click_seismo_data_header) + (count_samples_per_block * (sizeof(struct click_seismo_data) + (click_plugin_channels * sizeof(int32_t))));
  buffer = (char*)malloc(buffer_size);
  curr_click_data = (struct click_seismo_data_header*)buffer;
  data_index = (uint8_t *)&curr_click_data[1];
  sample_index = 0;

  curr_click_data->sampling_rate = htonl(sampling_rate);
  curr_click_data->samples = htonl(count_samples_per_block);
  curr_click_data->channels = htonl(ch_num);

  printf("Click_plugin: sampling_rate: %d samples: %d channels: %d buffer_size: %d\n",sampling_rate,count_samples_per_block,ch_num,buffer_size);

  /* Everything OK */
  printf("click_plugin loading finished\n");
  return 0;
}

/*
 * To be called when plugin should be unloaded.
 * FIXME: Currently this is not called at all.
 */
void plugin_unload()
{
  printf("click_plugin gets unloaded\n");
  plugin_sensors_deregister(&click_plugin_sensor_callback);
  plugin_position_deregister(&click_plugin_pos_callback);
  free(click_plugin_sta_name);

  if ( click_connection != NULL ) {
    close_connection(click_connection);
  }

  curr_click_data = NULL;
  free(buffer);
  buffer = NULL;
  buffer_size = 0;
  data_index = NULL;

  printf("click_plugin unload finish\n");
}
