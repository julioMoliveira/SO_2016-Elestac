#ifndef CPU_H_
#define CPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <parser/metadata_program.h>
#include <parser/parser.h>
#include <commons/collections/list.h>
#include "commons/config.h"
#include "common-types.h"
#include "sockets.h"

typedef struct {
	int port_Nucleo;
	char *ip_Nucleo;
	int port_UMC;
	char *ip_UMC;
	int pageSize;
} t_configFile;

t_configFile configuration;

AnSISOP_funciones* funciones;
AnSISOP_kernel* funciones_kernel;
int frameSize = 0;

int ejecutarPrograma(t_PCB *PCB);
int connectTo(enum_processes processToConnect);
void crearArchivoDeConfiguracion(char *configFile);
void sendRequestToUMC();
void waitRequestFromNucleo();

#endif /* CPU_H_ */
