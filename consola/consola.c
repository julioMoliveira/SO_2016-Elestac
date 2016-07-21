/*
 * consola.c
 *
 */

#include "consola.h"

int socketNucleo=0;

int main(int argc, char **argv) {
	char *configurationFile = NULL;
	char *logFile = NULL;

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

	//ERROR if not configuration parameter was passed
	assert(("ERROR - NOT configuration file was passed as argument", configurationFile != NULL));//Verifies if was passed the configuration file as parameter, if DONT FAILS

	//ERROR if not Log parameter was passed
	assert(("ERROR - NOT log file was passed as argument", logFile != NULL));//Verifies if was passed the Log file as parameter, if DONT FAILS

	//Creo archivo de configuracion
	//configurationFile = "/home/utnso/git/tp-2016-1c-YoNoFui/consola/configuracion.consola";
	crearArchivoDeConfiguracion(configurationFile);

	//Creo el archivo de Log
	logConsola = log_create(logFile, "NUCLEO", 0, LOG_LEVEL_TRACE);

	char* codeScript = string_new();
	int exitCode = EXIT_FAILURE;//por default EXIT_FAILURE
	char inputTeclado[250];

	exitCode = connectTo(NUCLEO, &socketNucleo);
	if (exitCode == EXIT_SUCCESS) {
		log_info(logConsola,"CONSOLA connected to NUCLEO successfully\n");
	}else{
		log_error(logConsola,"No server available - shutting down proces!!\n");
		return EXIT_FAILURE;
	}

	int tamanioArchivo;
	enum_processes fromProcess = CONSOLA;

	while (1) {
		tamanioArchivo =-1;
		printf(PROMPT);
		fgets(inputTeclado, sizeof(inputTeclado), stdin);
		char ** comando = string_split(inputTeclado, " ");
		switch (reconocerComando(comando[0])) {
		case 1: {
			printf("Comando Reconocido.\n");

			codeScript = leerArchivoYGuardarEnCadena(&tamanioArchivo);
			printf("Codigo del programa: %s .\n", codeScript);
			printf("Tamanio del archivo: %d .\n", tamanioArchivo);

			string_append(&codeScript,"\0");// "\0" para terminar el string
			int programCodeLen = tamanioArchivo + 1; //+1 por el '\0'

			//1)Envia el tamanio y el fromProcess
			sendMessage(&socketNucleo, &tamanioArchivo, sizeof(int));
			sendMessage(&socketNucleo, &fromProcess, sizeof(int));

			//2)Envia el codigo del programa
			exitCode = sendMessage(&socketNucleo, codeScript,programCodeLen);

			fgets(inputTeclado, sizeof(inputTeclado), stdin);

			break;
		}
		case 2: {
			printf("Comando Reconocido.\n");
			//Envia el tamanio y el fromProcess solamente porque el programa no es necesario para finalizar
			sendMessage(&socketNucleo, &tamanioArchivo, sizeof(int));
			sendMessage(&socketNucleo, &fromProcess, sizeof(int));
			break;
		}
		case 3: {
			printf("Comando Reconocido.\n");
			exit(-1);
			break;
		}
		default:
			log_warning(logConsola,"Comando invalido, inténtelo nuevamente.\n");
			break;
		}
		exitCode = reconocerOperacion();
	}
	return exitCode;
}

int reconocerComando(char* comando) {
	if (!strcmp("ejecutar\n", comando)) {
		return 1;
	}else if (!strcmp("finalizar\n", comando)){
		return 2;
	}else if (!strcmp("salir\n", comando)) {
		return 3;
	}
	return -1;
}

void* leerArchivoYGuardarEnCadena(int* tamanioDeArchivo) {
	FILE* archivo=NULL;

	int descriptorArchivo;
	printf("Ingrese archivo a ejecutar.\n");
	char nombreDelArchivo[100];
	scanf("%s", nombreDelArchivo);
	archivo = fopen(nombreDelArchivo, "r");
	descriptorArchivo=fileno(archivo);
	lseek(descriptorArchivo,0,SEEK_END);
	*tamanioDeArchivo=ftell(archivo);
	char* textoDeArchivo=malloc(*tamanioDeArchivo);
	lseek(descriptorArchivo,0,SEEK_SET);
	if (archivo == NULL) {
		log_error(logConsola,"Error al abrir el archivo.\n");
		printf("Error al abrir el archivo.\n");
	} else {
		size_t count = 1;
		fread(textoDeArchivo,*tamanioDeArchivo,count,archivo);
		//log_info(logConsola,"Tamanio adentro de la funcion: %i\n",*tamanioDeArchivo);
	}
	fclose(archivo);
	return textoDeArchivo;
}

int connectTo(enum_processes processToConnect, int *socketClient){
	int exitcode = EXIT_FAILURE;//DEFAULT VALUE
	int port = 0;
	char *ip = string_new();

	switch (processToConnect){
	case NUCLEO:{
		string_append(&ip,configConsola.ip_Nucleo);
		port= configConsola.port_Nucleo;
		break;
	}
	default:{
		 log_info(logConsola,"Process '%s' NOT VALID to be connected by UMC.\n", getProcessString(processToConnect));
		break;
	}
	}
	exitcode = openClientConnection(ip, port, socketClient);

	//If exitCode == 0 the client could connect to the server
	if (exitcode == EXIT_SUCCESS){

		// ***1) Send handshake
		exitcode = sendClientHandShake(socketClient,CONSOLA);

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
					log_info(logConsola, "Conectado a NUCLEO - Messsage: %s\n",	message->message);
					break;
				}
				default:{
					log_error(logConsola,"Process couldn't connect to SERVER - Not able to connect to server %s. Please check if it's down.\n",ip);
					close(*socketClient);
					break;
				}
				}
			}else if (receivedBytes == 0 ){
				//The client is down when bytes received are 0
				log_error(logConsola,
						"The client went down while receiving! - Please check the client '%d' is down!\n",*socketClient);
				close(*socketClient);
			}else{
				log_error(logConsola,
						"Error - No able to received - Error receiving from socket '%d', with error: %d\n",*socketClient, errno);
				close(*socketClient);
			}
		}

	}else{
		log_error(logConsola, "I'm not able to connect to the server! - My socket is: '%d'\n", *socketClient);
		close(*socketClient);
	}

	return exitcode;
}

void crearArchivoDeConfiguracion(char *configFile){
	t_config* configuration;
	configuration = config_create(configFile);
	configConsola.port_Nucleo = config_get_int_value(configuration,"PUERTO_NUCLEO");
	configConsola.ip_Nucleo= config_get_string_value(configuration,"IP_NUCLEO");

}

int reconocerOperacion() {
	int* tamanio = malloc(sizeof(int));
	int operacion = -1;
	int exitCode = EXIT_FAILURE;
	exitCode = receiveMessage(&socketNucleo, &operacion, sizeof(int));
	switch (operacion) {
	case 1: {	//Recibo del Nucleo el tamanio y el texto a imprimir
		exitCode = receiveMessage(&socketNucleo, tamanio, sizeof(int));
		char* textoImprimir = malloc(*tamanio);
		exitCode = receiveMessage(&socketNucleo, textoImprimir,sizeof(*tamanio));
		log_info(logConsola, "Texto: %s", textoImprimir);
		free(textoImprimir);
		break;
	}
	case 2: {	//Recibo del Nucleo el valor a mostrar
		t_valor_variable* valor = malloc(sizeof(t_valor_variable));
		exitCode = receiveMessage(&socketNucleo, valor,sizeof(t_valor_variable));
		log_info(logConsola, "Valor Recibido:%i", valor);
		free(valor);
		break;
	}
	case 3: {//Recibo del Nucleo el tamanio y el texto a imprimir, y luego finalizo proceso.
		exitCode = receiveMessage(&socketNucleo, tamanio, sizeof(int));
		char* textoImprimir = malloc(*tamanio);
		exitCode = receiveMessage(&socketNucleo, textoImprimir,sizeof(*tamanio));
		log_info(logConsola, "Texto: %s", textoImprimir);
		free(textoImprimir);
		exit(0);	//EXIT_SUCCESS
		break;
	}
	default: {
		log_error(logConsola, "No se pudo recibir ninguna operacion valida");
		break;
	}
	}
	free(tamanio);
	return exitCode;
}
