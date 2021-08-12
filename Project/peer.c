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
#include <time.h>

#define BUF_LEN 1024 // Lunghezza del buffer di trasmissione e ricezione dei messaggi
#define CMD_LEN 255 // Lunghezza del comando da inserire da terminale
#define MSGTYPE_LEN 7 // Lunghezza dei comandi
#define	VICINO_ASSENTE -1 // Variabile utilizzata per comunicare ad un peer l'assenza di uno dei suoi vicini
#define POLLING_TIME 1 // Intertempo di controllo
#define MAX_DATI 8096 // Numero massimo di dati gestibili dal peer
#define MAX_GIORNI (2*365) // Numero massimo di giorni dei quali gestire l'analisi dati
#define MAX_ARGOMENTI 5 // Numero massimo di argomenti previsti dai comandi incrementato di uno (utile per controllare che il comando sia stato inserito nel formato corretto)
#define ORA_CHIUSURA 18 // Ora limite per l'inserimento di dati
#define NON_TROVATO -1 // Risultato richiesto non trovato
#define MAX_PEER 256 // Numero massimo dei peer
#define ANNO_INIZIO 2021 // Anno di inizio dell'analisi dati
#define MESE_INIZIO 7 // Mese di inizio dell'analisi dati
#define GIORNO_INIZIO 20 // Giorno di inizio dell'analisi dati

/* ********************************************************************************************************************************************************
																	STRUTTURE DATI
******************************************************************************************************************************************************** */

// Enumerazione per i tipi di dato
enum tipoDato{
	T, // Tipo 'tampone'
	N // Tipo 'nuovo caso'
};

// Enumerazione per i tipi di aggregazione
enum tipoAggregazione{
	totale, // Tipo 'totale'
	variazione // Tipo 'variazione'
};

// Struttura per le date
struct data{
	int anno, mese, giorno; // Caratterizzazione della data
};

// Struttura per i dati
struct dato{
	struct data data; // Data di inserimento del dato
	enum tipoDato tipo; // Tipo del dato
	int valore; // Quantità del dato
};

// Struttura per le aggregazioni di tipo 'totale' giornaliere
struct risultatoTotaleGiornaliero{
	struct data data; // Data del risultato
	enum tipoDato tipo; // Tipo dei dati dell'aggregazione
	int valore; // Valore del risultato
};

// Struttura per gestire i periodi delle operazioni di aggregazione
struct periodo{
	int booleanoFormattazione; // Variabile booleana che controlla che il periodo sia stato formattato correttamente
	int booleanoInizioNonSpecificato; // Variabile booleana settata nel caso in cui la data di inizio non sia definita (assenza di lower bound)
	struct data data1; // Data di inizio del periodo
	int booleanoFineNonSpecificata; // Variabile booleana settata nel caso in cui la data di fine non sia definita (assenza di upper bound)
	struct data data2; // Data di fine del periodo
};

// Struttura dati per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
struct tipoRichiestaRegistrazione{
	pthread_cond_t *condizioneRichiestaRegistrazione; // Puntatore alla variabile condizione per il comando di registrazione
	struct in_addr *ds_addr_pointer; // Puntatore all'indirizzo del discovery server
	uint16_t *ds_port_pointer; // Puntatore alla porta del discovery server
};

// Struttura dati per gestire la comunicazione tra i thread nel caso di richiesta di aggregazione dati
struct tipoRichiestaAggregazione{
	pthread_cond_t *condizioneRisultatoCalcolato; // Puntatore alla variabile condizione per il calcolo del risultato dell'aggregazione
	int *puntatoreRisultato; // Puntatore al risultato dell'aggregazione
	int *puntatoreBooleanoRichiestaAggregazione; // Puntatore alla variabile booleana che viene settata nel caso di richiesta di aggregazione
	struct data *puntatoreData; // Puntatore alla data della richiesta di aggregazione
	enum tipoDato *puntatoreTipo; // Puntatore al tipo dei dati della richiesta di aggregazione
};

// Struttura dati per gli input del thread di gestione del terminale
struct terminalHandlerInputArguments{
	pthread_mutex_t *puntatoreMutexRichiesta; // Puntatore alla variabile mutex per gestire la comunicazione tra i thread nel caso di nuova richiesta
	struct tipoRichiestaRegistrazione *puntatoreRichiestaRegistrazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	struct tipoRichiestaAggregazione *puntatoreRichiestaAggregazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di aggregazione dati
	int *puntatoreBooleanoRichiestaStop; // Puntatore alla variabile booleana che viene settata nel caso di richiesta di stop
	int *puntatoreNumeroDati; // Puntatore alla variabile che indica il numero di dati gestiti dal peer
	struct dato *insiemeDati; // Vettore per contenere i dati
	pthread_mutex_t *puntatoreMutexInsiemeDati; // Puntatore alla variabile mutex per gestire il vettore dei dati
	int *puntatoreNumeroRisultati; // Puntatore al numero dei risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno memorizzati dal peer
	struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri; // Vettore per contenere i risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno
	pthread_mutex_t *puntatoreMutexRisultati; // Puntatore alla variabile mutex per gestire il vettore dei risultati
};

// Struttura dati per gli input del thread di gestione del peer in rete
struct networkHandlerInputArguments{
	uint16_t porta; // Porta del peer
	pthread_mutex_t *puntatoreMutexRichiesta; // Puntatore alla variabile mutex per gestire la comunicazione tra i thread nel caso di nuova richiesta
	struct tipoRichiestaRegistrazione *puntatoreRichiestaRegistrazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	struct tipoRichiestaAggregazione *puntatoreRichiestaAggregazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di aggregazione dati
	int *puntatoreBooleanoRichiestaStop; // Puntatore alla variabile booleana che viene settata nel caso di richiesta di stop
	int *puntatoreNumeroDati; // Puntatore alla variabile che indica il numero di dati gestiti dal peer
	struct dato *insiemeDati; // Vettore per contenere i dati
	pthread_mutex_t *puntatoreMutexInsiemeDati; // Puntatore alla variabile mutex per gestire il vettore dei dati
	int *puntatoreNumeroRisultati; // Puntatore al numero dei risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno memorizzati dal peer
	struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri; // Vettore per contenere i risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno
	pthread_mutex_t *puntatoreMutexRisultati; // Puntatore alla variabile mutex per gestire il vettore dei risultati
};

/* ********************************************************************************************************************************************************
																DICHIARAZIONE FUNZIONI
******************************************************************************************************************************************************** */

int calcolaTotaleGiornalieroIndividuale(struct data, enum tipoDato, struct dato*, int); // Funzione che calcola il valore totale dei dati di un certo tipo registrati da un peer in un certo giorno
int cercaRisultatoTotaleGiornaliero(struct data, enum tipoDato, struct risultatoTotaleGiornaliero*, int); // Funzione che controlla se il risultato dell'aggregazione di tipo 'totale' di un certo giorno è già presente tra quelli memorizzati dal peer, e che, se presente, lo restituisce
int calcolaRisultatoTotaleGiornaliero(struct data, enum tipoDato, struct dato*, int, pthread_mutex_t*, struct risultatoTotaleGiornaliero*, int*, pthread_mutex_t*, struct tipoRichiestaAggregazione*, pthread_mutex_t*); // Funzione che calcola il risultato dell'aggregazione di tipo 'totale' di un certo giorno, interagendo con gli altri peer tramite il thread di gestione del peer in rete
void calcolaAggregazione(struct periodo, enum tipoAggregazione, enum tipoDato, struct dato*, int, pthread_mutex_t*, struct risultatoTotaleGiornaliero*, int*, pthread_mutex_t*, struct tipoRichiestaAggregazione*, pthread_mutex_t*); // Funzione che svolge le operazioni di aggregazione dei dati

void *terminalHandler(void*); // Funzione per la gestione del terminale
void help(); // Funzione per la gestione del comando help
struct periodo checkPeriodoAggregazione(char*); // Funzione per controllare la validità del periodo di aggregazione inserito dall'utente
int isCifra(char); // Funzione che controlla che un carattere sia una cifra
int isBisestile(int); // Funzione che controlla se un anno sia bisestile
int giorniMese(int, int); // Funzione che calcola il numero di giorni di un mese
int dataMinore(struct data, struct data); // Funzione che restituisce 1 se la data1 è precedente alla data2, -1 se la data2 è precedente alla data1, e 0 se le due date coincidono
int isDataValida(struct data); // Funzione che controlla che una data sia valida

void *networkHandler(void*); // Funzione per la gestione del peer in rete

void inizializzaDati(struct dato*, int*); // Funzione che inizializza l'insieme dei dati

/* ********************************************************************************************************************************************************
																		MAIN
******************************************************************************************************************************************************** */

int main(int argc, char *argv[]){

	// Dichiarazione e inizializzazione delle variabili
	pthread_t networkHandlerID, terminalHandlerID; // Thread IDs per i thread di gestione del terminale e di gestione del peer in rete		
	uint16_t porta = atoi(argv[1]); // Porta del peer
	pthread_cond_t condizioneRichiestaRegistrazione; // Variabile condizione per il comando di registrazione
	struct in_addr ds_addr; // Indirizzo del discovery server
	uint16_t ds_port; // Porta del discovery server
	struct tipoRichiestaRegistrazione richiestaRegistrazione = {&condizioneRichiestaRegistrazione, &ds_addr, &ds_port}; // Variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	pthread_cond_t risultatoCalcolato; // Variabile condizione per il calcolo del risultato dell'aggregazione
	int risultato; // Risultato dell'aggregazione
	int booleanoRichiestaAggregazione = 0; // Variabile booleana che viene settata nel caso di richiesta di registrazione
	struct data dataRichiestaAggregazione; // Data della richiesta di aggregazione
	enum tipoDato tipoRichiestaAggregazione; // Tipo dei dati della richiesta di aggregazione
	struct tipoRichiestaAggregazione richiestaAggregazione = {&risultatoCalcolato, &risultato, &booleanoRichiestaAggregazione, &dataRichiestaAggregazione, &tipoRichiestaAggregazione}; // Variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	int booleanoRichiestaStop = 0; // Variabile booleana che viene settata nel caso di richiesta di stop
	pthread_mutex_t mutexRichiesta; // Variabile mutex per gestire la comunicazione tra i thread nel caso di nuova richiesta
	int numeroDati = 0; // Numero dei dati memorizzati dal peer 
	struct dato insiemeDati[MAX_DATI]; // Vettore per contenere i dati
	pthread_mutex_t mutexInsiemeDati; // Variabile mutex per gestire il vettore dei dati
	int numeroRisultati = 0; // Numero dei risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno memorizzati dal peer
	struct risultatoTotaleGiornaliero risultatiTotaliGiornalieri[MAX_GIORNI]; // Vettore per contenere i risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno
	pthread_mutex_t mutexRisultati; // Variabile mutex per gestire il vettore dei risultati

	// Inizializzazione degli input dei thread
	struct terminalHandlerInputArguments tHIA = {&mutexRichiesta, &richiestaRegistrazione, &richiestaAggregazione, &booleanoRichiestaStop, &numeroDati, insiemeDati, &mutexInsiemeDati, &numeroRisultati, risultatiTotaliGiornalieri, &mutexRisultati}; // Input del thread per la gestione del terminale
	struct networkHandlerInputArguments nHIA = {porta, &mutexRichiesta, &richiestaRegistrazione, &richiestaAggregazione, &booleanoRichiestaStop, &numeroDati, insiemeDati, &mutexInsiemeDati, &numeroRisultati, risultatiTotaliGiornalieri, &mutexRisultati}; // Input del thread per la gestione del peer in rete

	// Inizializzazione delle variabili condizione
	pthread_cond_init(&condizioneRichiestaRegistrazione, NULL);
	pthread_cond_init(&risultatoCalcolato, NULL);
	// Inizializzazione delle variabili mutex
	pthread_mutex_init(&mutexRichiesta, NULL);
	pthread_mutex_init(&mutexInsiemeDati, NULL);
	pthread_mutex_init(&mutexRisultati, NULL);

	// Inizializzazione dell'insieme dei dati
	inizializzaDati(insiemeDati, &numeroDati);

	// Creazione dei thread
	pthread_create(&networkHandlerID, NULL, networkHandler, (void*)&nHIA); // Thread per la gestione del peer in rete
	pthread_create(&terminalHandlerID, NULL, terminalHandler, (void*)&tHIA); //Thread per la gestione del terminale

	// Terminazione
	pthread_exit(NULL);

}

// Funzione che inizializza l'insieme dei dati
void inizializzaDati(struct dato *insiemeDati, int *puntatoreNumeroDati){
	
	struct tm tmStruct;
	time_t istante;
	struct data upper_bound;
	struct data data;
	int i = 1;

	// upper_bound viene inizializzato con la data del giorno precedente all'esecuzione del programma peer
	time(&istante);	
	tmStruct = *localtime(&istante);
	tmStruct.tm_mday -= 1;
	istante = mktime(&tmStruct);
	tmStruct = *localtime(&istante);
	upper_bound.anno = tmStruct.tm_year + 1900;
	upper_bound.mese = tmStruct.tm_mon + 1;
	upper_bound.giorno = tmStruct.tm_mday;

	tmStruct.tm_year = ANNO_INIZIO - 1900;
	tmStruct.tm_mon = MESE_INIZIO - 1;
	tmStruct.tm_mday = GIORNO_INIZIO;
	while (1){
		data.anno = tmStruct.tm_year + 1900;
		data.mese = tmStruct.tm_mon + 1;
		data.giorno = tmStruct.tm_mday;
		if (dataMinore(data, upper_bound) == -1){
			break;
		}

		insiemeDati[*puntatoreNumeroDati].data.anno = data.anno;
		insiemeDati[*puntatoreNumeroDati].data.mese = data.mese;
		insiemeDati[*puntatoreNumeroDati].data.giorno = data.giorno;
		insiemeDati[*puntatoreNumeroDati].tipo = T;
		insiemeDati[*puntatoreNumeroDati].valore = i;
		(*puntatoreNumeroDati)++;
		insiemeDati[*puntatoreNumeroDati].data.anno = data.anno;
		insiemeDati[*puntatoreNumeroDati].data.mese = data.mese;
		insiemeDati[*puntatoreNumeroDati].data.giorno = data.giorno;
		insiemeDati[*puntatoreNumeroDati].tipo = N;
		insiemeDati[*puntatoreNumeroDati].valore = ++i;
		(*puntatoreNumeroDati)++;

		tmStruct.tm_mday += 1;
		istante = mktime(&tmStruct);
		tmStruct = *localtime(&istante);
	}

}

/* ********************************************************************************************************************************************************
														FUNZIONI PER LE OPERAZIONI DI AGGREGAZIONE
******************************************************************************************************************************************************** */

// Funzione che calcola il valore totale dei dati di un certo tipo registrati dal peer in un certo giorno
int calcolaTotaleGiornalieroIndividuale(struct data data, enum tipoDato tipo, struct dato *insiemeDati, int numeroDati){
	int i, totale = 0;
	for (i = 0; i < numeroDati; i++){
		if (tipo == insiemeDati[i].tipo && dataMinore(data, insiemeDati[i].data) == 0){
			totale += insiemeDati[i].valore;
		}
	}
	return totale;
}

// Funzione che controlla se il risultato dell'aggregazione di tipo 'totale' di un certo giorno è già presente tra quelli memorizzati dal peer, e che, se presente, lo restituisce
int cercaRisultatoTotaleGiornaliero(struct data data, enum tipoDato tipo, struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri, int numeroRisultati){
	int i, risultato = NON_TROVATO;
	for (i = 0; i < numeroRisultati; i++){
		if (tipo == risultatiTotaliGiornalieri[i].tipo && dataMinore(data, risultatiTotaliGiornalieri[i].data) == 0){
			risultato = risultatiTotaliGiornalieri[i].valore;
			break;
		}
	}
	return risultato;
}

// Funzione che calcola il risultato dell'aggregazione di tipo 'totale' di un certo giorno, interagendo con gli altri peer tramite il thread di gestione del peer in rete
int calcolaRisultatoTotaleGiornaliero(struct data data, enum tipoDato tipo, struct dato *insiemeDati, int numeroDati, pthread_mutex_t *puntatoreMutexInsiemeDati, struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri, int *puntatoreNumeroRisultati, pthread_mutex_t *puntatoreMutexRisultati, struct tipoRichiestaAggregazione *puntatoreRichiestaAggregazione, pthread_mutex_t *puntatoreMutexRichiesta){
	int risultato;

	// Prova a trovare il risultato nel vettore dei risultati già registrati dal peer
	pthread_mutex_lock(puntatoreMutexRisultati);
	risultato = cercaRisultatoTotaleGiornaliero(data, tipo, risultatiTotaliGiornalieri, *puntatoreNumeroRisultati);
	pthread_mutex_unlock(puntatoreMutexRisultati);

	if (risultato == NON_TROVATO){
		// Calcola il totale aggregando i dati posseduti dal peer
		pthread_mutex_lock(puntatoreMutexInsiemeDati);
		*(puntatoreRichiestaAggregazione->puntatoreRisultato) = calcolaTotaleGiornalieroIndividuale(data, tipo, insiemeDati, numeroDati);
		pthread_mutex_unlock(puntatoreMutexInsiemeDati);

		// Prepara la richiesta di aggregazione e segnala che ce n'è una nuova, per poi attendere che il network handler calcoli il risultato dell'aggregazione
		pthread_mutex_lock(puntatoreMutexRichiesta);
		*(puntatoreRichiestaAggregazione->puntatoreData) = data;
		*(puntatoreRichiestaAggregazione->puntatoreTipo) = tipo;
		*(puntatoreRichiestaAggregazione->puntatoreBooleanoRichiestaAggregazione) = 1;
		pthread_cond_wait(puntatoreRichiestaAggregazione->condizioneRisultatoCalcolato, puntatoreMutexRichiesta);
		risultato = *(puntatoreRichiestaAggregazione->puntatoreRisultato);
		pthread_mutex_unlock(puntatoreMutexRichiesta);

		// Inserisce il risultato nel vettore dei risultati e aumenta il numero di risultati
		pthread_mutex_lock(puntatoreMutexRisultati);
		risultatiTotaliGiornalieri[*puntatoreNumeroRisultati].valore = risultato;
		risultatiTotaliGiornalieri[*puntatoreNumeroRisultati].tipo = tipo;
		risultatiTotaliGiornalieri[*puntatoreNumeroRisultati].data = data;
		(*puntatoreNumeroRisultati)++;
		pthread_mutex_unlock(puntatoreMutexRisultati);
	}

	return risultato;
}

// Funzione che svolge le operazioni di aggregazione dei dati
void calcolaAggregazione(struct periodo periodo, enum tipoAggregazione tipoAggregazione, enum tipoDato tipoDato, struct dato *insiemeDati, int numeroDati, pthread_mutex_t *puntatoreMutexInsiemeDati, struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri, int *puntatoreNumeroRisultati, pthread_mutex_t *puntatoreMutexRisultati, struct tipoRichiestaAggregazione *puntatoreRichiestaAggregazione, pthread_mutex_t *puntatoreMutexRichiesta){
	struct data lower_bound;
	struct data upper_bound;
	time_t istante;
	struct data data;
	int risultatoTotale = 0;
	int primoRisultatoVariazione;
	int secondoRisultatoVariazione;
	struct tm tmStruct;

	if (periodo.booleanoInizioNonSpecificato == 1){ // Nel caso di inizio non specificato il lower_bound viene posto uguale alla data di inizio dell'analisi dati
		lower_bound.anno = ANNO_INIZIO;
		lower_bound.mese = MESE_INIZIO;
		lower_bound.giorno = GIORNO_INIZIO;
	} else {
		lower_bound.anno = periodo.data1.anno;
		lower_bound.mese = periodo.data1.mese;
		lower_bound.giorno = periodo.data1.giorno;
	}

	if (periodo.booleanoFineNonSpecificata == 1){ // Nel caso di fine non specificata l'upper_bound viene posto uguale alla data precedente di un giorno alla data della richiesta di aggregazione
		time(&istante);
		tmStruct = *localtime(&istante);
		tmStruct.tm_mday -= 1;
		istante = mktime(&tmStruct);
		tmStruct = *localtime(&istante);
		upper_bound.anno = tmStruct.tm_year + 1900;
		upper_bound.mese = tmStruct.tm_mon + 1;
		upper_bound.giorno = tmStruct.tm_mday;
	} else {
		upper_bound.anno = periodo.data2.anno;
		upper_bound.mese = periodo.data2.mese;
		upper_bound.giorno = periodo.data2.giorno;
	}

	if (tipoAggregazione == totale){
		tmStruct.tm_year = lower_bound.anno - 1900;
		tmStruct.tm_mon = lower_bound.mese - 1;
		tmStruct.tm_mday = lower_bound.giorno;
		while (1){
			data.anno = tmStruct.tm_year + 1900;
			data.mese = tmStruct.tm_mon + 1;
			data.giorno = tmStruct.tm_mday;
			if (dataMinore(data, upper_bound) == -1){
				break;
			}
			risultatoTotale += calcolaRisultatoTotaleGiornaliero(data, tipoDato, insiemeDati, numeroDati, puntatoreMutexInsiemeDati, risultatiTotaliGiornalieri, puntatoreNumeroRisultati, puntatoreMutexRisultati, puntatoreRichiestaAggregazione, puntatoreMutexRichiesta);
			tmStruct.tm_mday += 1;
			istante = mktime(&tmStruct);
			tmStruct = *localtime(&istante);
		}
		printf("Totale: %d\n", risultatoTotale);
	} else if (tipoAggregazione == variazione){
		primoRisultatoVariazione = calcolaRisultatoTotaleGiornaliero(lower_bound, tipoDato, insiemeDati, numeroDati, puntatoreMutexInsiemeDati, risultatiTotaliGiornalieri, puntatoreNumeroRisultati, puntatoreMutexRisultati, puntatoreRichiestaAggregazione, puntatoreMutexRichiesta);
		tmStruct.tm_year = lower_bound.anno - 1900;
		tmStruct.tm_mon = lower_bound.mese - 1;
		tmStruct.tm_mday = lower_bound.giorno;
		while (1){
			tmStruct.tm_mday++;
			istante = mktime(&tmStruct);
			tmStruct = *localtime(&istante);
			data.anno = tmStruct.tm_year + 1900;
			data.mese = tmStruct.tm_mon + 1;
			data.giorno = tmStruct.tm_mday;
			if (dataMinore(data, upper_bound) == -1){
				break;
			}
			secondoRisultatoVariazione = calcolaRisultatoTotaleGiornaliero(data, tipoDato, insiemeDati, numeroDati, puntatoreMutexInsiemeDati, risultatiTotaliGiornalieri, puntatoreNumeroRisultati, puntatoreMutexRisultati, puntatoreRichiestaAggregazione, puntatoreMutexRichiesta);
			printf("Variazione in data %d:%d:%d -> %d\n", data.giorno, data.mese, data.anno, secondoRisultatoVariazione - primoRisultatoVariazione);
			primoRisultatoVariazione = secondoRisultatoVariazione;
		}
	}
}

/* ********************************************************************************************************************************************************
																GESTIONE DEL TERMINALE
******************************************************************************************************************************************************** */

// Funzione per la gestione del terminale
void *terminalHandler(void *arg){

	// Dichiarazione e inizializzazione delle variabili
	struct terminalHandlerInputArguments *inputArgument = (struct terminalHandlerInputArguments*)arg;
	struct tipoRichiestaRegistrazione *puntatoreRichiestaRegistrazione = inputArgument->puntatoreRichiestaRegistrazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	struct tipoRichiestaAggregazione *puntatoreRichiestaAggregazione = inputArgument->puntatoreRichiestaAggregazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	pthread_mutex_t *puntatoreMutexRichiesta = inputArgument->puntatoreMutexRichiesta; // Puntatore alla variabile mutex per gestire la comunicazione tra i thread nel caso di nuova richiesta
	int *puntatoreBooleanoRichiestaStop = inputArgument->puntatoreBooleanoRichiestaStop; // Puntatore alla variabile booleana che viene settata nel caso di richiesta di stop
	int *puntatoreNumeroDati = inputArgument->puntatoreNumeroDati; // Puntatore alla variabile che indica il numero di dati gestiti dal peer
	struct dato *insiemeDati = inputArgument->insiemeDati; // Vettore per contenere i dati
	pthread_mutex_t *puntatoreMutexInsiemeDati = inputArgument->puntatoreMutexInsiemeDati; // Puntatore alla variabile mutex per gestire il vettore dei dati
	int *puntatoreNumeroRisultati = inputArgument->puntatoreNumeroRisultati; // Puntatore al numero dei risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno memorizzati dal peer
	struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri = inputArgument->risultatiTotaliGiornalieri; // Vettore per contenere i risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno
	pthread_mutex_t *puntatoreMutexRisultati = inputArgument->puntatoreMutexRisultati; // Puntatore alla variabile mutex per gestire il vettore dei risultati
	char comando[CMD_LEN]; // Variabile che contiene la stringa del comando inserito
	char *argomenti[MAX_ARGOMENTI]; // Variabile che contiene gli argomenti del comando inserito
	int numeroArgomenti; // Numero degli argomenti del comando inserito
	char delimiter[] = " "; // Delimitatore degli argomenti del comando inserito
	int registrazioneEffettuata = 0; // Variabile booleana settata quando viene effettuata la registrazione
	time_t istanteInserimento; // Istante di inserimento di un nuovo dato tramite il comando 'add'
	struct tm tmStruct; // Struttura tm per gestire gli istanti di inserimento
	char* stringaPeriodoAggregazione; // Argomento 'periodo' del comando 'get'	
	struct periodo periodoAggregazione; // Periodo sul quale effettuare l'aggregazione dei dati
	enum tipoAggregazione tipoAggregazione; // Tipo di aggregazione da effettuare sui dati
	enum tipoDato tipoDatoAggregazione; // Tipo dei dati sui quali svolgere l'aggregazione
	int numeroDati; // Numero dei dati registrati dal peer

	// Welcome message
	printf("***************************** PEER STARTED *****************************\n");
	printf("Digita un comando:\n");
	printf("1) help --> mostra i dettagli dei comandi\n");
	printf("2) start DS_addr DS_port --> richiede al DS la connessione al network\n");
	printf("3) add type quantity --> aggiunge al register della data corrente un nuovo evento\n");
	printf("4) get aggr type period --> effettua una richiesta di elaborazione\n");
	printf("5) stop --> richiede al DS la disconnessione dal network\n");

	// Prompt e gestione del task
	while(1){
		printf(">> ");
		fgets(comando, CMD_LEN, stdin);

		// Parsing del comando: gli argomenti del comando vengono inseriti nel vettore 'argomenti'
		numeroArgomenti = 0;
		argomenti[numeroArgomenti] = strtok(comando, delimiter);
		while (argomenti[numeroArgomenti] != NULL && numeroArgomenti < 5){
			argomenti[++numeroArgomenti] = strtok(NULL, delimiter);
		}

		// Parsing degli argomenti: vengono analizzati gli argomenti del comando e viene controllato che il comando sia stato inserito con il corretto formato. Il comando viene poi gestito
		if (strcmp(argomenti[0], "help\n") == 0){
			help();
		} else if (strcmp(argomenti[0], "start") == 0) {
			pthread_mutex_lock(puntatoreMutexRichiesta);
			if (numeroArgomenti == 3 && inet_pton(AF_INET, argomenti[1], puntatoreRichiestaRegistrazione->ds_addr_pointer) == 1 && sscanf(argomenti[2], "%hu", puntatoreRichiestaRegistrazione->ds_port_pointer) == 1){
				if (registrazioneEffettuata == 1){
					printf("Errore: il comando start è stato già eseguito\n");
					pthread_mutex_unlock(puntatoreMutexRichiesta);
					continue;
				}
				pthread_cond_signal(puntatoreRichiestaRegistrazione->condizioneRichiestaRegistrazione);
			} else {
				printf("Errore: controllare il formato del comando\n");
				pthread_mutex_unlock(puntatoreMutexRichiesta);
				continue;
			}
			pthread_cond_wait(puntatoreRichiestaRegistrazione->condizioneRichiestaRegistrazione, puntatoreMutexRichiesta);
			pthread_mutex_unlock(puntatoreMutexRichiesta);
			registrazioneEffettuata = 1;
			continue;
		} else if (strcmp(argomenti[0], "add") == 0 && (strcmp(argomenti[1], "T") == 0 || strcmp(argomenti[1], "N") == 0) && atoi(argomenti[2]) != 0){
			time(&istanteInserimento);
			if (localtime(&istanteInserimento)->tm_hour + 1 >= ORA_CHIUSURA){
				printf("Errore: impossibile aggiungere dati dopo le %d:00\n", ORA_CHIUSURA);
				continue;
			}
			pthread_mutex_lock(puntatoreMutexInsiemeDati);
			insiemeDati[*puntatoreNumeroDati].data.anno = localtime(&istanteInserimento)->tm_year + 1900;
			insiemeDati[*puntatoreNumeroDati].data.mese = localtime(&istanteInserimento)->tm_mon + 1;
			insiemeDati[*puntatoreNumeroDati].data.giorno = localtime(&istanteInserimento)->tm_mday;
			insiemeDati[*puntatoreNumeroDati].tipo = strcmp(argomenti[1], "T") == 0  ? T : N;
			insiemeDati[*puntatoreNumeroDati].valore = atoi(argomenti[2]);
			(*puntatoreNumeroDati)++;
			pthread_mutex_unlock(puntatoreMutexInsiemeDati);
		} else if (strcmp(argomenti[0], "get") == 0 && (strcmp(argomenti[1], "variazione") == 0 || strcmp(argomenti[1], "totale") == 0)){
			tipoAggregazione = (strcmp(argomenti[1], "variazione") == 0 ? variazione : totale);
			tipoDatoAggregazione = (argomenti[2][0] == 'T'? T : N);
			pthread_mutex_lock(puntatoreMutexInsiemeDati);
			numeroDati = *puntatoreNumeroDati;
			pthread_mutex_unlock(puntatoreMutexInsiemeDati);
			if (strcmp(argomenti[2], "T\n") == 0 || strcmp(argomenti[2], "N\n") == 0){
				if (registrazioneEffettuata == 0){
					printf("Errore: il peer non è stato ancora connesso. Prima di lanciare il comando get è necessario lanciare il comando start.\n");
					continue;
				}
				periodoAggregazione.booleanoInizioNonSpecificato = periodoAggregazione.booleanoFineNonSpecificata = 1;
				calcolaAggregazione(periodoAggregazione, tipoAggregazione, tipoDatoAggregazione, insiemeDati, numeroDati, puntatoreMutexInsiemeDati, risultatiTotaliGiornalieri, puntatoreNumeroRisultati, puntatoreMutexRisultati, puntatoreRichiestaAggregazione, puntatoreMutexRichiesta);
			} else if (strcmp(argomenti[2], "T") == 0 || strcmp(argomenti[2], "N") == 0){
				stringaPeriodoAggregazione = argomenti[3];
				if ((periodoAggregazione = checkPeriodoAggregazione(stringaPeriodoAggregazione)).booleanoFormattazione == 1){
					if (registrazioneEffettuata == 0){
						printf("Errore: il peer non è stato ancora connesso. Prima di lanciare il comando get è necessario lanciare il comando start.\n");
						continue;
					}
					calcolaAggregazione(periodoAggregazione, tipoAggregazione, tipoDatoAggregazione, insiemeDati, numeroDati, puntatoreMutexInsiemeDati, risultatiTotaliGiornalieri, puntatoreNumeroRisultati, puntatoreMutexRisultati, puntatoreRichiestaAggregazione, puntatoreMutexRichiesta);
				} else {
					time(&istanteInserimento);
					tmStruct = *localtime(&istanteInserimento);
					tmStruct.tm_mday--;
					istanteInserimento = mktime(&tmStruct);
					tmStruct = *localtime(&istanteInserimento);
					printf("Errore: il periodo di aggregazione indicato non è valido. (Gli estremi del periodo di aggregazione devono andare dal %d:%d:%d al %d:%d:%d).\n", GIORNO_INIZIO, MESE_INIZIO, ANNO_INIZIO, tmStruct.tm_mday, tmStruct.tm_mon + 1, tmStruct.tm_year + 1900);
				}
			} else {
				printf("Errore: controllare il formato del comando\n");
			}
		} else if (strcmp(argomenti[0], "stop\n") == 0) {
			if (registrazioneEffettuata == 0){
				printf("Errore: il peer non è stato ancora connesso. Prima di lanciare il comando stop è necessario lanciare il comando start.\n");
				continue;
			}
			pthread_mutex_lock(puntatoreMutexRichiesta);
			*puntatoreBooleanoRichiestaStop = 1;
			pthread_mutex_unlock(puntatoreMutexRichiesta);
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
	printf("1) help:\n\tMostra il significato dei comandi e ciò che fanno.\n");
	printf("2) start DS_addr DS_port:\n\tRichiede al DS, in ascolto all’indirizzo DS_addr:DS_port,\n\tla connessione al network. Il DS registra il peer\n\te gli invia la lista di neighbor. Se il peer è il primo a connettersi,\n\tla lista sarà vuota. Il DS avrà cura di integrare il peer nel network\n\ta mano a mano che altri peer invocheranno la start.\n");
	printf("3) add type quantity:\n\tAggiunge al register della data corrente l’evento type con quantità quantity.\n\tQuesto comando provoca la memorizzazione di una nuova entry nel peer.\n");
	printf("4) get aggr type period:\n\tEffettua una richiesta di elaborazione per ottenere il dato aggregato aggr sui\n\tdati relativi a un lasso temporale d’interesse period sulle entry di tipo type,\n\tconsiderando tali dati su scala giornaliera. Le aggregazioni aggr calcolabili\n\tsono il 'totale' e la 'variazione'. Il parametro period è espresso come\n\t‘dd1:mm1:yyyy1-dd2:mm2:yyyy2’, dove con 1 e 2 si indicano, rispettivamente,\n\tla data più remota e più recente del lasso temporale d’interesse. Una delle\n\tdue date può valere ‘*’. Se a valere '*' è la prima, non esiste una data\n\tche fa da lower bound. Viceversa, non c’è l’upper bound e il calcolo va\n\tfatto fino alla data più recente (con register chiusi). Il parametro period\n\tpuò mancare, in quel caso il calcolo si fa su tutti i dati.\n");
	printf("5) stop:\n\tIl peer richiede una disconnessione dal network. Il comando stop\n\tprovoca l’invio ai neighbor di tutte le entry registrate dall’avvio. Il peer\n\tcontatta poi il DS per comunicare la volontà di disconnettersi. Il DS aggiorna\n\ti registri di prossimità rimuovendo il peer. Se la rimozione del peer\n\tcausa l’isolamento di una un’intera parte del network, il DS modifica\n\ti registri di prossimità di alcuni peer, affinché questo non avvenga.\n");
	printf("----------\n");
}

// Funzione per controllare la validità del periodo di aggregazione inserito dall'utente
struct periodo checkPeriodoAggregazione(char* stringaPeriodo){
	struct periodo risultato = {0, 0, {0, 0, 0}, 0, {0, 0, 0}};

	// Periodo di tipo "*-20:07:2021\n"
	if (strlen(stringaPeriodo) == strlen("*-20:07:2021\n") && stringaPeriodo[0] == '*' && stringaPeriodo[1] == '-' && isCifra(stringaPeriodo[2]) &&  isCifra(stringaPeriodo[3]) && stringaPeriodo[4] == ':' && isCifra(stringaPeriodo[5]) &&  isCifra(stringaPeriodo[6]) && stringaPeriodo[7] == ':' && isCifra(stringaPeriodo[8]) && isCifra(stringaPeriodo[9]) && isCifra(stringaPeriodo[10]) && isCifra(stringaPeriodo[11]) && stringaPeriodo[12] == '\n'){
		risultato.booleanoInizioNonSpecificato = 1;
		risultato.data2.giorno = (stringaPeriodo[2] - '0') * 10 + stringaPeriodo[3] - '0';
		risultato.data2.mese = (stringaPeriodo[5] - '0') * 10 + stringaPeriodo[6] - '0';
		risultato.data2.anno = (stringaPeriodo[8] - '0') * 1000 + (stringaPeriodo[9] - '0') * 100 + (stringaPeriodo[10] - '0') * 10 + stringaPeriodo[11] - '0';
		if (isDataValida(risultato.data2)){
			risultato.booleanoFormattazione = 1;
		}
	}

	// Periodo di tipo "20:07:2021-*\n"
	if (strlen(stringaPeriodo) == strlen("20:07:2021-*\n") && isCifra(stringaPeriodo[0]) && isCifra(stringaPeriodo[1]) && stringaPeriodo[2] == ':' && isCifra(stringaPeriodo[3]) && isCifra(stringaPeriodo[4]) && stringaPeriodo[5] == ':' && isCifra(stringaPeriodo[6]) && isCifra(stringaPeriodo[7]) && isCifra(stringaPeriodo[8]) && isCifra(stringaPeriodo[9]) && stringaPeriodo[10] == '-' && stringaPeriodo[11] == '*' && stringaPeriodo[12] == '\n'){
		risultato.booleanoFineNonSpecificata = 1;
		risultato.data1.giorno = (stringaPeriodo[0] - '0') * 10 + stringaPeriodo[1] - '0';
		risultato.data1.mese = (stringaPeriodo[3] - '0') * 10 + stringaPeriodo[4] - '0';
		risultato.data1.anno = (stringaPeriodo[6] - '0') * 1000 + (stringaPeriodo[7] - '0') * 100 + (stringaPeriodo[8] - '0') * 10 + stringaPeriodo[9] - '0';
		if (isDataValida(risultato.data1)){
			risultato.booleanoFormattazione = 1;
		}
	}

	// Periodo di tipo "20:07:2021-26:07:2021\n"
	if (strlen(stringaPeriodo) == strlen("20:07:2021-26:07:2021\n") && isCifra(stringaPeriodo[0]) && isCifra(stringaPeriodo[1]) && stringaPeriodo[2] == ':' && isCifra(stringaPeriodo[3]) && isCifra(stringaPeriodo[4]) && stringaPeriodo[5] == ':' && isCifra(stringaPeriodo[6]) && isCifra(stringaPeriodo[7]) && isCifra(stringaPeriodo[8]) && isCifra(stringaPeriodo[9]) && stringaPeriodo[10] == '-' && isCifra(stringaPeriodo[11]) && isCifra(stringaPeriodo[12]) && stringaPeriodo[13] == ':' && isCifra(stringaPeriodo[14]) &&  isCifra(stringaPeriodo[15]) && stringaPeriodo[16] == ':' && isCifra(stringaPeriodo[17]) && isCifra(stringaPeriodo[18]) && isCifra(stringaPeriodo[19]) && isCifra(stringaPeriodo[20]) && stringaPeriodo[21] == '\n'){
		risultato.data1.giorno = (stringaPeriodo[0] - '0') * 10 + stringaPeriodo[1] - '0';
		risultato.data1.mese = (stringaPeriodo[3] - '0') * 10 + stringaPeriodo[4] - '0';
		risultato.data1.anno = (stringaPeriodo[6] - '0') * 1000 + (stringaPeriodo[7] - '0') * 100 + (stringaPeriodo[8] - '0') * 10 + stringaPeriodo[9] - '0';
		risultato.data2.giorno = (stringaPeriodo[11] - '0') * 10 + stringaPeriodo[12] - '0';
		risultato.data2.mese = (stringaPeriodo[14] - '0') * 10 + stringaPeriodo[15] - '0';
		risultato.data2.anno = (stringaPeriodo[17] - '0') * 1000 + (stringaPeriodo[18] - '0') * 100 + (stringaPeriodo[19] - '0') * 10 + stringaPeriodo[20] - '0';
		if (isDataValida(risultato.data1) && isDataValida(risultato.data2) && dataMinore(risultato.data1, risultato.data2) >= 0){
			risultato.booleanoFormattazione = 1;
		}
	}

	return risultato;
}

// Funzione che controlla che un carattere sia una cifra
int isCifra(char carattere){
	if (carattere < '0' || carattere > '9'){
		return 0;
	}
	return 1;
}

// Funzione che controlla se un anno sia bisestile
int isBisestile(int anno){
	if (anno % 400 == 0 || (anno % 4 == 0 && anno % 100 != 0)){
		return 1;
	}
	return 0;
}

// Funzione che calcola il numero di giorni di un mese
int giorniMese(int anno, int mese){
	switch(mese){
		case 2:
			if(isBisestile(anno) == 1){
				return 29;
			}
			return 28;
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		default:
			return 31;
	}
}

// Funzione che restituisce 1 se data1 è precedente a data2, -1 se data2 è precedente a data1, e 0 se le due date coincidono
int dataMinore(struct data data1, struct data data2){
	if (data1.anno < data2.anno){
		return 1;
	} else if (data1.anno == data2.anno){
		if (data1.mese < data2.mese){
			return 1;
		} else if (data1.mese == data2.mese){
			if (data1.giorno < data2.giorno){
				return 1;
			} else if (data1.giorno == data2.giorno){
				return 0;
			}
		}
	}
	return -1;
}

// Funzione che controlla che una data sia valida
int isDataValida(struct data data){
	time_t istante;
	int annoPresente, mesePresente, giornoPresente;
	struct data dataPresente;
	struct data dataInizio = {ANNO_INIZIO, MESE_INIZIO, GIORNO_INIZIO};

	time(&istante);
	annoPresente = localtime(&istante)->tm_year + 1900;
	mesePresente = localtime(&istante)->tm_mon + 1;
	giornoPresente = localtime(&istante)->tm_mday;
	dataPresente.anno = annoPresente;
	dataPresente.mese = mesePresente;
	dataPresente.giorno = giornoPresente;

	if (data.mese > 0 && data.mese < 13 && data.giorno > 0 && data.giorno <= giorniMese(data.anno, data.mese) && dataMinore(dataInizio, data) >= 0 && dataMinore(data, dataPresente) == 1){
		return 1;
	}
	return 0;
}

/* ********************************************************************************************************************************************************
															GESTIONE DEL PEER IN RETE
******************************************************************************************************************************************************** */

// Funzione per la gestione del peer in rete
void *networkHandler(void *arg){

	// Dichiarazione e inizializzazione delle variabili
	struct networkHandlerInputArguments *inputArgument = (struct networkHandlerInputArguments*)arg;
	uint16_t porta = inputArgument->porta; // Porta del peer
	pthread_mutex_t *puntatoreMutexRichiesta = inputArgument->puntatoreMutexRichiesta; // Puntatore alla variabile mutex per gestire la comunicazione tra i thread nel caso di nuova richiesta
	struct tipoRichiestaRegistrazione *puntatoreRichiestaRegistrazione = inputArgument->puntatoreRichiestaRegistrazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	struct tipoRichiestaAggregazione *puntatoreRichiestaAggregazione = inputArgument->puntatoreRichiestaAggregazione; // Puntatore alla variabile per gestire la comunicazione tra i thread nel caso di richiesta di registrazione
	int *puntatoreBooleanoRichiestaStop = inputArgument->puntatoreBooleanoRichiestaStop; // Puntatore alla variabile booleana che viene settata nel caso di richiesta di stop
	int *puntatoreNumeroDati = inputArgument->puntatoreNumeroDati; // Puntatore alla variabile che indica il numero di dati gestiti dal peer
	struct dato *insiemeDati = inputArgument->insiemeDati; // Vettore per contenere i dati
	pthread_mutex_t *puntatoreMutexInsiemeDati = inputArgument->puntatoreMutexInsiemeDati; // Puntatore alla variabile mutex per gestire il vettore dei dati
	int *puntatoreNumeroRisultati = inputArgument->puntatoreNumeroRisultati; // Puntatore al numero dei risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno memorizzati dal peer
	struct risultatoTotaleGiornaliero *risultatiTotaliGiornalieri = inputArgument->risultatiTotaliGiornalieri; // Vettore per contenere i risultati delle aggregazioni di tipo 'totale' sui dati di un certo giorno
	pthread_mutex_t *puntatoreMutexRisultati = inputArgument->puntatoreMutexRisultati; // Puntatore alla variabile mutex per gestire il vettore dei risultati
	int sd; // Identificatore del socket del peer
	int ret; // Variabile per gestire il return delle varie funzioni
	struct sockaddr_in my_addr; // Indirizzo del peer
	struct sockaddr_in ds_addr; // Indirizzo del discovery server
	struct sockaddr_in connecting_addr; // Indirizzo del processo che si connette per inviare un messaggio
	struct sockaddr_in peer_addr; // Indirizzo dei peer da contattare nel caso di richieste di aggregazione
	int addr_len = sizeof(struct sockaddr_in); // Lunghezza degli indirizzi del peer e del discovery server
	char buffer[BUF_LEN]; // Buffer per l'invio e la ricezione di messaggi
	char trashString[MSGTYPE_LEN + 1]; // Stringa utilizzata per effettuare il parsing dei messaggi ricevuti con la funzione sscanf
	int vicinoSinistro; // Vicino sinistro (contiene -1 se questo non è presente)
	int vicinoDestro; // Vicino destro (contiene -1 se questo non è presemte)
	uint16_t vicinoSinistro_sin_port; // sin_port del vicino sinistro
	uint16_t vicinoDestro_sin_port; // sin_port del vicino destro
	struct data dataMessaggio; // Parametro data dei messaggi
	enum tipoDato tipoMessaggio; // Parametro tipo dei messaggi
	int valoreMessaggio; // Valore dei messaggi
	uint16_t portaMessaggio1, portaMessaggio2; // Parametri porta dei messaggi
	int REPDATAAttesi; // Numero di messaggi 'REPDATA' attesi dal peer
	int booleanoFlood; // Variabile booleana che viene settata nel caso in cui i vicini del peer non abbiano il dato richiesto dall'operazione di aggregazione
	int ENDLISTAttesi; // Numero di messaggi 'ENDLIST' attesi dal peer
	uint16_t porteREQENTR[MAX_PEER]; // Vettore dell porte alle quali richiedere le entries per le operazioni di aggregazione
	int numeroPorteREQENTR; // Numero delle porte alle quali richiedere le entries per le operazioni di aggregazione
	int REPENTRAttesi; // Numero di messaggi 'REPENTR' attesi dal peer
	int i; // Variabile contatore
	struct data lower_bound; // Lower bound per l'invio delle entries in caso di stop del peer
	struct data upper_bound; // Upper bound per l'invio delle entries in caso di stop del peer
	struct tm tmStruct; // Struttura tm per gestire le date
	time_t istante; // Variabile time_t di appoggio per gestire le date

	/* *************************
		FASE DI REGISTRAZIONE
	************************* */

	// Il thread attende la richiesta di registrazione
	pthread_mutex_lock(puntatoreMutexRichiesta);
	pthread_cond_wait(puntatoreRichiestaRegistrazione->condizioneRichiestaRegistrazione, puntatoreMutexRichiesta);

	// Creazione e inizializzazione del socket del peer
	sd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
	memset(&my_addr, 0, addr_len);
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(porta);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	// Bind del socket del peer
	ret = bind(sd, (struct sockaddr*)&my_addr, addr_len);
	if (ret < 0){
		printf("\n... ERRORE: Bind non riuscito\n");
		exit(0);
	}
	printf("... Bind effettuato con successo\n");

	// Inizializzazione dell'indirizzo dei peer a cui spedire messaggi ('sin_port' verrà inizializzato in seguito, a seconda del vicino)
	memset(&peer_addr, 0, addr_len);
	peer_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

	// Creazione e inizializzazione dell'indirizzo del discovery server
	memset(&ds_addr, 0, addr_len);
	ds_addr.sin_family = AF_INET;
	ds_addr.sin_port = htons(*(puntatoreRichiestaRegistrazione->ds_port_pointer));
	ds_addr.sin_addr = *(puntatoreRichiestaRegistrazione->ds_addr_pointer);

	// Invio del messaggio di boot
	strcpy(buffer, "BOOTREQ");
	buffer[strlen("BOOTREQ")] = '\0';
	do {	
		ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&ds_addr, addr_len);
		if (ret < 0){
			sleep(POLLING_TIME);
		}
	} while (ret < 0);
	printf("... Richiesta di boot inviata con successo\n");

	// Ricezione del messaggio del discovery server contenente i neighbour
	do {	
		ret = recvfrom(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&ds_addr, (socklen_t*)&addr_len);
		if (ret < 0){
			sleep(POLLING_TIME);
		}
	} while (ret < 0);

	// Terminazione della fase di registrazione con salvataggio delle informazioni sui vicini
	sscanf(buffer, "%d %d", &vicinoSinistro, &vicinoDestro);
	if (vicinoSinistro != VICINO_ASSENTE){
		vicinoSinistro_sin_port = htons(vicinoSinistro);
	}
	if (vicinoDestro != VICINO_ASSENTE){
		vicinoDestro_sin_port = htons(vicinoDestro);
	}
	printf("... Registrazione effettuata con successo\n");

	pthread_cond_signal(puntatoreRichiestaRegistrazione->condizioneRichiestaRegistrazione);
	pthread_mutex_unlock(puntatoreMutexRichiesta);

	/* *************************
		FASE DI ATTIVITÀ
	************************* */

	while (1){

		// -------------------------------------------
		// Gestione delle richieste fatte da terminale
		// -------------------------------------------

		pthread_mutex_lock(puntatoreMutexRichiesta);

		// Gestione del comando di stop
		if (*puntatoreBooleanoRichiestaStop == 1){
			// Invio delle entries ad uno dei due vicini
			if (vicinoDestro != VICINO_ASSENTE){
				peer_addr.sin_port = vicinoDestro_sin_port;
			} else if (vicinoSinistro != VICINO_ASSENTE){
				peer_addr.sin_port = vicinoSinistro_sin_port;
			}
			if (vicinoDestro != VICINO_ASSENTE || vicinoSinistro != VICINO_ASSENTE){
				lower_bound.anno = ANNO_INIZIO;
				lower_bound.mese = MESE_INIZIO;
				lower_bound.giorno = GIORNO_INIZIO;
				time(&istante);
				tmStruct = *localtime(&istante);
				upper_bound.anno = tmStruct.tm_year + 1900;
				upper_bound.mese = tmStruct.tm_mon + 1;
				upper_bound.giorno = tmStruct.tm_mday;
				tmStruct.tm_year = lower_bound.anno - 1900;
				tmStruct.tm_mon = lower_bound.mese - 1;
				tmStruct.tm_mday = lower_bound.giorno;
				while (1){
					dataMessaggio.anno = tmStruct.tm_year + 1900;
					dataMessaggio.mese = tmStruct.tm_mon + 1;
					dataMessaggio.giorno = tmStruct.tm_mday;
					if (dataMinore(dataMessaggio, upper_bound) == -1){
						break;
					}
					tipoMessaggio = T;
					valoreMessaggio = calcolaTotaleGiornalieroIndividuale(dataMessaggio, tipoMessaggio, insiemeDati, *puntatoreNumeroDati);
					sprintf(buffer, "ADDENTR %d %d %d %d %d", dataMessaggio.anno, dataMessaggio.mese, dataMessaggio.giorno, tipoMessaggio, valoreMessaggio);
					do {
						ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
						if (ret < 0){
							sleep(POLLING_TIME);
						}
					} while (ret < 0);
					tipoMessaggio = N;
					valoreMessaggio = calcolaTotaleGiornalieroIndividuale(dataMessaggio, tipoMessaggio, insiemeDati, *puntatoreNumeroDati);
					sprintf(buffer, "ADDENTR %d %d %d %d %d", dataMessaggio.anno, dataMessaggio.mese, dataMessaggio.giorno, tipoMessaggio, valoreMessaggio);
					do {
						ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
						if (ret < 0){
							sleep(POLLING_TIME);
						}
					} while (ret < 0);
					tmStruct.tm_mday++;
					istante = mktime(&tmStruct);
					tmStruct = *localtime(&istante);
				}
			}

			// Invio del messaggio di stop al ds
			sprintf(buffer, "STOPCON");
			do {
				ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&ds_addr, addr_len);
				if (ret < 0){
					sleep(POLLING_TIME);
				}
			} while (ret < 0);
			printf("... Stop del peer effettuato con successo\n");
			pthread_mutex_unlock(puntatoreMutexRichiesta);
			close(sd);
			pthread_exit(NULL);
		}

		//Gestione della richiesta di aggregazione
		if (*(puntatoreRichiestaAggregazione->puntatoreBooleanoRichiestaAggregazione) == 1){
			*(puntatoreRichiestaAggregazione->puntatoreBooleanoRichiestaAggregazione) = 0;
			REPDATAAttesi = 2;
			booleanoFlood = 1;
			sprintf(buffer, "REQDATA %d %d %d %d", (*(puntatoreRichiestaAggregazione->puntatoreData)).anno, (*(puntatoreRichiestaAggregazione->puntatoreData)).mese, (*(puntatoreRichiestaAggregazione->puntatoreData)).giorno, *(puntatoreRichiestaAggregazione->puntatoreTipo));
			if (vicinoDestro == VICINO_ASSENTE){
				REPDATAAttesi--;
			} else {
				peer_addr.sin_port = vicinoDestro_sin_port;
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}
			if (vicinoSinistro == VICINO_ASSENTE){
				REPDATAAttesi--;
			} else {
				peer_addr.sin_port = vicinoSinistro_sin_port;
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}
			if (REPDATAAttesi == 0){
				booleanoFlood = 0;
				pthread_cond_signal(puntatoreRichiestaAggregazione->condizioneRisultatoCalcolato);
			}
		}

		pthread_mutex_unlock(puntatoreMutexRichiesta);

		// ----------------------------------------
		// Gestione della ricezione di un messaggio
		// ----------------------------------------

		ret = recvfrom(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, (socklen_t*)&addr_len);
		if (ret < 0){
			continue;
		}

		// Gestione del messaggio di server down
		if (strncmp(buffer, "SERDOWN", MSGTYPE_LEN) == 0){
			printf("\nTERMINAZIONE DEL PROCESSO DOVUTO A DISCONNESSIONE DEL SERVER.\n");
			close(sd);
			exit(0);
		}

		// Gestione del messaggio di add entries
		if (strncmp(buffer, "ADDENTR", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %d %d %d %d %d", trashString, &dataMessaggio.anno, &dataMessaggio.mese, &dataMessaggio.giorno, (int*)&tipoMessaggio, &valoreMessaggio);
			pthread_mutex_lock(puntatoreMutexInsiemeDati);
			insiemeDati[*puntatoreNumeroDati].data.anno = dataMessaggio.anno;
			insiemeDati[*puntatoreNumeroDati].data.mese = dataMessaggio.mese;
			insiemeDati[*puntatoreNumeroDati].data.giorno = dataMessaggio.giorno;
			insiemeDati[*puntatoreNumeroDati].tipo = tipoMessaggio;
			insiemeDati[*puntatoreNumeroDati].valore = valoreMessaggio;
			(*puntatoreNumeroDati)++;
			pthread_mutex_unlock(puntatoreMutexInsiemeDati);
			continue;
		}

		// Gestione del messaggio di update dei vicini
		if (strncmp(buffer, "UPDTNGB", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %d %d", trashString, &vicinoSinistro, &vicinoDestro);
			if (vicinoSinistro != VICINO_ASSENTE){
				vicinoSinistro_sin_port = htons(vicinoSinistro);
			}
			if (vicinoDestro != VICINO_ASSENTE){
				vicinoDestro_sin_port = htons(vicinoDestro);
			}
			continue;
		}

		// Gestione del messaggio 'REPDATA'
		if (strncmp(buffer, "REPDATA", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %d", trashString, &valoreMessaggio);
			REPDATAAttesi--;

			// Se REPDATA contiene il risultato allora aggiorna la risorsa condivisa con il thread di gestione del terminale contenente il risultato dell'operazione di aggregazione, e resetta il booleano che indica che è necessaria una flood
			if (valoreMessaggio != NON_TROVATO){
				booleanoFlood = 0;
				pthread_mutex_lock(puntatoreMutexRichiesta);
				*(puntatoreRichiestaAggregazione->puntatoreRisultato) = valoreMessaggio;
				pthread_mutex_unlock(puntatoreMutexRichiesta);
			}

			// Quando arriva l'ultima REPDATA, se non è richiesto il flood, segnala al thread di gestione del terminale che il risultato è stato calcolato
			if (REPDATAAttesi == 0 && booleanoFlood == 0){
				pthread_mutex_lock(puntatoreMutexRichiesta);
				pthread_cond_signal(puntatoreRichiestaAggregazione->condizioneRisultatoCalcolato);
				pthread_mutex_unlock(puntatoreMutexRichiesta);
			}

			// Quando arriva l'ultima REPDATA, se necessario, viene effettuato il flood
			if (REPDATAAttesi == 0 && booleanoFlood == 1){
				ENDLISTAttesi = 2;
				numeroPorteREQENTR = 0;
				pthread_mutex_lock(puntatoreMutexRichiesta);
				sprintf(buffer, "FLDFENT %d %d %d %d %d", porta, (*(puntatoreRichiestaAggregazione->puntatoreData)).anno, (*(puntatoreRichiestaAggregazione->puntatoreData)).mese, (*(puntatoreRichiestaAggregazione->puntatoreData)).giorno, *(puntatoreRichiestaAggregazione->puntatoreTipo));
				pthread_mutex_unlock(puntatoreMutexRichiesta);
				if (vicinoDestro == VICINO_ASSENTE){
					ENDLISTAttesi--;
				} else {
					peer_addr.sin_port = vicinoDestro_sin_port;
					do {
						ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
						if (ret < 0){
							sleep(POLLING_TIME);
						}
					} while (ret < 0);
				}
				if (vicinoSinistro == VICINO_ASSENTE){
					ENDLISTAttesi--;
				} else {
					peer_addr.sin_port = vicinoSinistro_sin_port;
					do {
						ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
						if (ret < 0){
							sleep(POLLING_TIME);
						}
					} while (ret < 0);
				}
				if (ENDLISTAttesi == 0){
					pthread_mutex_lock(puntatoreMutexRichiesta);
					pthread_cond_signal(puntatoreRichiestaAggregazione->condizioneRisultatoCalcolato);
					pthread_mutex_unlock(puntatoreMutexRichiesta);
				}
			}

			continue;
		}

		// Gestione del messaggio di richiesta di risultato
		if (strncmp(buffer, "REQDATA", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %d %d %d %d", trashString, &dataMessaggio.anno, &dataMessaggio.mese, &dataMessaggio.giorno, (int*)&tipoMessaggio);
			pthread_mutex_lock(puntatoreMutexRisultati);
			valoreMessaggio = cercaRisultatoTotaleGiornaliero(dataMessaggio, tipoMessaggio, risultatiTotaliGiornalieri, *puntatoreNumeroRisultati);
			pthread_mutex_unlock(puntatoreMutexRisultati);
			sprintf(buffer, "REPDATA %d", valoreMessaggio);
			do {
				ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, addr_len);
				if (ret < 0){
					sleep(POLLING_TIME);
				}
			} while (ret < 0);
			continue;
		}

		// Gestione del messaggio di flood for entries
		if (strncmp(buffer, "FLDFENT", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %hu %d %d %d %d", trashString, &portaMessaggio1, &dataMessaggio.anno, &dataMessaggio.mese, &dataMessaggio.giorno, (int*)&tipoMessaggio);
			
			// Se il peer possiede delle entries relative alla data oggetto allora invia il messaggio di ENTRFND all'indietro
			pthread_mutex_lock(puntatoreMutexInsiemeDati);
			if (calcolaTotaleGiornalieroIndividuale(dataMessaggio, tipoMessaggio, insiemeDati, *puntatoreNumeroDati) != 0){
				sprintf(buffer, "ENTRFND %d %d", portaMessaggio1, porta);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}
			pthread_mutex_unlock(puntatoreMutexInsiemeDati);

			if (vicinoSinistro == VICINO_ASSENTE || vicinoDestro == VICINO_ASSENTE){ // Se il peer è l'ultimo della lista viene inviato il messaggio di 'ENDLIST'
				sprintf(buffer, "ENDLIST %d", portaMessaggio1);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			} else { // Altrimenti viene inoltrato il 'FLDFENT'
				if (portaMessaggio1 > porta){
					peer_addr.sin_port = vicinoSinistro_sin_port;
				} else {
					peer_addr.sin_port = vicinoDestro_sin_port;
				}
				sprintf(buffer, "FLDFENT %d %d %d %d %d", portaMessaggio1, dataMessaggio.anno, dataMessaggio.mese, dataMessaggio.giorno, tipoMessaggio);
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}

			continue;
		}

		// Gestione del messaggio di ENTRFND
		if (strncmp(buffer, "ENTRFND", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %hu %hu", trashString, &portaMessaggio1, &portaMessaggio2);

			if (portaMessaggio1 == porta){ // Se il messaggio è giunto al destinatario allora memorizza il peer tra quelli a cui inviare una 'REQENTR'
				porteREQENTR[numeroPorteREQENTR] = portaMessaggio2;
				numeroPorteREQENTR++;
			} else { // Altrimenti il messaggio viene inoltrato
				if (portaMessaggio1 > porta){
					peer_addr.sin_port = vicinoDestro_sin_port;
				} else {
					peer_addr.sin_port = vicinoSinistro_sin_port;
				}
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			}

			continue;
		}

		// Gestione del messaggio di ENDLIST
		if (strncmp(buffer, "ENDLIST", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %hu", trashString, &portaMessaggio1);

			if (portaMessaggio1 != porta){ // Se il messaggio non è giunto al destinatario allora viene inoltrato
				if (portaMessaggio1 > porta){
					peer_addr.sin_port = vicinoDestro_sin_port;
				} else {
					peer_addr.sin_port = vicinoSinistro_sin_port;
				}
				do {
					ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
					if (ret < 0){
						sleep(POLLING_TIME);
					}
				} while (ret < 0);
			} else { // Altrimenti il messaggio viene gestito
				ENDLISTAttesi--;
				if (ENDLISTAttesi == 0){
					REPENTRAttesi = numeroPorteREQENTR;
					for (i = 0; i < numeroPorteREQENTR; i++){
						peer_addr.sin_port = htons(porteREQENTR[i]);
						pthread_mutex_lock(puntatoreMutexRichiesta);
						sprintf(buffer, "REQENTR %d %d %d %d", (*(puntatoreRichiestaAggregazione->puntatoreData)).anno, (*(puntatoreRichiestaAggregazione->puntatoreData)).mese, (*(puntatoreRichiestaAggregazione->puntatoreData)).giorno, (*puntatoreRichiestaAggregazione->puntatoreTipo));
						pthread_mutex_unlock(puntatoreMutexRichiesta);
						do {
							ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&peer_addr, addr_len);
							if (ret < 0){
								sleep(POLLING_TIME);
							}
						} while (ret < 0);
					}
				}
			}

			continue;
		}

		// Gestione del messaggio REQENTR
		if (strncmp(buffer, "REQENTR", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %d %d %d %d", trashString, &dataMessaggio.anno, &dataMessaggio.mese, &dataMessaggio.giorno, (int*)&tipoMessaggio);

			pthread_mutex_lock(puntatoreMutexInsiemeDati);
			sprintf(buffer, "REPENTR %d", calcolaTotaleGiornalieroIndividuale(dataMessaggio, tipoMessaggio, insiemeDati, *puntatoreNumeroDati));
			pthread_mutex_unlock(puntatoreMutexInsiemeDati);
			do {
				ret = sendto(sd, buffer, BUF_LEN, 0, (struct sockaddr*)&connecting_addr, addr_len);
				if (ret < 0){
					sleep(POLLING_TIME);
				}
			} while (ret < 0);

			continue;
		}

		// Gestione del messaggio REPENTR
		if (strncmp(buffer, "REPENTR", MSGTYPE_LEN) == 0){
			sscanf(buffer, "%s %d", trashString, &valoreMessaggio);
			pthread_mutex_lock(puntatoreMutexRichiesta);
			*(puntatoreRichiestaAggregazione->puntatoreRisultato) += valoreMessaggio;
			pthread_mutex_unlock(puntatoreMutexRichiesta);
			REPENTRAttesi--;
			if (REPENTRAttesi == 0){
				pthread_mutex_lock(puntatoreMutexRichiesta);
				pthread_cond_signal(puntatoreRichiestaAggregazione->condizioneRisultatoCalcolato);
				pthread_mutex_unlock(puntatoreMutexRichiesta);
			}
		}

	}

}