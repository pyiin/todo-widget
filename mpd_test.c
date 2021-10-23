#include <stdio.h>
#include <mpd/client.h>
#include <string.h>

int main(){
	struct mpd_connection *conn = mpd_connection_new("localhost", 0, 0);
	struct mpd_status* stat = mpd_run_status(conn);

	/* get song name (and length) */
	mpd_send_current_song(conn);
	struct mpd_song* now_playing = mpd_recv_song(conn);


	char* name = mpd_song_get_tag(now_playing, MPD_TAG_TITLE, 0);
	printf("%s\n", name);
	printf("%d\n", mpd_status_get_kbit_rate(stat));
	unsigned a = mpd_song_get_duration(now_playing);

	/* current song status */
	unsigned b = mpd_status_get_elapsed_time(stat);

	/* ------------------ */
	char time[100];
	sprintf(time, "%u/%u ", b,a);
	strcat(time, name);
	printf("%s\n", time);

	// cleanup
	mpd_connection_free(conn);
	mpd_song_free(now_playing);

}
