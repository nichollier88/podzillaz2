#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pz.h"

#define path "/usr/lib/mpd/mpd"
#define mpd_cfg "/etc/podzilla/modules/mpd/"
#define conf "/etc/podzilla/modules/mpd/mpd.conf"
#define db "/etc/podzilla/modules/mpd/mpddb"

static PzModule *module;

static int send_command(char *str)
{
	int sock;
	struct hostent *he;
	struct sockaddr_in addr;
	char *hostname = getenv("MPD_HOST");
	char *port = getenv("MPD_PORT");

	if (hostname == NULL) hostname = "127.0.0.1";
	if (port == NULL) port = "6600";

	if ((he = gethostbyname(hostname)) == NULL) {
		herror(hostname);
		return 1;
	}
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return 2;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr = *((struct in_addr *)he->h_addr);
	addr.sin_port = htons(atoi(port));
	memset(&(addr.sin_zero), 0, 8);
	if (connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr))==-1){
		perror("connect");
		close(sock);
		return 3;
	}
	send(sock, str, strlen(str), 0);
	send(sock, "\n", 1, 0);
	close(sock);
	return 0;
}

static void init_conf()
{
	if (!(access(conf, F_OK) == 0)) {
		FILE *fconf = fopen(conf, "w");
		fprintf(fconf,
		"port 			\"6600\"\n"
		"music_directory 	\"/mnt\"\n"
		"playlist_directory 	\"%s\"\n"
		"log_file 		\"%smessages.log\"\n"
		"error_file 		\"%serror.log\"\n"
		"#	Usually this is either:\n"
		"#	ISO-8859-1 or UTF-8\n"
		"filesystem_charset	\"ISO-8859-1\"\n"
		"db_file		\"%smpddb\"\n"
		"state_file		\"%sstate\"\n"
		"mixer_control		\"PCM\"\n\n"
		"audio_output {\n\ttype \"oss\"\n\tname \"oss\"\n}\n",
		mpd_cfg, mpd_cfg, mpd_cfg, mpd_cfg, mpd_cfg);
		fclose(fconf);
	}
}

static void create_db()
{
	if (!(access(db, F_OK) == 0)) {
		FILE *fdb = fopen(db, "w");
		fprintf(fdb,
		"info_begin\n"
    "mpd_version: mpd-ke\n"
    "fs_charset: ISO-8859-1\n"
    "info_end\n"
    "songList begin\n"
    "songList end\n");
  	fclose(fdb);
	}
}

static void init_loopback()
{
	switch (vfork()) {
		case 0:
			execl("/bin/ifconfig", "ifconfig", "lo", "127.0.0.1", NULL);
		case -1:
			pz_perror("Unable to initialize loopback interface");
			break;
		default:
			wait(NULL);
			break;
	}
}

void kill_mpd()
{
	send_command("kill");
}

void init_mpd()
{
        init_loopback();
        switch (vfork()) {
                case 0:
                         execl(path, path, conf, NULL);
                case -1: 
                         pz_perror("Unable to start MPD");
                         break;
	        default: 
                         wait(NULL);
                         break;
	}

	putenv("MPD_PORT=6600");
	putenv("MPD_HOST=127.0.0.1");
	while (send_command(""));

}

PzWindow *db_do_update()
{
        const char *const argv[] = { path, "--update-db", conf, NULL };
        pz_execv(path, (char *const *)argv);
        return TTK_MENU_DONOTHING;
}

static void mpd_init()
{
	module = pz_register_module("mpd", kill_mpd);
	 	
	pz_menu_add_action_group("/Settings/Music/Update BD", "setting", db_do_update);
	
        init_conf();
	create_db();
	init_mpd();
}


PZ_MOD_INIT(mpd_init)
