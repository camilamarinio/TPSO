#ifndef INSTRUCCIONES_H
#define INSTRUCCIONES_H
#include <stdlib.h>
#include <stdio.h>
#include <commons/log.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/collections/list.h>
#include <assert.h>
#include "client.h"
#include "server.h"




t_registro devolverRegistro(char *);

void agregarInstruccionesDesdeArchivo(FILE *instructionsFile, t_list *instrucciones);


#endif