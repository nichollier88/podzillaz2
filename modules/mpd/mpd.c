#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pz.h"

static PzModule *module;

static void cleanup_mpd()
{
#ifdef IPOD
	int sock;
	struct hostent *he;
	struct sockaddr_in addr;
	char *hostname = getenv("MPD_HOST");
	char *port = getenv("MPD_PORT");

	if (hostname == NULL) hostname = "127.0.0.1";
	if (port == NULL) port = "6600";

	if ((he = gethostbyname(hostname)) == NULL) {
		herror(hostname);
		return;
	}
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr = *((struct in_addr *)he->h_addr);
	addr.sin_port = htons(atoi(port));
	memset(&(addr.sin_zero), 0, 8);
	if (connect(sock,(struct sockaddr *)&addr,sizeof(struct sockaddr))==-1){
		perror("connect");
		return;
	}
	send(sock, "kill\n", 5, 0);
	close(sock);
#endif
}

static void check_conf(const char *filename)
{
	FILE *fp;
	char line[1024];

	if ((fp = fopen(filename, "a+")) == NULL)
		return;

	while (fgets(line, 1024, fp)) {
		if (!strncmp(line, "audio_output", 12)) {
			fclose(fp);
			return;
		}
	}
	fputs("\naudio_output {\n\ttype \"oss\"\n\tname \"oss\"\n}\n", fp);
	fclose(fp);
}

static char *init_conf(const char *filename)
{
	FILE *fp;
	const char *path;
	char iTunes = !access("/iPod_Control/Music", R_OK);
	if ((fp = fopen(filename, "w")) == NULL)
		return NULL;

	path = pz_module_get_cfgpath(module, ""),
	fprintf(fp,
		"port 			\"6600\"\n"
		"music_directory 	\"%s\"\n"
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
		iTunes ? "/iPod_Control/Music" : "/mnt",
		path, path, path, path, path);
	fclose(fp);
	return (char *)filename;
}

static void init_mpd()
{
#ifdef IPOD
	const char *path, *conf;
	module = pz_register_module("mpd", cleanup_mpd);
	path = pz_module_get_datapath(module, "mpd");
	conf = pz_module_get_cfgpath(module, "mpd.conf");
	switch (vfork()) {
	case 0: execl("/sbin/ifconfig", "ifconfig", "lo", "127.0.0.1", NULL);
	case -1: pz_perror("Unable to initialize loopback interface");
		 break;
	default: wait(NULL);
		 break;
	}
	if (access(conf, F_OK)) conf = init_conf(conf);
	else {
		check_conf(conf);
		switch (vfork()) {
		case 0: execl(path, path, "--update-db", conf, NULL);
		case -1: pz_perror("MPD Update failed");
			 break;
		default: wait(NULL);
			 break;
		}
	}
	switch (vfork()) {
	case 0: execl(path, path, conf, NULL);
	case -1: pz_perror("Unable to start MPD");
		 break;
	default: wait(NULL);
		 break;
	}
	putenv("MPD_PORT=6600");
	putenv("MPD_HOST=127.0.0.1");
	sleep(1); /* yes, cheap hack, I know. */
#endif
}

PZ_MOD_INIT(init_mpd)
