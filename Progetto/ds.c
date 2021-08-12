/* ********************************************************************************************************************************************************
																	DIRETTIVE
******************************************************************************************************************************************************** */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define CMD_LEN 255 // Lunghezza del comando inserito da terminale
#define MAX_ARGOMENTI 3 // Numero massimo di argomenti previsti dai comandi incrementato di uno (utile per controllare che il comando sia stato inserito nel formato corretto)	
#define BUF_LEN 1024 // Lunghezza del buffer di trasmissione e ricezione dei messaggi
#define	VICINO_ASSENTE -1 // Valore utilizzato per comunicare ad un peer la mancanza di uno dei suoi vicini
#define POLLING_TIME 1 // Intertempo di controllo

/* ********************************************************************************************************************************************************
																	STRUTTURE DATI
******************************************************************************************************************************************************** */

// Struttura dati per gestire la memorizzazione di un peer
struct peer{
	uint16_t porta; // Porta del peer
	struct peer *successivo; // Puntatore al vicino destro del peer
	struct peer *precedente; // Puntatore al vicino sinistro del peer
};

// Struttura dati per gestire la rete di peer, implementata come una lista concatenata
struct listaPeer{
	unsigned int numeroPeers; // Numero di peers della rete
	struct peer *primoPeer; // Puntatore al primo peer
	struct peer *ultimoPeer; // Puntatore all'ultimo peer
};

// Struttura per gestire i vicini di un peer
struct neighbourhood{
	struct peer *leftNeighbourPointer; // Puntatore al vicino sinistro
	struct peer *rightNeighbourPointer; // Puntatore al vicino destro
};

// Struttura dati per gli input del thread di gestione del terminale
struct terminalHandlerInputArguments{
	struct listaPeer *peersListPointer; // Puntatore alla lista dei peer
	pthread_mutex_t *peersListMutexPointer; // Puntatore alla variabile mutex per la lista dei peer
	int *puntatoreBooleanoRichiestaEsc; // Puntatore alla variabile booleana che viene settata in caso di richiesta di esc
	pthread_mutex_t *puntatoreMutexBooleanoRichiestaEsc; // Puntatore alla variabile mutex per la variabile booleana che viene settata in caso di richiesta di esc
};

// Struttura dati per gli input del thread di gestione del server in rete
struct networkHandlerInputArguments{
	struct listaPeer *peersListPointer; // Puntatore alla lista dei peer
	pthread_mutex_t *peersListMutexPointer; // Puntatore alla variabile mutex per la lista dei peer
	uint16_t porta; // Porta del server
	int *puntatoreBooleanoRichiestaEsc; // Puntatore alla variabile booleana che viene settata in caso di richiesta di esc
	pthread_mutex_t *puntatoreMutexBooleanoRichiestaEsc; // Puntatore alla variabile mutex per la variabile booleana che viene settata in caso di richiesta di esc
};

/* ********************************************************************************************************************************************************
																DICHIARAZIONE FUNZIONI
******************************************************************************************************************************************************** */

struct neighbourhood inserisciPeer(struct listaPeer*, uint16_t); // Funzione per la registrazione di un peer nella lista
struct neighbourhood eliminaPeer(struct listaPeer*, uint16_t); // Funzione per l'eliminazione di un peer dalla lista

void *terminalHandler(void*); // Funzione per la gestione del terminale
void help(); // Funzione per la gestione del comando 'help'
void showpeers(struct listaPeer*); // Funzione per la gestione del comando 'showpeers'
void showneighbours(struct listaPeer*); // Funzione per la gestione del comando 'showneighbours'
void showneighboursPeer(struct listaPeer*, uint16_t); // Funzione per la gestione del comando 'showneighbours <peer>'

void *networkHandler(void*); // Funzione per la gestione del server in rete

/* ********************************************************************************************************************************************************
																		MAIN
******************************************************************************************************************************************************** */

int main(int argc, char *argv[]){

	// Dichiarazione e inizializzazione delle variabili
	pthread_t networkHandlerID, terminalHandlerID; // Thread IDs per i thread di gestione del terminale e di gestione del server in rete	
	struct listaPeer peersList = {0, NULL, NULL}; // Lista dei peer
	pthread_mutex_t peersListMutex; // Variabile mutex per la lista dei peer
	uint16_t porta = atoi(argv[1]); // Porta del server
	int booleanoRichiestaEsc = 0; // Variabile booleana che viene settata nel caso di richiesta di esc
	pthread_mutex_t mutexBooleanoRichiestaEsc; // Variabile mutex per il booleano che viene settato nel caso di richiesta di esc

	// Inizializzazione degli input dei thread
	struct terminalHandlerInputArguments tHIA = {&peersList, &peersListMutex, &booleanoRichiestaEsc, &mutexBooleanoRichiestaEsc};
	struct networkHandlerInputArguments nHIA = {&peersList, &peersListMutex, porta, &booleanoRichiestaEsc, &mutexBooleanoRichiestaEsc};

	// Inizializzazione delle variabili mutex
	pthread_mutex_init(&peersListMutex, NULL);
	pthread_mutex_init(&mutexBooleanoRichiestaEsc, NULL);

	// Creazione dei thread
	pthread_create(&networkHandlerID, NULL, networkHandler, (void*)&nHIA);
	pthread_create(&terminalHandlerID, NULL, terminalHandler, (void*)&tHIA);

	//Terminazione
	pthread_exit(NULL);

}

/* ********************************************************************************************************************************************************
														FUNZIONI PER LA GESTIONE DELLA LISTA DEI PEER
******************************************************************************************************************************************************** */

// Funzione per la registrazione di un peer nella lista
struct neighbourhood inserisciPeer(struct listaPeer* peersListPointer, uint16_t porta){

	// Dichiarazione e inizializzazione delle variabili
	struct peer *follower; // Puntatore per lo scorrimento della lista dei peer
	struct peer *scout; // Puntatore per lo scorrimento della lista dei peer
	struct neighbourhood neighbours; // Vicini del peer inserito
	struct peer **peerDaInserire; // Puntatore che punta al puntatore al peer da inserire

	// Incremento del numero di peer della lista
	(peersListPointer->numeroPeers)++;

	// Scorrimento della lista
	for (follower = NULL, scout = peersListPointer->primoPeer; (scout != NULL) && (scout->porta < porta); scout = scout -> successivo){
		follower = scout;
	}

	// Inserimento del peer nella lista
	if (follower == NULL){
		peerDaInserire = &(peersListPointer->primoPeer);
	} else {
		peerDaInserire = &(follower->successivo);
	}
	*peerDaInserire = (struct peer*)malloc(sizeof(struct peer));
	(*peerDaInserire)->porta = porta;
	(*peerDaInserire)->successivo = scout;
	(*peerDaInserire)->precedente = follower;

	if (scout == NULL){ // Se il peer risulta essere l'unico nella lista allora aggiorna il valore di 'ultimoPeer'
		peersListPointer->ultimoPeer = *peerDaInserire;
	} else { // Altrimenti aggiorna il valore dell'attributo 'precedente' del peer successivo
		scout->precedente = *peerDaInserire;
	}

	// Terminazione
	neighbours.leftNeighbourPointer = follower;
	neighbours.rightNeighbourPointer = scout;
	return neighbours;

}

// Funzione per l'eliminazione di un peer dalla lista.
struct neighbourhood eliminaPeer(struct listaPeer* peersListPointer, uint16_t porta){

	// Dichiarazione e inizializzazione delle variabili
	struct peer *follower; // Puntatore per lo scorrimento della lista dei peer
	struct peer *scout; // Puntatore per lo scorrimento della lista dei peer
	struct neighbourhood neighbours; // Vicini del peer inserito

	// Scorrimento della lista
	for (follower = NULL, scout = peersListPointer->primoPeer; (scout != NULL) && (scout->porta < porta); scout = scout -> successivo){
		follower = scout;
	}

	// Eliminazione del peer dalla lista
	if (follower == NULL){
		peersListPointer->primoPeer = scout->successivo;
	} else {
		follower->successivo = scout->successivo;
	}
	if (scout->successivo != NULL){
		scout->successivo->precedente = follower;
	} else {
		peersListPointer->ultimoPeer = follower;
	}
	free(scout);

	// Decremento del numero di peer della lista
	(peersListPointer->numeroPeers)--;

	// Terminazione
	neighbours.leftNeighbourPointer = follower;
	neighbours.rightNeighbourPointer = scout->successivo;
	return neighbours;

}

/* ********************************************************************************************************************************************************
																GESTIONE DEL TERMINALE
******************************************************************************************************************************************************** */

// Funzione per la gestione del terminale
void *terminalHandler(void *arg){

	// Dichiarazione e inizializzazione delle variabili
	struct terminalHandlerInputArguments *inputArgument = (struct terminalHandlerInputArguments*)arg;
	struct listaPeer *peersListPointer = inputArgument->peersListPointer; // Puntatore alla lista dei peer
	pthread_mutex_t *peersListMutexPointer = inputArgument->peersListMutexPointer; // Puntatore alla variabile mutex per la lista dei peer
	int *puntatoreBooleanoRichiestaEsc = inputArgument->puntatoreBooleanoRichiestaEsc; // Puntatore alla variabile booleana che viene settata in caso di richiesta di esc
	pthread_mutex_t *puntatoreMutexBooleanoRichiestaEsc = inputArgument->puntatoreMutexBooleanoRichiestaEsc; // Puntatore alla variabile mutex per la variabile booleana che viene settata in caso di richiesta di esc
	char comando[CMD_LEN]; // Variabile che contiene la stringa del comando inserito
	char *argomenti[MAX_ARGOMENTI]; // Variabile che contiene gli argomenti del comando inserito
	int numeroArgomenti; // Numero degli argomenti del comando inserito
	char delimiter[] = " "; // Delimitatore degli argomenti del comando inserito

	// Welcome message
	printf("***************************** DS STARTED *****************************\n");
	printf("Digita un comando:\n");
	printf("1) help --> mostra i dettagli dei comandi\n");
	printf("2) showpeers --> mostra un elenco dei peer connessi\n");
	printf("3) showneighbours <peer> --> mostra i neighbor di un peer\n");
	printf("4) esc --> chiude il DS\n");

	// Prompt e gestione del task
	while (1){
		printf(">> ");
		fgets(comando, CMD_LEN, stdin);

		// Parsing del comando: gli argomenti del comando vengono inseriti nel vettore 'argomenti'
		numeroArgomenti = 0;
		argomenti[numeroArgomenti] = strtok(comando, delimiter);
		while (argomenti[numeroArgomenti] != NULL && numeroArgomenti < 5){
			argomenti[++numeroArgomenti] = strtok(NULL, delimiter);
		}

		// Parsing degli argomenti: vengono analizzati gli argomenti del comando e viene controllato che il comando sia stato inserito con il corretto formato
		if (strcmp(argomenti[0], "help\n") == 0) {
			help();
		} else if (strcmp(argomenti[0], "showpeers\n") == 0) {
			pthread_mutex_lock(peersListMutexPointer);
			showpeers(peersListPointer);
			pthread_mutex_unlock(peersListMutexPointer);
		} else if (strncmp(argomenti[0], "showneighbours", strlen("showneighbours")) == 0) {
			if (strcmp(argomenti[0], "showneighbours\n") == 0){
				pthread_mutex_lock(peersListMutexPointer);
				showneighbours(peersListPointer);
				pthread_mutex_unlock(peersListMutexPointer);
			} else if (strcmp(argomenti[0], "showneighbours") == 0 && numeroArgomenti == 2 && atoi(argomenti[1]) != 0){
				pthread_mutex_lock(peersListMutexPointer);
				showneighboursPeer(peersListPointer, atoi(argomenti[1]));
				pthread_mutex_unlock(peersListMutexPointer);
			}
		} else if (strcmp(argomenti[0], "esc\n") == 0){
			pthread_mutex_lock(puntatoreMutexBooleanoRichiestaEsc);
			*puntatoreBooleanoRichiestaEsc = 1;
			pthread_mutex_unlock(puntatoreMutexBooleanoRichiestaEsc);
			pthread_exit(NULL);
		} else if (strcmp(argomenti[0], "\n") == 0){
			;
		} else {
			printf("Errore: controllare il formato del comando\n");
		}
	}

}

// Funzione per la gestione del comando 'help'
void help(){
	printf("----------");
	printf("\n   HELP   \n");
	printf("----------\n");
	printf("1) help:\n\tmostra il significato dei comandi e ciò che fanno.\n");
	printf("2) showpeers:\n\tmostra l’elenco dei peer connessi alla rete tramite il loro numero di porta.\n");
	printf("3) showneighbours:\n\tmostra i neighbor di un peer specificato come parametro opzionale.\n\tSe non c’è il parametro, il comando mostra i neighbor di ogni peer.\n");
	printf("4) esc:\n\ttermina il DS. La terminazione del DS causa la terminazione di tutti i peer.\n");
	printf("----------\n");
}

// Funzione per la gestione del comando 'showpeers'
void showpeers(struct listaPeer* peersListPointer){

	struct peer *peerPointer;
	int i;

	printf("---------------");
	printf("\n   SHOWPEERS   \n");
	printf("---------------\n");

	if (peersListPointer->numeroPeers == 0){
		printf("Non risultano esserci peer registrati. \n");	
	}

	for(i = 1, peerPointer = peersListPointer->primoPeer; peerPointer != NULL; peerPointer = peerPointer->successivo, i++){
		printf("Peer n.%d: porta %d\n", i, peerPointer->porta);
	}

	printf("---------------\n");

}

// Funzione per la gestione del comando 'showneighbours'
void showneighbours(struct listaPeer *peersListPointer){

	struct peer *peerPointer;
	int i;

	printf("--------------------");
	printf("\n   SHOWNEIGHBOURS   \n");
	printf("--------------------\n");

	switch(peersListPointer->numeroPeers){
	case 0:
		printf("Non risultano esserci peer registrati.\n");
		break;
	case 1:
		printf("Risulta esserci un unico peer registrato sulla porta %d.\n", peersListPointer->primoPeer->porta);
		break;
	default:
		printf("Neighbours del peer n.1 (porta %d): peer n.2 (porta %d).\n", peersListPointer->primoPeer->porta, peersListPointer->primoPeer->successivo->porta);
		for (i = 2, peerPointer = peersListPointer->primoPeer->successivo; i < peersListPointer->numeroPeers; peerPointer = peerPointer->successivo, i++){
			printf("Neighbours del peer n.%d (porta %d): peer n.%d (porta %d), peer n.%d (porta %d).\n", i, peerPointer->porta, i + 1, peerPointer->successivo->porta, i - 1, peerPointer->precedente->porta);
		}
		printf("Neighbours del peer n.%d (porta %d): peer n.%d (porta %d).\n", peersListPointer->numeroPeers, peersListPointer->ultimoPeer->porta, peersListPointer->numeroPeers - 1, peersListPointer->ultimoPeer->precedente->porta);
	}

	printf("--------------------\n");

}

// Funzione per la gestione del comando 'showneighbours <peer>'
void showneighboursPeer(struct listaPeer *peersListPointer, uint16_t peer){

	struct peer *peerPointer;
	int i;

	for (i = 0, peerPointer = peersListPointer->primoPeer; i < peersListPointer->numeroPeers; peerPointer = peerPointer->successivo, i++){
		if (peer == peerPointer->porta){
			if (peerPointer->precedente == NULL){
				if (peerPointer->successivo == NULL){
					printf("Il peer selezionato risulta essere l'unico della rete.\n");
				} else {
					printf("Neighbours del peer n.%d (porta %d): peer n.%d (porta %d).\n", i, peerPointer->porta, i + 1, peerPointer->successivo->porta);
				}
			} else {
				if (peerPointer->successivo == NULL){
					printf("Neighbours del peer n.%d (porta %d): peer n.%d (porta %d).\n", i, peerPointer->porta, i - 1, peerPointer->precedente->porta);
				} else {
					printf("Neighbours del peer n.%d (porta %d): peer n.%d (porta %d), peer n.%d (porta %d).\n", i, peerPointer->porta, i + 1, peerPointer->successivo->porta, i - 1, peerPointer->precedente->porta);
				}
			}
			return;
		}
	}
	
	printf("Il peer selezionato non risulta essere registrato.\n");

}

/* ********************************************************************************************************************************************************
															GESTIONE DEL SERVER IN RETE
******************************************************************************************************************************************************** */

// Funzione per la gestione del server in rete
void *networkHandler(void *arg){

	// Dichiarazione e inizializzazione delle variabili
	struct networkHandlerInputArguments *inputArgument = (struct networkHandlerInputArguments*)arg;
	struct listaPeer *peersListPointer = inputArgument->peersListPointer; // Puntatore alla lista dei peer
	pthread_mutex_t *peersListMutexPointer = inputArgument->peersListMutexPointer; // Puntatore alla variabile mutex per la lista dei peer
	uint16_t porta = inputArgument->porta; // Porta del server
	int *puntatoreBooleanoRichiestaEsc = inputArgument->puntatoreBooleanoRichiestaEsc; // Puntatore alla variabile booleana che viene settata in caso di richiesta di esc
	pthread_mutex_t *puntatoreMutexBooleanoRichiestaEsc = inputArgument->puntatoreMutexBooleanoRichiestaEsc; // Puntatore alla variabile mutex per la variabile booleana che viene settata in caso di richiesta di esc
	int sd; // Identificatore del socket del server
	int ret; // Variabile per gestire il return delle varie funzioni
	struct sockaddr_in my_addr; // Indirizzo del server
	struct sockaddr_in connecting_addr; // Indirizzo del peer che richiede la connessione al server
	struct sockaddr_in peer_addr; // Indirizzo del peer da contattare
	int addr_len = sizeof(struct sockaddr_in); // Lunghezza degli indirizzi dei peer e del server
	char buffer[BUF_LEN]; // Buffer per l'invio e la ricezione di messaggi
	struct neighbourhood neighbours; // Variabile per memorizzare i neighbours di un peer
	struct peer *peerScanner; // Variabile per scorrere i peer della lista

	// Creazione e inizializzazione del socket del server
	sd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	memset(&my_addr, 0, addr_len);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(porta);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	// Bind del socket del server
	ret = bind(sd, (struct sockaddr*)&my_addr, addr_len);
	if (ret < 0){
		printf("\n*** ERRORE: Bind non riuscito ***\n");
		exit(0);
	}

	// Inizializzazione dell'indirizzo del peer a cui spedire messaggi ('sin_port' verrà inizializzato in seguito, a seconda del vicino)
	memset(&peer_addr, 0, addr_len);
	peer_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

	while(1){

		// -------------------------------------------
		// Gestione delle richieste fatte da terminale
		// -------------------------------------------

		pthread_mutex_lock(puntatoreMutexBooleanoRichiestaEsc);

		// Gestione del comando di esc
		if (*puntatoreBooleanoRichiestaEsc == 1){
			// Invio del messaggio di server down
			sprintf(buffer, "SERDOWN");
			for (peerScanner = peersListPointer->primoPeer; peerScanner != NULL; peerScanner = peerScanner->successivo){
				peer_addr.sin_port = htons(peerScanner->porta);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}
			printf("... Disconnessione del server effettuata con successo.\n");
			pthread_mutex_unlock(puntatoreMutexBooleanoRichiestaEsc);
			close(sd);
			pthread_exit(NULL);
		}

		pthread_mutex_unlock(puntatoreMutexBooleanoRichiestaEsc);

		// ----------------------------------------
		// Gestione della ricezione di un messaggio
		// ----------------------------------------

		ret = recvfrom(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, (socklen_t*)&addr_len);
		if (ret < 0){
			continue;
		}

		// Gestione del messaggio di richiesta di registrazione
		if (strcmp(buffer, "BOOTREQ") == 0){
			pthread_mutex_lock(peersListMutexPointer);

			// Inserimento del peer nella lista
			neighbours = inserisciPeer(peersListPointer, ntohs(connecting_addr.sin_port));
			// Invio del messaggio contenente le porte dei vicini del nuovo peer
			sprintf(buffer, "%d %d", neighbours.leftNeighbourPointer == NULL ? VICINO_ASSENTE : neighbours.leftNeighbourPointer->porta, neighbours.rightNeighbourPointer == NULL ? VICINO_ASSENTE : neighbours.rightNeighbourPointer->porta);
			do {
				ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, addr_len);
				if (ret < 0){
					sleep(POLLING_TIME);
				}
			} while (ret < 0);

			// Aggiornamento del vicino sinistro del nuovo peer, se questo esiste
			if (neighbours.leftNeighbourPointer != NULL){
				peer_addr.sin_port = htons(neighbours.leftNeighbourPointer->porta);
				sprintf(buffer, "UPDTNGB %d %d", neighbours.leftNeighbourPointer->precedente == NULL? VICINO_ASSENTE : neighbours.leftNeighbourPointer->precedente->porta, ntohs(connecting_addr.sin_port));
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}
			// Aggiornamento del vicino destro del nuovo peer, se questo esiste
			if (neighbours.rightNeighbourPointer != NULL){
				peer_addr.sin_port = htons(neighbours.rightNeighbourPointer->porta);
				sprintf(buffer, "UPDTNGB %d %d", ntohs(connecting_addr.sin_port), neighbours.rightNeighbourPointer->successivo == NULL? VICINO_ASSENTE : neighbours.rightNeighbourPointer->successivo->porta);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}

			pthread_mutex_unlock(peersListMutexPointer);
			continue;
		}

		// Gestione del messaggio di richiesta di eliminazione
		if (strcmp(buffer, "STOPCON") == 0){
			pthread_mutex_lock(peersListMutexPointer);

			// Eliminazione del peer dalla lista
			neighbours = eliminaPeer(peersListPointer, ntohs(connecting_addr.sin_port));

			// Aggiornamento del vicino sinistro del nuovo peer, se questo esiste
			if (neighbours.leftNeighbourPointer != NULL){
				peer_addr.sin_port = htons(neighbours.leftNeighbourPointer->porta);
				sprintf(buffer, "UPDTNGB %d %d", neighbours.leftNeighbourPointer->precedente == NULL? VICINO_ASSENTE : neighbours.leftNeighbourPointer->precedente->porta, neighbours.rightNeighbourPointer == NULL? VICINO_ASSENTE : neighbours.rightNeighbourPointer->porta);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}
			// Aggiornamento del vicino destro del nuovo peer, se questo esiste
			if (neighbours.rightNeighbourPointer != NULL){
				peer_addr.sin_port = htons(neighbours.rightNeighbourPointer->porta);
				sprintf(buffer, "UPDTNGB %d %d", neighbours.leftNeighbourPointer == NULL? VICINO_ASSENTE : neighbours.leftNeighbourPointer->porta, neighbours.rightNeighbourPointer->successivo == NULL? VICINO_ASSENTE : neighbours.rightNeighbourPointer->successivo->porta);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}

			pthread_mutex_unlock(peersListMutexPointer);
		}

	}

}
