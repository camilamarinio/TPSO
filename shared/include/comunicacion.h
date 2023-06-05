#ifndef COMUNICACION_H_
#define COMUNICACION_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include "utils.h"
#include "../globals.h"

#define ERROR -1
#define SUCCESS 0


/*

███████╗███╗   ██╗██╗   ██╗██╗ ██████╗ 
██╔════╝████╗  ██║██║   ██║██║██╔═══██╗
█████╗  ██╔██╗ ██║██║   ██║██║██║   ██║
██╔══╝  ██║╚██╗██║╚██╗ ██╔╝██║██║   ██║
███████╗██║ ╚████║ ╚████╔╝ ██║╚██████╔╝
╚══════╝╚═╝  ╚═══╝  ╚═══╝  ╚═╝ ╚═════╝ 
*/


/*
/ @brief Envia un stream a traves de un socket.
/ @return Codigo de resultado
*/
int enviarStream(int, void *, size_t);

/*
/ @brief Envia un numero a traves de un socket.
/ @return Codigo de resultado
*/
int enviarNumero(int socket,uint32_t num);

/*
/ @brief Envia un mensaje a traves de n socket
/ @return Codigo de resultado
*/
int enviarMensaje(int, char *);

void enviarMsje(int socket, t_enviadoPor unModulo, void* mensaje, int tamanioMensaje, t_tipoMensaje tipoMensaje);

void enviarPaquete(int socket, t_paqt* paquete);

void enviarResultado(int socket, char* mensaje);



void *serializarMensaje(char *msj, size_t *size_stream);

/*

██████╗ ███████╗ ██████╗██╗██████╗ ██╗██████╗ 
██╔══██╗██╔════╝██╔════╝██║██╔══██╗██║██╔══██╗
██████╔╝█████╗  ██║     ██║██████╔╝██║██████╔╝
██╔══██╗██╔══╝  ██║     ██║██╔══██╗██║██╔══██╗
██║  ██║███████╗╚██████╗██║██████╔╝██║██║  ██║
╚═╝  ╚═╝╚══════╝ ╚═════╝╚═╝╚═════╝ ╚═╝╚═╝  ╚═╝
                                              

*/



/*
/ @brief Recibe un stream de datos
/ @return Puntero a un ese stream de datos, o NULL si falla.
*/
void *recibirStream(int, size_t);

/*
/ @brief Recibe un mensaje de caracteres.
/ @return Un puntero a un mensaje, o NULL si falla
*/
char *recibirMensaje(int);

/*
/ @brief Recibe un numero por una conexion
/ @return El numero, o NULL si falla.
*/

void recibirMsje(int socket, t_paqt * paquete);

int recibirDatos(void* paquete, int socket, uint32_t cantARecibir);


uint32_t recibirNumero(int socket);

#endif
