#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/ipc.h>

struct mydata
{
	int licznik;
	char txt[128];
	char user[20];
	int ilosc;
	int wolne;
};

key_t shmkey;
int shmid;
struct mydata *shared_data;
int i;
int n;
int semid;

void sig(int nr)
{
	printf("\n[Serwer]: dostalem SIGINT => koncze i sprzatam...");
	printf(" (odlaczanie: %s, usuniecie :%s)\n",
		   (shmdt(shared_data) == 0) ? "OK" : "blad shmdt",
		   (shmctl(shmid, IPC_RMID, 0) == 0) ? "OK" : "blad shmctl");
	if (semctl(semid, 0, IPC_RMID) == -1)
	{
		printf("\nBlad usuwania semafora\n");
	}
	else
	{
		printf("[Serwer]: usuniecie semafora: OK\n");
	}
	exit(0);
}
void sig1(int nr)
{
	if (shared_data->txt[0] == '\0')
	{
		printf("\nBrak wpisow\n");
	}
	else
	{
		printf("\n__________ Twitter 2.0: __________\n");
		for (i = 0; i < shared_data->ilosc; i++)
		{
			if (shared_data[i].txt[0] != '\0')
			{
				printf("[%s]: %s [Polubienia: %d]\n", shared_data[i].user, shared_data[i].txt, shared_data[i].licznik);
				fflush(stdout);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Brak nazwy pliku do wygenerowania klucza i brak liczby wpisow\n");
		exit(1);
	}

	if (argc < 3)
	{
		fprintf(stderr, "Brak liczby wpisow\n");
		exit(1);
	}
	if (argc > 3)
	{
		fprintf(stderr, "Za duzo podanych argumentow\n");
		exit(1);
	}

	struct shmid_ds buf;
	int n = atoi(argv[2]);
	signal(SIGINT, sig);
	signal(SIGTSTP, sig1);
	printf("[Serwer]: Twitter 2.0 (wersja B)\n");
	printf("[Serwer]: tworze klucz na podstawie pliku %s...", argv[1]);

	if ((shmkey = ftok(argv[1], 1)) == -1)
	{
		perror("Blad tworzenia klucza");
		exit(1);
	}
	printf(" OK (klucz: %d)\n", shmkey);

	if ((semid = semget(shmkey, 1, 0600 | IPC_CREAT | IPC_EXCL)) == -1)
	{
		perror("Blad semid");
		exit(1);
	}

	if (semctl(semid, 0, SETVAL, 1) == -1)
	{
		perror("Blad semctl");
		exit(1);
	}

	printf("[Serwer]: tworze segment pamieci wspolnej na %d wpisow po 128b...", n);

	if ((shmid = shmget(shmkey, n * sizeof(struct mydata), 0600 | IPC_CREAT | IPC_EXCL)) == -1)
	{
		perror("Blad shmget");
		exit(1);
	}
	shmctl(shmid, IPC_STAT, &buf);
	printf(" OK (id: %d, rozmiar: %zub)\n", shmid, buf.shm_segsz);
	printf("[Serwer]: dolaczam pamiec wspolna...");
	shared_data = (struct mydata *)shmat(shmid, (void *)0, 0);
	if (shared_data == (struct mydata *)-1)
	{
		perror(" Blad shmat\n");
		exit(1);
	}
	shared_data->ilosc = n;
	shared_data->wolne = n;
	printf(" OK (adres: %lx)\n", (long int)shared_data);

	shared_data->txt[0] = '\0';

	printf("nacisnij Ctrl^Z by wyswietlac stan serwisu\n");
	printf("nacisnij Ctrl^C by zakonczyc program\n");

	while (1)
	{

		sleep(1);
	}

	return 0;
}
