#ifndef MEMORIA_H
#define MEMORIA_H
#include <stdlib.h>
#include <stdio.h>
#include <commons/log.h>
#include <stdbool.h>
#include <string.h>
#include "client.h"
#include "server.h"
#include "comunicacion.h"

t_config *config;

int socketAceptadoCPU;
typedef struct
{
	char *puertoEscuchaUno;
	char *puertoEscuchaDos;
	int tamMemoria;
	int tamPagina;
	int entradasPorTabla;
	int retardoMemoria;
	char *algoritmoReemplazo;
	int marcosPorProceso;
	int retardoSwap;
	char *pathSwap;
	int tamanioSwap;
} t_configMemoria;

t_configMemoria configMemoria;
typedef struct
{
	uint32_t comienzo;
	uint32_t idPCB;
} t_inicioTablaXPCB;

// t_inicioTablaXPCB inicioTablaXPCB;
t_list *LISTA_INICIO_TABLA_PAGINA;

typedef struct
{
	uint32_t idTablaPag; //esto es tambien el nro de segmento
	uint32_t idPCB;
	t_list *paginas;
} __attribute__((packed)) t_tabla_paginas;

typedef struct
{   int nroSegmento;
	int nroPagina;
	int nroMarco;
	uint8_t presencia;	  // 0 v 1
	uint8_t modificacion; // 0 v 1
	uint8_t uso;		  // 0 v 1
	uint32_t posicionSwap;
} __attribute__((packed)) t_pagina;


int tamanio;


typedef struct {
	int nroMarco;
	int uso;
} __attribute__((packed)) t_bitmap_marcos_libres;

typedef enum {
	NO_OCUPADO, // 0
	OCUPADO // 1
}t_estadoMarco;

typedef struct {
	int pidProceso;
	int nroPag;
	int nroMarco;
	t_estadoMarco estado;
	//void* frameVoid;
}t_filaMarco;

t_list* tablaDeMarcos;	

size_t tamanioSgtePagina;

typedef struct {
	int idPCB;
	int marcoSiguiente;
	t_list* paginas;
}__attribute__((packed)) t_marcos_por_proceso;

//bitmap_marcos_libres bitmap_marco[];


void *memoriaRAM; // espacio que en el que voy a guardar bytes, escribir y leer como hago en el archivo (RAM)
FILE *swap;



t_configKernel configKernel;
int contadorIdTablaPag;
int buscar_marco_vacio();
void inicializar_bitmap();
void conexionCPU(int socketAceptadoVoid);
void iniciar_servidor_hacia_kernel();
void iniciar_servidor_hacia_cpu();
void agregar_tabla_paginas(t_tabla_paginas *);
t_configMemoria extraerDatosConfig(t_config *);
void crearTablasPaginas(void *pcb);
void eliminarTablasPaginas(void *pcb);
FILE *abrirArchivo(char *filename);
void crear_hilos_memoria();
bool esta_vacio_el_archivo(FILE *);
void* conseguir_puntero_a_base_memoria(int , void *);
void* conseguir_puntero_al_desplazamiento_memoria(int , void *, int );
t_marcos_por_proceso *buscarMarcosPorProcesosAccesos(int );
void algoritmo_reemplazo_clock(t_info_remplazo *);
void algoritmo_reemplazo_clock_modificado(t_info_remplazo *);
void asignarPaginaAMarco(t_marcos_por_proceso*, t_info_remplazo*);
t_tipo_algoritmo_sustitucion obtenerAlgoritmoSustitucion();
void agregar_marco_por_proceso(t_marcos_por_proceso * );
void algoritmo_reemplazo_clock_modificado(t_info_remplazo *);
void asignacionDeMarcos(t_info_remplazo * , t_marcos_por_proceso *);
//t_list* filtrarPorPID(int );
bool chequearCantidadMarcosPorProceso(t_info_remplazo *);
t_info_remplazo* declararInfoReemplazo();
t_list *filtrarPorPIDTabla(int );
void incrementarMarcoSiguiente(t_marcos_por_proceso *);
void agregar_pagina_a_lista_de_paginas_marcos_por_proceso(t_marcos_por_proceso *, t_pagina *);
void primer_recorrido_paginas_clock(t_marcos_por_proceso *, t_info_remplazo *);
t_pagina *buscarPagina (t_info_remplazo *);
int buscarSegmento(t_info_remplazo *);
void filtrarYEliminarMarcoPorPIDTabla(int );
void eliminarEstructura(t_marcos_por_proceso *);
void imprimirMarcosPorProceso();
void accesoMemoriaTP(int, int, int,int);
void escribirEnSwap(int , int );
void leerEnSwap(int , int );
				
void accesoMemoriaLeer(t_direccionFisica* df, int pid, int socketAceptado);
t_pagina *buscarMarcoSegun(t_marcos_por_proceso*,t_info_remplazo*,int, int);
void accesoMemoriaEscribir(t_direccionFisica* dirFisica, uint32_t valorAEscribir, int pid, int socketAceptado);
t_marcos_por_proceso* buscarMarcosPorProcesos(t_info_remplazo* );
int recorrer_marcos(int );

int contadorIdPCB;
int socketAceptadoKernel;
int conexionMemoria;
int conexionDispatch;
int conexionConsola;
int conexionInterrupt;

// el segmento esta en utils

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
t_list *LISTA_BLOCKED_GENERAL;
t_list *LISTA_TABLA_PAGINAS;
t_list *LISTA_BLOCK_PAGE_FAULT;
t_list *LISTA_BITMAP_MARCO;
t_list *LISTA_INFO_MARCO;
t_list *LISTA_MARCOS_POR_PROCESOS;
t_list *LISTA_BLOCKED_WIFI;
t_list *LISTA_BLOCKED_USB;
t_list *LISTA_BLOCKED_AUDIO;

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
pthread_mutex_t mutex_creacion_ID_tabla;
pthread_mutex_t mutex_lista_tabla_paginas;
pthread_mutex_t mutex_lista_block_page_fault;
pthread_mutex_t mutex_lista_marco_por_proceso;
pthread_mutex_t mutex_lista_pagina_marco_por_proceso;
pthread_mutex_t mutex_lista_tabla_paginas_pagina;
pthread_mutex_t mutex_void_memoria_ram;
pthread_mutex_t mutex_lista_frames;
pthread_mutex_t mutex_swap;
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
sem_t contador_marcos_por_proceso;

sem_t sem_agregar_pcb;
sem_t sem_eliminar_pcb;
sem_t sem_hay_pcb_lista_new;
sem_t sem_hay_pcb_lista_ready;
sem_t sem_pasar_pcb_running;
sem_t sem_timer;
sem_t sem_desalojar_pcb;
sem_t sem_kill_trhread;

pthread_mutex_t mutex_lista_ready_auxiliar;
sem_t sem_llamar_feedback;

bool hayTimer;
#endif