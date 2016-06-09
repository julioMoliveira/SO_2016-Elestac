/*
 * nucleo.c
 *
 */

#include "nucleo.h"

int main(int argc, char **argv) {

	int exitCode = EXIT_FAILURE; //DEFAULT failure
	pthread_t serverThread;
	char *configurationFile = NULL;

	//Creo el archivo de Log
		logNucleo = log_create("logNucleo", "TP", 0, LOG_LEVEL_TRACE);
	//Creo la lista de CPUs
		listaCPU = list_create();
	//Creo Lista Procesos
		listaProcesos = list_create();
	//Creo la Cola de Listos
		colaListos = queue_create();
	//Creo la Cola de Bloqueados
		colaBloqueados = queue_create();
	//Creo cola de Procesos a Finalizar (por Finalizar PID).
		colaFinalizar = queue_create();
	//Creo archivo de configuracion

	assert(("ERROR - NOT arguments passed", argc > 1)); // Verifies if was passed at least 1 parameter, if DONT FAILS TODO => Agregar logs con librerias

	int i;
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0) {
			configurationFile = argv[i + 1];
			printf("Configuration File: '%s'\n", configurationFile);
		}
	}

	assert(("ERROR - NOT configuration file was passed as argument", configurationFile != NULL)); //Verifies if was passed the configuration file as parameter, if DONT FAILS TODO => Agregar logs con librerias

	getConfiguration(configurationFile);

	//Create thread for server start
	pthread_create(&serverThread, NULL, (void*) startServer, NULL);
	pthread_join(serverThread, NULL);

	return exitCode;
}

void startServer(){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure
	t_serverData serverData;

	exitCode = openServerConnection(configuration.puerto_cpu, &serverData.socketServer);
	printf("socketServer: %d\n",serverData.socketServer);

	//If exitCode == 0 the server connection is opened and listening
	if (exitCode == 0) {
		puts ("the server is opened");

		exitCode = listen(serverData.socketServer, SOMAXCONN);

		while (1){
			newClients((void*) &serverData);
		}
	}

}

void newClients (void *parameter){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure

	t_serverData *serverData = (t_serverData*) parameter;
	//TODO disparar un thread para acceptar cada cliente nuevo (debido a que el accept es bloqueante) y para hacer el handshake

	exitCode = acceptClientConnection(&serverData->socketServer, &serverData->socketClient);

	if (exitCode == EXIT_FAILURE){
		printf("There was detected an attempt of wrong connection\n");//TODO => Agregar logs con librerias
	}else{
		//TODO posiblemente aca haya que disparar otro thread para que haga el recv y continue recibiendo conexiones al mismo tiempo
		//Create thread attribute detached
		pthread_attr_t handShakeThreadAttr;
		pthread_attr_init(&handShakeThreadAttr);
		pthread_attr_setdetachstate(&handShakeThreadAttr, PTHREAD_CREATE_DETACHED);

		//Create thread for checking new connections in server socket
		pthread_t handShakeThread;
		pthread_create(&handShakeThread, &handShakeThreadAttr, (void*) handShake, parameter);

		//Destroy thread attribute
		pthread_attr_destroy(&handShakeThreadAttr);

	}// END handshakes

}
void handShake (void *parameter){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure

	t_serverData *serverData = (t_serverData*) parameter;

	//Receive message size
	int messageSize = 0;
	char *messageRcv = malloc(sizeof(messageSize));
	int receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, sizeof(messageSize));

	//Receive message using the size read before
	memcpy(&messageSize, messageRcv, sizeof(int));
	//printf("messageSize received: %d\n",messageSize);
	messageRcv = realloc(messageRcv,messageSize);
	receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, messageSize);

	printf("bytes received in message: %d\n",receivedBytes);

	//starting handshake with client connected
	t_MessageGenericHandshake *message = malloc(sizeof(t_MessageGenericHandshake));
	deserializeHandShake(message, messageRcv);

	//Now it's checked that the client is not down
	if ( receivedBytes == 0 ){
		perror("The client went down while handshaking!"); //TODO => Agregar logs con librerias
		printf("Please check the client: %d is down!\n", serverData->socketClient);
		log_info(logNucleo, "Se conecto la CPU %d", message->message);
		close(serverData->socketClient);
	}else{
		switch ((int) message->process){
			case CPU:{
				printf("%s\n",message->message);
				exitCode = sendClientAcceptation(&serverData->socketClient);

				if (exitCode == EXIT_SUCCESS){
					t_MessageNucleo_CPU *datosCPU = malloc(sizeof(t_MessageNucleo_CPU));
					datosCPU->id = numCPU;
					datosCPU->numSocket = serverData->socketClient;
					list_add(listaCPU, (void*)datosCPU);
					numCPU++;

					//Create thread attribute detached
					pthread_attr_t processMessageThreadAttr;
					pthread_attr_init(&processMessageThreadAttr);
					pthread_attr_setdetachstate(&processMessageThreadAttr, PTHREAD_CREATE_DETACHED);

					//Create thread for checking new connections in server socket
					pthread_t processMessageThread;
					pthread_create(&processMessageThread, &processMessageThreadAttr, (void*) processMessageReceived, parameter);

					//Destroy thread attribute
					pthread_attr_destroy(&processMessageThreadAttr);
				}

				break;
			}
			case CONSOLA:{
				printf("%s\n",message->message);
				exitCode = sendClientAcceptation(&serverData->socketClient);

				if (exitCode == EXIT_SUCCESS){

					//Create thread attribute detached
					pthread_attr_t processMessageThreadAttr;
					pthread_attr_init(&processMessageThreadAttr);
					pthread_attr_setdetachstate(&processMessageThreadAttr, PTHREAD_CREATE_DETACHED);

					//Create thread for checking new connections in server socket
					pthread_t processMessageThread;
					pthread_create(&processMessageThread, &processMessageThreadAttr, (void*) processMessageReceived, parameter);

					//Destroy thread attribute
					pthread_attr_destroy(&processMessageThreadAttr);
				}

				break;
			}
			case UMC:{
				printf("%s\n",message->message);
				exitCode = sendClientAcceptation(&serverData->socketClient);

				if (exitCode == EXIT_SUCCESS){

					//Create thread attribute detached
					pthread_attr_t processMessageThreadAttr;
					pthread_attr_init(&processMessageThreadAttr);
					pthread_attr_setdetachstate(&processMessageThreadAttr, PTHREAD_CREATE_DETACHED);

					//Create thread for checking new connections in server socket
					pthread_t processMessageThread;
					pthread_create(&processMessageThread, &processMessageThreadAttr, (void*) processMessageReceived, parameter);

					//Destroy thread attribute
					pthread_attr_destroy(&processMessageThreadAttr);
					break;
				}
			break;
			}
			default:{
				perror("Process not allowed to connect");//TODO => Agregar logs con librerias
				printf("Invalid process '%d' tried to connect to UMC\n",(int) message->process);
				close(serverData->socketClient);
				break;
			}
		}
 	}

	free(messageRcv);
	free(message->message);
	free(message);
}

void processMessageReceived (void *parameter){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure

	t_serverData *serverData = (t_serverData*) parameter;

	//Receive message size
	int messageSize = 0;
	char *messageRcv = malloc(sizeof(messageSize));
	int receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, sizeof(messageSize));

	if ( receivedBytes > 0 ){
		messageSize = atoi(messageRcv);

		//Receive process from which the message is going to be interpreted
		enum_processes fromProcess;
		messageRcv = realloc(messageRcv, sizeof(fromProcess));
		receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, sizeof(fromProcess));

		//Receive message using the size read before
		fromProcess = (enum_processes) messageRcv;
		messageRcv = realloc(messageRcv, messageSize);
		receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, messageSize);

		printf("bytes received: %d\n",receivedBytes);

		switch (fromProcess){
			case CONSOLA:{
				printf("Processing CONSOLA message received\n");
				/*t_MessageNucleo_Consola *message = malloc(sizeof(t_MessageNucleo_Consola));
				deserializeConsola_Nucleo(message, messageRcv);
				printf("se recibio el processID #%d\n", message->processID);
				free(message);*/
				break;
			}
			case CPU: {
				printf("Processing CPU message received\n");
				t_MessageNucleo_CPU *message;
				char *messageRcv = (char*) malloc(sizeof(t_MessageNucleo_CPU));
				char *datosEntradaSalida = malloc(sizeof(t_es));
				t_es infoES;
				memset(messageRcv, '\0', sizeof(t_MessageNucleo_CPU));
				recv(serverData->socketClient,(void*)messageRcv,sizeof(t_MessageNucleo_CPU),0);
				deserializeCPU_Nucleo(message, messageRcv);
				switch (message->operacion) {
					case 1: //Entrada Salida
						recv(serverData->socketClient, (void*) datosEntradaSalida, sizeof(t_es),MSG_WAITALL);
						deserializeIO(&infoES, &datosEntradaSalida);
						hacerEntradaSalida(serverData->socketClient, message->processID,infoES.ProgramCounter, infoES.tiempo);
						break;
					case 2: //Finaliza Proceso Bien
						finalizaProceso(serverData->socketClient, message->processID,message->operacion);
						break;
					case 3: //Finaliza Proceso Mal
						finalizaProceso(serverData->socketClient, message->processID,message->operacion);
						break;
					case 4:	//Falla otra cosa
						printf("Hubo un fallo.\n");
						break;
					case 5: //Corte por Quantum
						printf("Corto por Quantum.\n");
						atenderCorteQuantum(serverData->socketClient, message->processID);
						break;
					default:
						printf("Mensaje recibido invalido, CPU desconectado.\n");
						abort();
				} //fin del switch interno
				printf("Processing CPU message received\n");
				break;
			}
			case UMC:{
				printf("Processing UMC message received\n");
				break;
			}
			default:{
				perror("Process not allowed to connect");//TODO => Agregar logs con librerias
				printf("Invalid process '#%d' tried to connect to NUCLEO \n",(int) fromProcess);
				close(serverData->socketClient);
				break;
			}
		}

	}else if (receivedBytes == 0 ){
		//The client is down when bytes received are 0
		perror("One of the clients is down!"); //TODO => Agregar logs con librerias
		printf("Please check the client: %d is down!\n", serverData->socketClient);
	}else{
		perror("Error - No able to received");//TODO => Agregar logs con librerias
		printf("Error receiving from socket '%d', with error: %d\n",serverData->socketClient,errno);
	}

	free(messageRcv);

}

void correrPath(char* path){
	//Creo el PCB del proceso.
	t_PCB* PCB = malloc(sizeof(t_PCB));
	t_proceso* datosProceso = malloc(sizeof(t_proceso));

	PCB->PID = idProcesos;
	strcpy(PCB->path,path);
	PCB->ProgramCounter = 0;
	PCB->estado=1;
	idProcesos++;
	PCB->finalizar = 0;
	list_add(listaProcesos,(void*)PCB);
	log_info(logNucleo, "myProcess %d - Iniciado  Path: %s",PCB->PID, path);

	//Agrego a la Cola de Listos
	datosProceso->ProgramCounter = PCB->ProgramCounter;
	datosProceso->PID = PCB->PID;

	pthread_mutex_lock(&cListos);
	queue_push(colaListos, (void*)datosProceso);
	pthread_mutex_unlock(&cListos);

	planificarProceso();

}

void atenderCorteQuantum(int socket,int PID){
	//Libero la CPU
	liberarCPU(socket);

	//Cambio el PC del Proceso, le sumo el quantum al PC actual.
	t_PCB* infoProceso;
	int buscar = buscarPCB(PID);
	infoProceso = (t_PCB*)list_get(listaProcesos,buscar);
	int pcnuevo;
	pcnuevo = infoProceso->ProgramCounter + configuration.quantum;
	actualizarPC(PID,pcnuevo);
	//Agrego el proceso a la Cola de Listos
	t_proceso* datosProceso = malloc(sizeof(t_proceso));
	datosProceso->PID = PID;
	datosProceso->ProgramCounter = pcnuevo;
	if (infoProceso->finalizar == 0){

	//Cambio el estado del proceso
	int estado = 1;
	cambiarEstadoProceso(PID, estado);
	pthread_mutex_lock(&cListos);
	queue_push(colaListos,(void*)datosProceso);
	pthread_mutex_unlock(&cListos);
	}else {
			pthread_mutex_lock(&cFinalizar);
			queue_push(colaFinalizar,(void*)datosProceso);
			pthread_mutex_unlock(&cFinalizar);
	}

	//Mando nuevamente a PLanificar
	planificarProceso();
}

void finalizaProceso(int socket, int PID, int estado) {

	int posicion = buscarPCB(PID);
	int estadoProceso;
	//Cambio el estado del proceso, y guardo en Log que finalizo correctamente
	t_PCB* datosProceso;
	datosProceso = (t_PCB*) list_get(listaProcesos, posicion);
	if (estado == 2) {
		estadoProceso = 4;
		cambiarEstadoProceso(PID, estadoProceso);
		log_info(logNucleo, "myProcess %d path %s - Finalizo correctamente",
				datosProceso->PID, datosProceso->path);
	} else {
		if (estado == 3) {
			estadoProceso = 5;
			cambiarEstadoProceso(PID, estadoProceso);
			log_info(logNucleo,
					"myProcess %d path %s - Finalizo incorrectamente por falta de espacio en Swap",
					datosProceso->PID, datosProceso->path);
		}
	}
	//Libero la CPU
	liberarCPU(socket);

	//Mando a revisar si hay alguno en la lista para ejecutar.
	planificarProceso();

}

void planificarProceso() {
	//Veo si hay procesos para planificar en la cola de Listos
	if (queue_is_empty(colaListos) && (queue_is_empty(colaFinalizar))) {
		return;
	}
	//Veo si hay CPU libre para asignarle
	int libre = buscarCPULibre();

	if (libre == -1) {
		printf("No hay CPU libre.\n");
		return;
	}

	//Le envio la informacion a la CPU
	t_PCB* datosPCB;
	t_MessageNucleo_CPU contextoProceso;
	t_proceso* datosProceso;
	contextoProceso.head = 0;
	contextoProceso.cantInstruc = 0;

	if (queue_is_empty(colaFinalizar)) {

		pthread_mutex_lock(&cListos);
		datosProceso = (t_proceso*) queue_peek(colaListos);
		pthread_mutex_unlock(&cListos);

		int posicion = buscarPCB(datosProceso->PID);
		if (posicion != -1) {
			datosPCB = (t_PCB*) list_get(listaProcesos, posicion);
			char* bufferEnviar = malloc(sizeof(t_MessageNucleo_CPU));

			contextoProceso.cantInstruc = configuration.quantum;

			contextoProceso.ProgramCounter = datosPCB->ProgramCounter;
			strcpy(contextoProceso.path, datosPCB->path);
			contextoProceso.processID = datosPCB->PID;
			//Saco el primer elemento de la cola, porque ya lo planifique.
			pthread_mutex_lock(&cListos);
			queue_pop(colaListos);
			pthread_mutex_unlock(&cListos);

			send(libre, bufferEnviar, sizeof(t_MessageNucleo_CPU), 0);
			//Cambio Estado del Proceso
			int estado = 2;
			cambiarEstadoProceso(datosPCB->PID, estado);

		} else {
			printf("Proceso no encontrado en la lista.\n");
		}
	} else {
		pthread_mutex_lock(&cFinalizar);
		datosProceso = (t_proceso*) queue_peek(colaFinalizar);
		pthread_mutex_unlock(&cFinalizar);
		int posicion = buscarPCB(datosProceso->PID);
		if (posicion != -1) {
			datosPCB = (t_PCB*) list_get(listaProcesos, posicion);
			char* bufferEnviar = malloc(sizeof(t_MessageNucleo_CPU));

			contextoProceso.head = 1;

			contextoProceso.ProgramCounter = datosPCB->ProgramCounter;
			strcpy(contextoProceso.path, datosPCB->path);
			contextoProceso.processID = datosPCB->PID;
			serializeNucleo_CPU(&contextoProceso, bufferEnviar, sizeof(t_MessageNucleo_CPU));
			//Saco el primer elemento de la cola, porque ya lo planifique.
			pthread_mutex_lock(&cFinalizar);
			queue_pop(colaFinalizar);
			pthread_mutex_unlock(&cFinalizar);

			send(libre, bufferEnviar, sizeof(t_MessageNucleo_CPU), 0);
			//Cambio Estado del Proceso
			int estado = 2;
			cambiarEstadoProceso(datosPCB->PID, estado);
		}
	}

}

int buscarCPULibre() {
	int cantCPU, i = 0;
	t_MessageNucleo_CPU* datosCPU;
	cantCPU = list_size(listaCPU);
	for (i = 0; i < cantCPU; i++) {
		datosCPU = (t_MessageNucleo_CPU*) list_get(listaCPU, i);
		if (datosCPU->processStatus == NEW) {
			datosCPU->processStatus = READY;
			return datosCPU->numSocket;
		}
	}
	return -1;
}

int buscarPCB(int id) {
	t_PCB* datosPCB;
	int i = 0;
	int cantPCB = list_size(listaProcesos);
	for (i = 0; i < cantPCB; i++) {
		datosPCB = list_get(listaProcesos, i);
		if (datosPCB->PID == id) {
			return i;
		}
	}
	return -1;
}

int buscarCPU(int socket) {
	t_MessageNucleo_CPU* datosCPU;
	int i = 0;
	int cantCPU = list_size(listaCPU);
	for (i = 0; i < cantCPU; i++) {
		datosCPU = list_get(listaCPU, i);
		if (datosCPU->numSocket == socket) {
			return i;
		}
	}
	return -1;
}

void hacerEntradaSalida(int socket, int PID, int ProgramCounter, int tiempo) {

	t_bloqueado* infoBloqueado = malloc(sizeof(t_bloqueado));
//Libero la CPU que ocupaba el proceso
	liberarCPU(socket);
//Cambio el estado del proceso
	int estado = 3;
	cambiarEstadoProceso(PID, estado);
//Cambio el PC del Proceso
	actualizarPC(PID, ProgramCounter);
//Agrego a la cola de Bloqueados, y seteo el semaforo
	infoBloqueado->PID = PID;
	infoBloqueado->tiempo = tiempo;

	pthread_mutex_lock(&cBloqueados);
	queue_push(colaBloqueados, (void*) infoBloqueado);
	pthread_mutex_unlock(&cBloqueados);

	sem_post(&semBloqueados);
}

void liberarCPU(int socket) {
	int liberar = buscarCPU(socket);
	if (liberar != -1) {
		t_MessageNucleo_CPU* datosCPU;
		datosCPU = (t_MessageNucleo_CPU*) list_get(listaCPU, liberar);
		datosCPU->processStatus = NEW;
		planificarProceso();
	} else {
		printf("Error al liberar CPU \n CPU no encontrada en la lista.\n");
	}
}

void cambiarEstadoProceso(int PID, int estado) {
	int cambiar = buscarPCB(PID);
	if (cambiar != -1) {
		t_PCB* datosProceso;
		datosProceso = (t_PCB*) list_get(listaProcesos, cambiar);
		datosProceso->estado = estado;
	} else {
		printf("Error al cambiar estado de proceso, proceso no encontrado en la lista.\n");
	}
}

void atenderBloqueados() {
	while (1) {
		sem_wait(&semBloqueados);
		t_bloqueado* datosProceso;
		printf("Entre a ejecutar E/S de Bloqueados.\n");
		pthread_mutex_lock(&cBloqueados);
		datosProceso = (t_bloqueado*) queue_peek(colaBloqueados);
		pthread_mutex_unlock(&cBloqueados);
		sleep(datosProceso->tiempo);
		//Proceso en estado ready
		int estado = 1;
		cambiarEstadoProceso(datosProceso->PID, estado);
		//Agrego a la Lista de Listos el Proceso
		int buscar = buscarPCB(datosProceso->PID);
		t_PCB * informacionDeProceso = list_get(listaProcesos, buscar);
		t_proceso* infoProceso = (t_proceso*) malloc(sizeof(t_proceso));
		t_PCB* datos;
		if (buscar != -1) {
			datos = (t_PCB*) list_get(listaProcesos, buscar);
			infoProceso->PID = datos->PID;
			infoProceso->ProgramCounter = datos->ProgramCounter;
			pthread_mutex_lock(&cBloqueados);
			queue_pop(colaBloqueados);
			pthread_mutex_unlock(&cBloqueados);
			if (informacionDeProceso->finalizar == 0) {
				pthread_mutex_lock(&cListos);
				queue_push(colaListos, (void*) infoProceso);
				pthread_mutex_unlock(&cListos);
			} else {
				pthread_mutex_lock(&cFinalizar);
				queue_push(colaFinalizar, (void*) infoProceso);
				pthread_mutex_unlock(&cFinalizar);
			}
			planificarProceso();
		} else {
			printf("Proceso no encontrado en la lista.\n");
		}
	}
}

void actualizarPC(int PID, int ProgramCounter) {
	int cambiar = buscarPCB(PID);
	if (cambiar != -1) {
		t_PCB* datosProceso;
		datosProceso = (t_PCB*) list_get(listaProcesos, cambiar);
		datosProceso->ProgramCounter = ProgramCounter;
	} else {
		printf("Error al cambiar el PC del proceso, proceso no encontrado en la lista.\n");
	}
}

void deserializeIO(t_es* datos, char** buffer) {
	int offset = 0;
	memcpy(&datos->tiempo, *buffer, sizeof(datos->tiempo));
	offset += sizeof(datos->tiempo);

	memcpy(&datos->ProgramCounter, *buffer + offset, sizeof(datos->ProgramCounter));
	offset += sizeof(datos->ProgramCounter);
}

void getConfiguration(char *configFile){

	FILE *file = fopen(configFile, "r");

	assert(("ERROR - Could not open the configuration file", file != 0));// ERROR - Could not open file TODO => Agregar logs con librerias

	char parameter[20]; //[12] is the max paramenter's name size
	char parameterValue[20];

	while ((fscanf(file, "%s", parameter)) != EOF){

		switch(getEnum(parameter)){
			case(PUERTO_PROG):{ //1
				fscanf(file, "%s",parameterValue);
				configuration.puerto_prog = (strcmp(parameter, EOL_DELIMITER) != 0) ? atoi(parameterValue) : 0 /*DEFAULT VALUE*/;
				break;
			}
			case(PUERTO_CPU):{ //2
				fscanf(file, "%s",parameterValue);
				configuration.puerto_cpu = (strcmp(parameter, EOL_DELIMITER) != 0) ? atoi(parameterValue) : 0 /*DEFAULT VALUE*/;
				break;
			}
			case(QUANTUM):{ //3
				fscanf(file, "%s",parameterValue);
				configuration.quantum = (strcmp(parameter, EOL_DELIMITER) != 0) ? atoi(parameterValue) : 0 /*DEFAULT VALUE*/;
				break;
			}
			case(QUANTUM_SLEEP):{ //4
				fscanf(file, "%s",parameterValue);
				configuration.quantum_sleep = (strcmp(parameter, EOL_DELIMITER) != 0) ? atoi(parameterValue) : 0 /*DEFAULT VALUE*/;
				break;
			}
			case(SEM_IDS):{ //5
				fscanf(file, "%s",parameterValue);
				(strcmp(parameter, EOL_DELIMITER) != 0) ? memcpy(&configuration.sem_ids, parameterValue, sizeof(configuration.sem_ids)) : "" ;
				break;
			}
			case(SEM_INIT):{ //6
				fscanf(file, "%s",parameterValue);
				(strcmp(parameter, EOL_DELIMITER) != 0) ? memcpy(&configuration.sem_init, parameterValue, sizeof(configuration.sem_init)) : "" ;
				break;
			}
			case(IO_IDS):{ //7
				fscanf(file, "%s",parameterValue);
				(strcmp(parameter, EOL_DELIMITER) != 0) ? memcpy(&configuration.io_ids, parameterValue, sizeof(configuration.io_ids)) : "" ;
				break;
			}
			case(IO_SLEEP):{ //8
				fscanf(file, "%s",parameterValue);
				(strcmp(parameter, EOL_DELIMITER) != 0) ? memcpy(&configuration.io_sleep, parameterValue, sizeof(configuration.io_sleep)) : "" ;
				break;
			}
			case(SHARED_VARS):{ //9
				fscanf(file, "%s",parameterValue);
				(strcmp(parameter, EOL_DELIMITER) != 0) ? memcpy(&configuration.shared_vars, parameterValue, sizeof(configuration.shared_vars)) : "" ;
				break;
			}
			case(STACK_SIZE):{ //10
				fscanf(file, "%s",parameterValue);
				configuration.stack_size = (strcmp(parameter, EOL_DELIMITER) != 0) ? atoi(parameterValue) : 0 /*DEFAULT VALUE*/;
				break;
			}
			default:{
				if (strcmp(parameter, EOL_DELIMITER) != 0){
					perror("Error - Parameter read not recognized");//TODO => Agregar logs con librerias
					printf("Error Parameter read not recognized '%s'\n",parameter);
				}
				break;
			}
		}// END switch(parameter)
	}

}

int getEnum(char *string){
	int parameter = -1;

	//printf("string: %s \n", string);

	strcmp(string,"PUERTO_PROG") == 0 ? parameter = PUERTO_PROG : -1 ;
	strcmp(string,"PUERTO_CPU") == 0 ? parameter = PUERTO_CPU : -1 ;
	strcmp(string,"QUANTUM") == 0 ? parameter = QUANTUM : -1 ;
	strcmp(string,"QUANTUM_SLEEP") == 0 ? parameter = QUANTUM_SLEEP : -1 ;
	strcmp(string,"SEM_IDS") == 0 ? parameter = SEM_IDS : -1 ;
	strcmp(string,"SEM_INIT") == 0 ? parameter = SEM_INIT : -1 ;
	strcmp(string,"IO_IDS") == 0 ? parameter = IO_IDS : -1 ;
	strcmp(string,"IO_SLEEP") == 0 ? parameter = IO_SLEEP : -1 ;
	strcmp(string,"SHARED_VARS") == 0 ? parameter = SHARED_VARS : -1 ;
	strcmp(string,"STACK_SIZE") == 0 ? parameter = STACK_SIZE : -1 ;

	//printf("parameter: %d \n", parameter);

	return parameter;
}
