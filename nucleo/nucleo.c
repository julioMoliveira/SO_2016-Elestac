/*
 * nucleo.c
 *
 */
 
 #include "nucleo.h"

int main(int argc, char *argv[]) {
	int exitCode = EXIT_FAILURE; //DEFAULT failure
	char *configurationFile = NULL;
	char *logFile = NULL;
	pthread_t serverThread;
	pthread_t serverConsolaThread;
	pthread_t hiloBloqueados;

/*

	assert(("ERROR - NOT arguments passed", argc > 1)); // Verifies if was passed at least 1 parameter, if DONT FAILS

	//get parameter
	int i;
	for( i = 0; i < argc; i++){
		if (strcmp(argv[i], "-c") == 0){
			configurationFile = argv[i+1];
			printf("Configuration File: '%s'\n",configurationFile);
		}
		//check log file parameter
		if (strcmp(argv[i], "-l") == 0){
			logFile = argv[i+1];
			printf("Log File: '%s'\n",logFile);
		}
		}
	}

	//ERROR if not configuration parameter was passed
		assert(("ERROR - NOT configuration file was passed as argument", configurationFile != NULL));//Verifies if was passed the configuration file as parameter, if DONT FAILS

	//ERROR if not Log parameter was passed
		assert(("ERROR - NOT log file was passed as argument", logNucleo != NULL));//Verifies if was passed the Log file as parameter, if DONT FAILS

	//Creo archivo de configuracion
		crearArchivoDeConfiguracion(configurationFile);
		*/
	//Creo el archivo de Log
		logNucleo = log_create(logFile, "NUCLEO", 0, LOG_LEVEL_TRACE);
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

	pthread_mutex_init(&activeProcessMutex, NULL);

	//Create thread for server start
	pthread_create(&serverThread, NULL, (void*) startServerProg, NULL);
	pthread_create(&serverConsolaThread, NULL, (void*) startServerCPU, NULL);
	//Create thread for blocked processes
	pthread_create(&hiloBloqueados, NULL, (void*)atenderBloqueados, NULL);

	exitCode = connectTo(UMC,&socketUMC);
	if (exitCode == EXIT_SUCCESS) {
		 log_info(logNucleo, "NUCLEO connected to UMC successfully\n");
		// TODO solicitarle lás páginas necesarias para almacenar el codeScript y el stack.
		// TODO enviarle el codigo completo del programa

	}else{
		printf("No server available - shutting down proces!!\n");
		return EXIT_FAILURE;
	}

	pthread_join(serverThread, NULL);
	pthread_join(serverConsolaThread, NULL);
	pthread_join(hiloBloqueados, NULL);

	return exitCode;
}

void startServerProg(){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure
	t_serverData serverData;

	exitCode = openServerConnection(configNucleo.puerto_prog, &serverData.socketServer);
	log_info(logNucleo, "SocketServer: %d\n",serverData.socketServer);

	//If exitCode == 0 the server connection is opened and listening
	if (exitCode == 0) {
		log_info(logNucleo, "the server is opened\n");

		exitCode = listen(serverData.socketServer, SOMAXCONN);

		if (exitCode < 0 ){
			log_error(logNucleo,"Failed to listen server Port.\n");
			return;
		}

		while (1){
			newClients((void*) &serverData);
		}
	}

}

void startServerCPU(){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure
	t_serverData serverData;

	exitCode = openServerConnection(configNucleo.puerto_cpu, &serverData.socketServer);
	log_info(logNucleo, "SocketServer: %d\n",serverData.socketServer);

	//If exitCode == 0 the server connection is opened and listening
	if (exitCode == 0) {
		log_info(logNucleo, "the server is opened\n");

		exitCode = listen(serverData.socketServer, SOMAXCONN);

		if (exitCode < 0 ){
			log_error(logNucleo,"Failed to listen server Port.\n");
			return;
		}

		while (1){
			newClients((void*) &serverData);
		}
	}

}

void newClients (void *parameter){
	int exitCode = EXIT_FAILURE; //DEFAULT Failure

	t_serverData *serverData = (t_serverData*) parameter;
	//TODO disparar un thread para acceptar cada cliente nuevo (debido a que el accept es bloqueante) y para hacer el handshake
/**************************************/
	//Create thread attribute detached
//			pthread_attr_t acceptClientThreadAttr;
//			pthread_attr_init(&acceptClientThreadAttr);
//			pthread_attr_setdetachstate(&acceptClientThreadAttr, PTHREAD_CREATE_DETACHED);
//
//			//Create thread for checking new connections in server socket
//			pthread_t acceptClientThread;
//			pthread_create(&acceptClientThread, &acceptClientThreadAttr, (void*) acceptClientConnection1, &serverData);
//
//			//Destroy thread attribute
//			pthread_attr_destroy(&acceptClientThreadAttr);
/************************************/

	exitCode = acceptClientConnection(&serverData->socketServer, &serverData->socketClient);

	if (exitCode == EXIT_FAILURE){
		log_warning(logNucleo,"There was detected an attempt of wrong connection\n");
		close(serverData->socketClient);
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
	messageRcv = realloc(messageRcv,messageSize);
	receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, messageSize);

	//starting handshake with client connected
	t_MessageGenericHandshake *message = malloc(sizeof(t_MessageGenericHandshake));
	deserializeHandShake(message, messageRcv);

	//Now it's checked that the client is not down
	if ( receivedBytes == 0 ){
		log_error(logNucleo,
				"The client went down while handshaking! - Please check the client '%d' is down!\n",
				serverData->socketClient);
		close(serverData->socketClient);
	}else{
		switch ((int) message->process){
			case CPU:{
				log_info(logNucleo,"Message from '%s': %s\n", getProcessString(message->process), message->message);
				close(serverData->socketClient);
				exitCode = sendClientAcceptation(&serverData->socketClient);

				if (exitCode == EXIT_SUCCESS){

					t_MessageNucleo_CPU *datosCPU = malloc(sizeof(t_MessageNucleo_CPU));
					datosCPU->numSocket = serverData->socketClient;
					list_add(listaCPU, (void*)datosCPU);
					free(datosCPU);
				}

				break;
			}
			case CONSOLA:{
			log_info(logNucleo, "Message from '%s': %s\n",
					getProcessString(message->process), message->message);
				exitCode = sendClientAcceptation(&serverData->socketClient);

				if (exitCode == EXIT_SUCCESS){

					//processMessageReceived(parameter);

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
			default:{
			log_error(logNucleo,
					"Process not allowed to connect - Invalid process '%s' tried to connect to UMC\n",
					getProcessString(message->process));
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

	t_serverData *serverData = (t_serverData*) parameter;

	//Receive message size
	int messageSize = 0;
	char *messageRcv = malloc(sizeof(messageSize));
	int receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, sizeof(messageSize));

	if ( receivedBytes > 0 ){
		messageSize = atoi(messageRcv);

		//Get Payload size
		messageSize = atoi(messageRcv);

		//Receive process from which the message is going to be interpreted
		enum_processes fromProcess;
		messageRcv = realloc(messageRcv, sizeof(fromProcess));
		receivedBytes = receiveMessage(&serverData->socketClient, messageRcv, sizeof(fromProcess));
		fromProcess = (enum_processes) messageRcv;

		log_info(logNucleo,"Bytes received from process '%s': %d\n",getProcessString(fromProcess),receivedBytes);

		switch (fromProcess){
			case CONSOLA:{
				log_info(logNucleo, "Processing CONSOLA message received\n");
				pthread_mutex_lock(&activeProcessMutex);
				int opFinalizar;

				//Deserializar messageRcv
				//memcpy(message, messageRcv,sizeof(messageRcv)); //TODO deserealizar mensaje recibido

				//Finaliza Proceso
				if(opFinalizar==-1){
				//finalizaProceso(serverData->socketClient, message->processID, message->processStatus); //TODO finalizar proceso
				}

				runScript(messageRcv);

				receiveMessage(&serverData->socketClient, messageRcv, sizeof(fromProcess));

				pthread_mutex_unlock(&activeProcessMutex);
			break;
		}
			default:{
			log_error(logNucleo,
					"Process not allowed to connect - Invalid process '%s' send a message to UMC\n",
					getProcessString(fromProcess));
			close(serverData->socketClient);
				break;
			}
		}

	}else if (receivedBytes == 0 ){
		//The client is down when bytes received are 0
		log_error(logNucleo,
				"The client went down while receiving! - Please check the client '%d' is down!\n",
				serverData->socketClient);
		close(serverData->socketClient);
	}else{
		log_error(logNucleo,
				"Error - No able to received - Error receiving from socket '%d', with error: %d\n",
				serverData->socketClient, errno);
		close(serverData->socketClient);
	}

	free(messageRcv);

}

void runScript(char* codeScript){
	//Creo el PCB del proceso.
	t_PCB* PCB = malloc(sizeof(t_PCB));
	t_proceso* datosProceso = malloc(sizeof(t_proceso));

	PCB->PID = idProcesos;
	PCB->ProgramCounter = 0;
	PCB->estado=1;
	idProcesos++;
	PCB->finalizar = 0;
	list_add(listaProcesos,(void*)PCB);
	log_info(logNucleo, "myProcess %d - Iniciado  Script: %s",PCB->PID, codeScript);

	//Agrego a la Cola de Listos
	datosProceso->ProgramCounter = PCB->ProgramCounter;
	datosProceso->PID = PCB->PID;

	pthread_mutex_lock(&cListos);
	queue_push(colaListos, (void*)datosProceso);
	pthread_mutex_unlock(&cListos);

	planificarProceso();

	free(PCB);
	free(datosProceso);

}

void planificarProceso() {
	int exitCode = EXIT_FAILURE;
	//Veo si hay procesos para planificar en la cola de Listos
	if (queue_is_empty(colaListos) && (queue_is_empty(colaFinalizar))) {
		return;
	}
	//Veo si hay CPU libre para asignarle
	int libreCPU = buscarCPULibre();

	if (libreCPU == -1) {
		printf("No hay CPU libre.\n");
		return;
	}

	//Le envio la informacion a la CPU
	t_PCB* datosPCB;
	t_MessageNucleo_CPU* contextoProceso = malloc(sizeof(t_MessageNucleo_CPU));
	t_proceso* datosProceso;
	contextoProceso->head = 0;
	contextoProceso->cantInstruc = 0;

	if (queue_is_empty(colaFinalizar)) {

		pthread_mutex_lock(&cListos);
		datosProceso = (t_proceso*) queue_peek(colaListos);
		pthread_mutex_unlock(&cListos);

		int posicion = buscarPCB(datosProceso->PID);
		if (posicion != -1) {
			datosPCB = (t_PCB*) list_get(listaProcesos, posicion);
			contextoProceso->cantInstruc = configNucleo.quantum;

			exitCode = procesarMensajeCPU(datosPCB, datosProceso, contextoProceso, libreCPU);

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
			contextoProceso->head = 1;

			exitCode = procesarMensajeCPU(datosPCB, datosProceso, contextoProceso,libreCPU);

		} else {
			printf("Proceso no encontrado en la lista.\n");
		}
	}
	free(contextoProceso);
}

int procesarMensajeCPU(t_PCB* datosPCB, t_proceso* datosProceso,t_MessageNucleo_CPU* contextoProceso,int libreCPU){
		int exitCode = EXIT_FAILURE;
		char* bufferEnviar = malloc(sizeof(t_MessageNucleo_CPU));

		contextoProceso->ProgramCounter = datosPCB->ProgramCounter;

		contextoProceso->processID = datosPCB->PID;

		//TODO enviar metadata

		serializeNucleo_CPU(contextoProceso, bufferEnviar,sizeof(t_MessageNucleo_CPU));

		//Saco el primer elemento de la cola, porque ya lo planifique.
		pthread_mutex_lock(&cListos);
		queue_pop(colaListos);
		pthread_mutex_unlock(&cListos);

		send(libreCPU, bufferEnviar, sizeof(t_MessageNucleo_CPU), 0);

		//Cambio Estado del Proceso
		int estado = 2;
		cambiarEstadoProceso(datosPCB->PID, estado);

		//1) rcv();
		//processMessageReceived()

		//2)

		exitCode = procesarRespuesta(libreCPU);

		free(bufferEnviar);
		return exitCode;

}

int procesarRespuesta(int socketLibre){
	printf("Processing CPU message \n");
	int exitCode = EXIT_FAILURE;

	t_MessageNucleo_CPU *message=malloc(sizeof(t_MessageNucleo_CPU));
	char *messageRcv = malloc(sizeof(t_MessageNucleo_CPU));

	char *mensajePrivilegiado = malloc(sizeof(t_MessageNucleo_CPU));

	t_es infoES;
	memset(messageRcv, '\0', sizeof(t_MessageNucleo_CPU));
	exitCode = receiveMessage(&socketLibre,(void*)messageRcv,sizeof(t_MessageNucleo_CPU));

	//Deserializar messageRcv
	deserializeCPU_Nucleo(message, messageRcv);

	//TODO Deserializar mensajePrivilegiado
	exitCode = receiveMessage(&socketLibre,(void*)mensajePrivilegiado,sizeof(t_MessageNucleo_CPU));

	switch (message->operacion) {
	case 1: //Entrada Salida
		pthread_mutex_lock(&activeProcessMutex);
		char *datosEntradaSalida = malloc(sizeof(t_es));

		//change active PID
		activePID = message->processID;

		exitCode = recv(socketLibre, (void*) datosEntradaSalida, sizeof(t_es),0);
		deserializarES(&infoES, datosEntradaSalida); //TODO deserializar entrada-salida

		//Libero la CPU que ocupaba el proceso
		liberarCPU(socketLibre);

		//Cambio el PC del Proceso
		actualizarPC(message->processID, infoES.ProgramCounter);

		//Cambio el estado del proceso
		int estado = 3;
		cambiarEstadoProceso(message->processID, estado);

		EntradaSalida(infoES.dispositivo, infoES.tiempo);

		free(datosEntradaSalida);
		pthread_mutex_unlock(&activeProcessMutex);
		break;
	case 2: //Finaliza Proceso Bien
		finalizaProceso(socketLibre, message->processID,message->processStatus);
		break;

	case 3: //Finaliza Proceso Mal
		finalizaProceso(socketLibre, message->processID,message->processStatus);
		break;

	case 4:	//Falla otra cosa
		printf("Hubo un fallo.\n");
		break;
	case 5 : //Corte por Quantum
			printf("Corto por Quantum.\n");
			atenderCorteQuantum(socketLibre, message->processID);
			break;
	case 6://obtener valor TODO
		//obtenerValor(mensajePrivilegiado->variable);
		break;
	case 7://grabar_valor TODO
		//grabarValor(mensajePrivilegiado->variable,mensajePrivilegiado->valor);
		break;
	case 8:	//wait o signal
		break;
	default:
		printf("Mensaje recibido invalido, CPU desconectado.\n");
		abort();
	}

	free(message);
	free(messageRcv);
	return exitCode;
}

void atenderCorteQuantum(int socket,int PID){
	//Libero la CPU
	liberarCPU(socket);

	//Cambio el PC del Proceso, le sumo el quantum al PC actual.
	t_PCB* infoProceso;
	int buscar = buscarPCB(PID);
	infoProceso = (t_PCB*)list_get(listaProcesos,buscar);
	int pcnuevo;
	pcnuevo = infoProceso->ProgramCounter + configNucleo.quantum;
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
	} else {
		pthread_mutex_lock(&cFinalizar);
		queue_push(colaFinalizar, (void*) datosProceso);
		pthread_mutex_unlock(&cFinalizar);
	}

	//Mando nuevamente a PLanificar
	planificarProceso();

	free(datosProceso);
}

void finalizaProceso(int socket, int PID, int estado) {

	int posicion = buscarPCB(PID);
	int estadoProceso;
	//Cambio el estado del proceso, y guardo en Log que finalizo correctamente
	t_PCB* datosProceso;
	pthread_mutex_lock(&listadoProcesos);
	datosProceso = (t_PCB*) list_get(listaProcesos, posicion);
	pthread_mutex_unlock(&listadoProcesos);
	if (estado == 2) {
		estadoProceso = 4;
		cambiarEstadoProceso(PID, estadoProceso);
		log_info(logNucleo, "myProcess %d - Finalizo correctamente",datosProceso->PID);
	} else {
		if (estado == 3) {
			estadoProceso = 5;
			cambiarEstadoProceso(PID, estadoProceso);
			log_info(logNucleo,
					"myProcess %d - Finalizo incorrectamente por falta de espacio en Swap",
					datosProceso->PID);
		}
	}

	//TODO enviarle el mensaje del log a la Consola asociada y destruir PCB

	//Libero la CPU
	liberarCPU(socket);

	//Mando a revisar si hay alguno en la lista para ejecutar.
	planificarProceso();

}

int buscarCPULibre() {
	int cantCPU, i=0;
	t_MessageNucleo_CPU* datosCPU;
	cantCPU = list_size(listaCPU);
	for(i=0;i<cantCPU;i++){
		pthread_mutex_lock(&listadoCPU);
		datosCPU = (t_MessageNucleo_CPU*) list_get(listaCPU, 0);
		pthread_mutex_unlock(&listadoCPU);

		if (datosCPU->estadoCPU == 0){
			datosCPU->estadoCPU = 1;
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
		pthread_mutex_lock(&listadoProcesos);
		datosPCB = list_get(listaProcesos, i);
		pthread_mutex_unlock(&listadoProcesos);
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
		pthread_mutex_lock(&listadoCPU);
		datosCPU = list_get(listaCPU, i);
		pthread_mutex_unlock(&listadoCPU);
		if (datosCPU->numSocket == socket) {
			return i;
		}
	}
	return -1;
}

void EntradaSalida(t_nombre_dispositivo dispositivo, int tiempo){

	t_bloqueado* infoBloqueado = malloc(sizeof(t_bloqueado));

	//Agrego a la cola de Bloqueados, y seteo el semaforo
	infoBloqueado->PID = activePID;
	infoBloqueado->dispositivo = dispositivo;
	infoBloqueado->tiempo = tiempo;
	t_bloqueado* elementoDeLaCola;
	pthread_mutex_lock(&cBloqueados);
	queue_push(colaBloqueados, (void*) infoBloqueado);
	pthread_mutex_unlock(&cBloqueados);

	sem_post(&semBloqueados);//TODO

	free(infoBloqueado);
}

void liberarCPU(int socket) {
	int liberar = buscarCPU(socket);
	if (liberar != -1) {
		t_MessageNucleo_CPU* datosCPU;
		pthread_mutex_lock(&listadoCPU);
		datosCPU = (t_MessageNucleo_CPU*) list_get(listaCPU, liberar);
		pthread_mutex_unlock(&listadoCPU);
		datosCPU->estadoCPU = 0;
		planificarProceso();
	} else {
		printf("Error al liberar CPU \n CPU no encontrada en la lista.\n");
	}
}

void cambiarEstadoProceso(int PID, int estado) {
	int cambiar = buscarPCB(PID);
	if (cambiar != -1) {
		t_PCB* datosProceso;
		pthread_mutex_lock(&listadoProcesos);
		datosProceso = (t_PCB*) list_get(listaProcesos, cambiar);
		pthread_mutex_lock(&listadoProcesos);
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
		free(infoProceso);
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

t_valor_variable *obtenerValor(t_nombre_compartida variable) {
	printf("NUCLEO: pide variable %s\n", variable);
	t_valor_variable *valorVariable = NULL;
	int i = 0;

	while (configNucleo.shared_vars[i] != NULL){

		if (strcmp(configNucleo.shared_vars[i], variable) == 0) {
			//return (int*)configNucleo.shared_vars[i];
			*valorVariable = configNucleo.shared_vars_values[i];
			break;
		}
		i++;
	}

	//TENER EN CUENTA que aca esta funcionando con el \n
	if (valorVariable == NULL){//TODO PASAR ESTE BLOQUE IF A DONDE SE LLAMA LA FUNCION obtenerValor
		printf("No encontre variable %s %d id, exit\n",variable,strlen(variable));
	}

	return valorVariable;
}

void grabarValor(t_nombre_compartida variable, t_valor_variable* valor){

	int i = 0;

	while (configNucleo.shared_vars[i] != NULL){

		if (strcmp(configNucleo.shared_vars[i], variable) == 0) {
			//return (int*)configNucleo.shared_vars[i];
			configNucleo.shared_vars_values[i] =  *valor;
			break;
		}
		i++;
	}

}

void obtenerMetadata(char *codeScript,t_metadata_program* miMetaData) {
	miMetaData = metadata_desde_literal(codeScript);

	printf("%d", miMetaData->instrucciones_serializado->start);


}

void armarIndiceDeCodigo(t_PCB unBloqueControl,t_metadata_program* miMetaData){
	int i;

	t_registroIndiceCodigo* registroAAgregar = malloc(sizeof(t_registroIndiceCodigo));

	//First instruction

	for (i=0; i < miMetaData->instrucciones_size ; i++){

		registroAAgregar->inicioDeInstruccion= miMetaData->instrucciones_serializado[i].start;
		registroAAgregar->longitudInstruccionEnBytes = miMetaData->instrucciones_serializado[i].offset;
		list_add(unBloqueControl.indiceDeCodigo,(void*)registroAAgregar);
	}
}



void armarIndiceDeEtiquetas(t_PCB unBloqueControl,t_metadata_program* miMetaData){

	unBloqueControl.indiceDeEtiquetas = miMetaData->etiquetas;

	log_trace(logNucleo,"'indiceDeEtiquetas' size: %d\n", miMetaData->etiquetas_size);
}


int definirVar(char* nombreVariable, t_registroStack miPrograma, int posicion) {
	t_vars *nuevaVariable;

	nuevaVariable->identificador = nombreVariable;
	miPrograma.pos = 0;

	list_add(miPrograma.vars, (void*) &nuevaVariable);

	miPrograma.retPos = posicion;

	return 1;
}

//TODO invocar al recibir un script
void iniciarPrograma(int PID, char *codeScript) {

	int bufferSize = 0;
	int payloadSize = 0;
	int contentLen = strlen(codeScript) + 1;	//+1 because of '\0'
	int cantPages = contentLen / configNucleo.pageSize;

	t_MessageNucleo_UMC *message = malloc(sizeof(t_MessageNucleo_UMC));

	message->operation = agregar_proceso;
	message->PID = PID;
	message->cantPages = cantPages + configNucleo.stack_size;

	payloadSize = sizeof(message->operation) + sizeof(message->PID) + sizeof(message->cantPages);
	bufferSize = sizeof(bufferSize) + payloadSize;
	char *buffer = malloc(bufferSize);

	//Serializar mensaje
	serializeNucleo_UMC(message, buffer, payloadSize);

	//1) Enviar info a la UMC - message serializado
	sendMessage(&socketUMC, buffer, bufferSize);

	//2) Despues de enviar la info, Enviar el tamanio del prgrama
	string_append(&codeScript, "\0");	//ALWAYS put \0 for finishing the string
	sendMessage(&socketUMC, (void*) contentLen, sizeof(contentLen));

	//3) Enviar programa
	sendMessage(&socketUMC, (void*) codeScript, contentLen);

	free(message);

}

//TODO invocar al procesar estado exit
void finalizarPrograma(int PID) {

	int bufferSize = 0;
	int payloadSize = 0;

	t_MessageNucleo_UMC *message = malloc(sizeof(t_MessageNucleo_UMC));

	message->operation = finalizar_proceso;
	message->PID = PID;
	message->cantPages = -1; //DEFAULT value when the operation doesn't need it

	payloadSize = sizeof(message->operation) + sizeof(message->PID) + sizeof(message->cantPages);
	bufferSize = sizeof(bufferSize) + payloadSize;

	char *buffer = malloc(bufferSize);

	//Serializar mensaje
	serializeNucleo_UMC(message, buffer, payloadSize);

	//1) Enviar info a la UMC - message serializado
	sendMessage(&socketUMC, buffer, bufferSize);

	free(message);
}

void deserializarES(t_es* datos, char* buffer) {

	memcpy(&datos->dispositivo,buffer, (strlen(datos->dispositivo)));
	int offset = strlen(datos->dispositivo);

	memcpy(&datos->tiempo, buffer+offset, sizeof(datos->tiempo));
	offset += sizeof(datos->tiempo);

	memcpy(&datos->ProgramCounter, buffer + offset, sizeof(datos->ProgramCounter));


	/* int dispositivoLen = 0;
	memcpy(&dispositivoLen, buffer + offset, sizeof(dispositivoLen));
	offset += sizeof(dispositivoLen);
	datos->dispositivo= malloc(dispositivoLen);
	memcpy(datos->dispositivo, buffer + offset, dispositivoLen);
*/
}

void crearArchivoDeConfiguracion(char *configFile){
	t_config* configuration;

	configuration = config_create(configFile);
	configNucleo.puerto_prog = config_get_int_value(configuration,"puerto_prog");
	configNucleo.puerto_cpu = config_get_int_value(configuration,"puerto_cpu");
	configNucleo.ip_umc = config_get_string_value(configuration,"ip_umc");
	configNucleo.puerto_umc = config_get_int_value(configuration,"puerto_umc");
	configNucleo.quantum = config_get_int_value(configuration,"quantum");
	configNucleo.quantum_sleep = config_get_int_value(configuration,"quantum_sleep");
	configNucleo.sem_ids =  config_get_array_value(configuration,"sem_ids");
	configNucleo.sem_init = (int**) config_get_array_value(configuration,"sem_init");
	configNucleo.io_ids =  config_get_array_value(configuration,"io_ids");
	configNucleo.io_sleep = (int**) config_get_array_value(configuration,"io_sleep");
	configNucleo.shared_vars = config_get_array_value(configuration,"shared_vars");
	configNucleo.stack_size = config_get_int_value(configuration,"stack_size");
	configNucleo.pageSize = config_get_int_value(configuration,"pageSize");
	int i = 0;
	/*while (configNucleo.io_sleep[i] != NULL){
		printf("valor %d - %d\n",i,configNucleo.io_sleep[i]);
		i++;
	}*/

	//initializing shared variables values
	i = 0;
	while (configNucleo.shared_vars[i] != NULL){
		configNucleo.shared_vars_values[i] = 0; //DEFAULT Value
		i++;
	}
}


int connectTo(enum_processes processToConnect, int *socketClient){
	int exitcode = EXIT_FAILURE;//DEFAULT VALUE
	int port = 0;
	char *ip = string_new();

	switch (processToConnect){
	case UMC:{
		string_append(&ip,configNucleo.ip_umc);
		port= configNucleo.puerto_umc;
		break;
	}
	default:{
		log_info(logNucleo,"Process '%s' NOT VALID to be connected by NUCLEO.\n", getProcessString(processToConnect));
		break;
	}
	}
	exitcode = openClientConnection(ip, port, socketClient);

	//If exitCode == 0 the client could connect to the server
	if (exitcode == EXIT_SUCCESS){

		// ***1) Send handshake
		exitcode = sendClientHandShake(socketClient,NUCLEO);

		if (exitcode == EXIT_SUCCESS){

			// ***2)Receive handshake response
			//Receive message size
			int messageSize = 0;
			char *messageRcv = malloc(sizeof(messageSize));
			int receivedBytes = receiveMessage(socketClient, messageRcv, sizeof(messageSize));

			if ( receivedBytes > 0 ){
				//Receive message using the size read before
				memcpy(&messageSize, messageRcv, sizeof(int));
				messageRcv = realloc(messageRcv,messageSize);
				receivedBytes = receiveMessage(socketClient, messageRcv, messageSize);

				//starting handshake with client connected
				t_MessageGenericHandshake *message = malloc(sizeof(t_MessageGenericHandshake));
				deserializeHandShake(message, messageRcv);

				free(messageRcv);

				switch (message->process) {
				case ACCEPTED: {
					switch (processToConnect) {
						case UMC: {
							log_info(logNucleo, "Conectado a UMC - Message: %s\n",	message->message);
							printf("Receiving frame size\n");
							//After receiving ACCEPTATION has to be received the "Tamanio de pagina" information
							receivedBytes = receiveMessage(socketClient, &frameSize,sizeof(messageSize));
							configNucleo.pageSize = frameSize;

							printf("Tamanio de pagina: %d\n", frameSize);
							break;
						}
						default: {
							log_error(logNucleo,
									"Handshake not accepted when tried to connect your '%s' with '%s'\n",
									getProcessString(processToConnect),	getProcessString(message->process));
							close(*socketClient);
							break;
						}
					}
				default:{
					log_error(logNucleo,
						"Process couldn't connect to SERVER - Not able to connect to server %s. Please check if it's down.\n",ip);
					close(*socketClient);
				break;}
				}//fin case ACCEPTED
				}
			}else if (receivedBytes == 0 ){
				//The client is down when bytes received are 0
				log_error(logNucleo,
						"The client went down while receiving! - Please check the client '%d' is down!\n",*socketClient);
				close(*socketClient);
			}else{
				log_error(logNucleo,
						"Error - No able to received - Error receiving from socket '%d', with error: %d\n",*socketClient, errno);
				 close(*socketClient);
			}
		}
	}else{
		 log_error(logNucleo, "I'm not able to connect to the server! - My socket is: '%d'\n", *socketClient);
		 close(*socketClient);
	}

	return exitcode;
}
