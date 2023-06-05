#ifndef CONSOLA_H
#define CONSOLA_H
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
#include "instrucciones.h"

	// rutaArchivoConfiguracion = ;
	// rutaInstrucciones = "./pseudocodigo/pseudocodigo";


int conexion;


int contadorIdPCB;
int conexionMemoria;

t_config *config;

int contadorIdTablaPag;

char *rutaArchivoConfiguracion;

char *rutaInstrucciones;

typedef struct
{
	char *ipKernel;
	char *puertoKernel;
	char **segmentos;
	int tiempoPantalla;
} t_configConsola;

t_configConsola configConsola;

t_configKernel configKernel;

/*******Funcion que permite leer la configuracion del puerrto y la ip del kernel*******/
t_configConsola extraerDatosConfig(t_config * ruta);


/*******Funcion que recibe y valida los argumentos que se ingresan cuando se inicia el modulo *******/
void obtenerArgumentos(int, char **);

FILE *abrirArchivo(char *);


/**Funcion que crea la estructura que sera enviada**/
t_informacion* crearInformacion(); 

t_paquete* crear_paquete_programa(t_informacion* informacion);

void liberar_programa(t_informacion* informacion);

t_list* listaSegmentos();

char *recibirMensaje(int socket);
void *recibirStream(int socket, size_t stream_size);
void conectar_kernel();
/*

██████╗ 
╚════██╗
  ▄███╔╝
  ▀▀══╝ 
  ██╗   
  ╚═╝   
        
*/

int conexionDispatch;
int conexionConsola;
int conexionInterrupt;
// LISTAS
t_list *LISTA_NEW;
t_list *LISTA_READY;
t_list *LISTA_EXEC;
t_list *LISTA_BLOCKED;
t_list *LISTA_BLOCKED_PANTALLA;
t_list *LISTA_BLOCKED_TECLADO;
t_list *LISTA_EXIT;
t_list *LISTA_SOCKETS;
t_list *LISTA_READY_AUXILIAR;
t_list *LISTA_BLOCKED_DISCO;
t_list *LISTA_BLOCKED_IMPRESORA;
t_list *LISTA_TABLA_PAGINAS;
t_list *LISTA_BLOCK_PAGE_FAULT;
t_list *LISTA_INICIO_TABLA_PAGINA;
t_list *LISTA_BITMAP_MARCO;
t_list *LISTA_INFO_MARCO;
t_list *LISTA_MARCOS_POR_PROCESOS;
t_list *LISTA_BLOCKED_WIFI;
t_list *LISTA_BLOCKED_USB;
t_list *LISTA_BLOCKED_AUDIO;
t_list *LISTA_BLOCKED_GENERAL;

// MUTEX
pthread_mutex_t mutex_creacion_ID;
pthread_mutex_t mutex_ID_Segmnento;
pthread_mutex_t mutex_lista_new;
pthread_mutex_t mutex_lista_ready;
pthread_mutex_t mutex_lista_exec;
pthread_mutex_t mutex_lista_blocked_disco;
pthread_mutex_t mutex_lista_blocked_impresora;
pthread_mutex_t mutex_lista_blocked_pantalla;
pthread_mutex_t mutex_conexion_memoria;
pthread_mutex_t mutex_lista_blocked_audio;
pthread_mutex_t mutex_lista_blocked_wifi;
pthread_mutex_t mutex_lista_blocked_usb;
pthread_mutex_t mutex_lista_blocked;
pthread_mutex_t mutex_lista_exit;
pthread_mutex_t mutex_lista_ready_auxiliar;
pthread_mutex_t mutex_creacion_ID_tabla;
pthread_mutex_t mutex_lista_tabla_paginas;
pthread_mutex_t mutex_lista_block_page_fault ;
pthread_mutex_t mutex_lista_marco_por_proceso;
pthread_mutex_t mutex_lista_pagina_marco_por_proceso;
pthread_mutex_t mutex_lista_tabla_paginas_pagina;
pthread_mutex_t mutex_lista_marcos_por_proceso_pagina;
pthread_mutex_t mutex_lista_blockeados_por_dispositivo;

// SEMAFOROS
sem_t sem_planif_largo_plazo;
sem_t contador_multiprogramacion;
sem_t contador_pcb_running;
sem_t contador_bloqueo_teclado_running;
sem_t contador_bloqueo_pantalla_running;
sem_t contador_bloqueo_disco_running;
sem_t contador_bloqueo_impresora_running;
sem_t contador_bloqueo_wifi_running;
sem_t contador_bloqueo_usb_running;
sem_t contador_bloqueo_audio_running;
sem_t sem_ready;
sem_t sem_bloqueo;
sem_t sem_procesador;
sem_t sem_agregar_pcb;
sem_t sem_eliminar_pcb;
sem_t sem_hay_pcb_lista_new;
sem_t sem_hay_pcb_lista_ready;
sem_t sem_pasar_pcb_running;
sem_t sem_timer;
sem_t sem_desalojar_pcb;
sem_t sem_kill_trhread;
sem_t sem_llamar_feedback;

bool hayTimer;
#endif