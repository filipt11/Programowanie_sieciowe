#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <sys/shm.h>

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
int semid;
int i;
int m;
char buf[128];
struct mydata *shared_data;

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "Za malo podanych argumentow");
		exit(1);
	}
	if (argc > 4)
	{
		fprintf(stderr, "Za duzo podanych argumentow");
		exit(1);
	}

	char n = argv[2][0];
	if (n != 'N' && n != 'P')
	{
		//w linijce ponizej dodalem '\n' juz po nakreceniu filmu
		fprintf(stderr,"Zly drugi argument, (podaj 'N' aby napisac nowy post, lub 'P' aby polubic\n");
		exit(1);
	}

	printf("Twitter 2.0 wita! (wersja B)\n");

	if ((shmkey = ftok(argv[1], 1)) == -1)
	{
		perror("Blad tworzenia klucza");
		exit(1);
	}

	if ((semid = semget(shmkey, 1, 0600)) == -1)
	{
		perror("Blad semid");
		exit(1);
	}
	struct sembuf sb;
	sb.sem_num = 0;
	sb.sem_op = -1;
	sb.sem_flg = 0;

	if (semop(semid, &sb, 1) == -1)
	{
		perror("Blad semop");
		exit(1);
	}

	if ((shmid = shmget(shmkey, 0, 0600)) == -1)
	{
		perror("Blad shmget");
		exit(1);
	}
	shared_data = (struct mydata *)shmat(shmid, (void *)0, 0);
	if (shared_data == (struct mydata *)-1)
	{
		perror("Blad shmat");
		exit(1);
	}

	if (n == 'P')
	{
		printf("Wpisy w serwisie:\n");

		for (i = 0; i < shared_data->ilosc; i++)
		{
			if (shared_data[i].txt[0] != '\0')
			{
				printf("	%d. %s [Autor: %s, Polubienia: %d]\n", i + 1, shared_data[i].txt, shared_data[i].user, shared_data[i].licznik);
				fflush(stdout);
			}
		}
		printf("Ktory wpis chcesz polubic\n");
		scanf("%d", &m);
		if (m < 1 || m > shared_data->ilosc || shared_data[m - 1].txt[0] == '\0')
		{
			fprintf(stderr, "Niepoprawny numer wpisu\n");
			exit(1);
		}
		shared_data[m - 1].licznik++;
		printf("Dziekuje za skorzystanie z aplikacji Twitter 2.0\n");
	}

	if (n == 'N')
	{
		if (argc < 4)
		{
			fprintf(stderr, "Brak nazwy uzytkownika\n");
			sb.sem_op = 1;
			if (semop(semid, &sb, 1) == -1)
			{
				perror("Blad odblokowania semafora");
				exit(1);
			}
			exit(1);
		}
		if (shared_data->wolne < 1)
		{
			fprintf(stderr, "Brak wolnego miejsca na wpisy\n");
			sb.sem_op = 1;
			if (semop(semid, &sb, 1) == -1)
			{
				perror("Blad odblokowania semafora");
				exit(1);
			}
			exit(1);
		}
		printf("[Wolnych %d wpisow (na %d)]\n", shared_data->wolne, shared_data->ilosc);
		printf("Podaj swoj wpis:\n");
		fgets(buf, 128, stdin);
		buf[strlen(buf) - 1] = '\0';
		for (i = 0; i < shared_data->ilosc; i++)
		{
			if (shared_data[i].txt[0] == '\0')
			{
				strcpy(shared_data[i].txt, buf);
				strncpy(shared_data[i].user, argv[3], sizeof(shared_data[i].user) - 1);
				shared_data[i].user[sizeof(shared_data[i].user) - 1] = '\0';
				shared_data->wolne--;
				break;
			}
		}
		printf("Dziekuje za skorzystanie z aplikacji Twitter 2.0\n");
	}
	sb.sem_op = 1;
	if (semop(semid, &sb, 1) == -1)
	{
		perror("Blad odblokowania semafora");
		exit(1);
	}

	return 0;
}
