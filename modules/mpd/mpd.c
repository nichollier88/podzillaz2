#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>

#include "pz.h"

static PzModule *module;

static const char *mpd_binary = "/usr/bin/mpd";

static void get_mpd_path(char *buffer, size_t size, const char *suffix)
{
	const char *home = getenv("HOME");
	if (!home)
		home = "/root";

	snprintf(buffer, size, "%s/podzilla/modules/mpd%s%s", 
		home, suffix ? "/" : "", suffix ? suffix : "");
}

void print_reply(int sock)
{
	struct timeval timeout;
	fd_set readfds;

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	if (!select(sock + 1, &readfds, NULL, NULL, &timeout))
		return;

	if (!FD_ISSET(sock, &readfds))
		return;

	char buffer[1024];
	int len = recv(sock, buffer, sizeof(buffer) - 1, 0);

	if (len < 1)
	{
		printf("No data received or connection closed\n");
		return;
	}

	buffer[len] = '\0'; // Null-terminate the received data
	printf("Response from MPD: %s\n", buffer);
}

static int send_command(char *str)
{
	int sock;
	struct hostent *he;
	struct sockaddr_in addr;
	char *hostname = getenv("MPD_HOST");
	char *port = getenv("MPD_PORT");

	if (hostname == NULL)
		hostname = "127.0.0.1";
	if (port == NULL)
		port = "6600";

	if ((he = gethostbyname(hostname)) == NULL)
	{
		herror(hostname);
		return 1;
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return 2;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr = *((struct in_addr *)he->h_addr);
	addr.sin_port = htons(atoi(port));
	memset(&(addr.sin_zero), 0, 8);
	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
	{
		perror("connect");
		close(sock);
		return 3;
	}
	send(sock, str, strlen(str), 0);
	send(sock, "\nclose", 1, 0);
	print_reply(sock);

	close(sock);
	return 0;
}

static void init_conf()
{
	char mpd_dir[256];
	char config[256];

	get_mpd_path(mpd_dir, sizeof(mpd_dir), NULL);
	get_mpd_path(config, sizeof(config), "mpd.conf");

	if (!(access(config, F_OK) == 0))
	{
		FILE *fconf = fopen(config, "w");
		fprintf(fconf,
				"music_directory		\"%s/music\"\n"
				"playlist_directory 	\"%s/playlists\"\n"
				"db_file				\"%s/mpddb\"\n"
				"pid_file				\"%s/pid\"\n"
				"state_file				\"%s/state\"\n"
				"#user					\"mpd\"\n"
				"port					\"6600\"\n"
				"bind_to_address		\"localhost\"\n"
				"log_file				\"%s/messages.log\"\n"
				"#	Usually this is either:\n"
				"#	ISO-8859-1 or UTF-8\n"
				"filesystem_charset		\"ISO-8859-1\"\n"
				"input {\n\tplugin \"curl\"\n}\n"
				"decoder {\n\tplugin \"hybrid_dsd\"\n\tenabled \"no\"\n}\n"
				"decoder {\n\tplugin \"wildmidi\"\n\tenabled \"no\"\n}\n"
				"audio_output {\n\ttype \"pipewire\"\n\tname \"Pipewire Output\"\n}\n",
				mpd_dir, mpd_dir, mpd_dir, mpd_dir, mpd_dir, mpd_dir);
		fclose(fconf);
	}
}

static void create_db()
{
	char db[256];
	get_mpd_path(db, sizeof(db), "mpddb");

	if (!(access(db, F_OK) == 0))
	{
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
	switch (vfork())
	{
	case 0:
		execl("/usr/sbin/ifconfig", "ifconfig", "lo", "127.0.0.1", NULL);
	case -1:
		pz_perror("Unable to initialize loopback interface");
		_exit(127);
		break;
	default:
		wait(NULL);
		break;
	}
}

static void kill_mpd()
{
	// send_command("kill");

	char pid_filepath[256];
	get_mpd_path(pid_filepath, sizeof(pid_filepath), "pid");

	FILE *file = fopen(pid_filepath, "r");
	if (!file)
	{
		perror("Failed to open PID file");
		return;
	}

	pid_t pid;

	if (fscanf(file, "%d", &pid) != 1)
	{
		perror("Failed to read PID");
		fclose(file);
		return;
	}

	fclose(file);

	if (kill(pid, SIGTERM) != 0)
	{
		perror("Failed to kill process");
		return;
	}

	printf("Successfully sent SIGTERM to process %d\n", pid);
}

static void init_mpd()
{
	char config[256];
	get_mpd_path(config, sizeof(config), "mpd.conf");

	init_loopback();
	switch (vfork())
	{
	case 0:
		execl(mpd_binary, mpd_binary, config, NULL);
	case -1:
		pz_perror("Unable to start MPD");
		_exit(127);
		break;
	default:
		wait(NULL);
		break;
	}

	putenv("MPD_PORT=6600");
	putenv("MPD_HOST=127.0.0.1");

	int retries = 50;
	while (send_command("") != 0 && retries-- > 0)
	{
		usleep(100000);
	}
}

PzWindow *db_do_update()
{
	if (send_command("update"))
		pz_message("Error requesting update");
	else
		pz_message("Update requested");

	return TTK_MENU_DONOTHING;
}

static void mpd_init()
{
	struct stat st;
	if (stat(mpd_binary, &st) == 0)
	{
		if (!(st.st_mode & S_IXUSR))
			chmod(mpd_binary, S_IRWXU);
	}

	module = pz_register_module("mpd", kill_mpd);

	pz_menu_add_action_group("/Settings/Music/Update BD", "setting", db_do_update);

	init_conf();
	create_db();
	init_mpd();
}

PZ_MOD_INIT(mpd_init)
