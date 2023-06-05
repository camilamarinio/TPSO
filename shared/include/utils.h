#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>
#include <semaphore.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/collections/list.h>
#include <assert.h>
#include "../globals.h"

typedef enum
{
	MENSAJE,
	PAQUETE,
	NEW,
	PROGRAMA
} op_code;

typedef enum
{
	SET,
	ADD,
	MOV_IN,
	MOV_OUT,
	IO,
	EXIT
} t_instCode;

typedef enum
{
	AX,
	BX,
	CX,
	DX,
} t_registro;

typedef enum
{
	DISCO,
	TECLADO,
	PANTALLA,
	IMPRESORA,
	WIFI,
	USB,
	AUDIO,
} t_IO;

typedef struct
{
	uint32_t nroSegmento;
	uint32_t nroPagina;
	int desplazamiento;
} t_direccion_logica;

typedef struct
{
	uint32_t nroMarco;
	int tamPagina;
	int desplazamientoPagina;
	t_direccion_logica dl;
} t_direccionFisica;

typedef struct {
	int idTablaDePaginas;
	int pagina;
	int pid;
}MSJ_MEMORIA_CPU_ACCESO_TABLA_DE_PAGINAS;

typedef struct {
	int nro_segmento;
	int nro_pagina;
	//t_pcb* pcb;
}MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT;

typedef struct {
	int nroMarco;
	int desplazamiento;
	int pid;
} MSJ_MEMORIA_CPU_LEER;

typedef struct {
	int nroMarco;
	int desplazamiento;
	uint32_t valorAEscribir;
	int pid;
}MSJ_MEMORIA_CPU_ESCRIBIR;

typedef struct
{
	int numero;
} MSJ_INT;

typedef struct
{
	char *cadena;
} MSJ_STRING;

// Utils del cliente
typedef struct
{
	uint32_t size; // Tamaño del payload
	void *stream;  // Payload
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer *buffer;
} t_paquete;

typedef struct
{
	uint8_t codigo_operacion;
	t_buffer *buffer;
} t_paqueteActual;

typedef struct
{
	t_instCode instCode;
	uint32_t paramInt;
	char* paramIO;
	uint32_t sizeParamIO;
	t_registro paramReg[2];
} __attribute__((packed)) t_instruccion;

typedef struct
{
	t_list *instrucciones;
	uint32_t instrucciones_size;
	t_list *segmentos;
	uint32_t segmentos_size;
} __attribute__((packed)) t_informacion;

typedef struct

{
	uint32_t AX;
	uint32_t BX;
	uint32_t CX;
	uint32_t DX;

} __attribute__((packed)) t_registros;

typedef struct
{
	uint8_t id;
	uint32_t tamanio;
	uint32_t indiceTablaPaginas;
} __attribute__((packed)) t_tabla_segmentos;

int size_char_array(char **);

extern int conexionMemoria;
extern int conexionDispatch;
extern int conexionInterrupt;
extern int conexionConsola;
extern int contadorIdTablaPag;

typedef struct
{
	uint32_t id;
	// uint32_t tamanio;
	uint32_t program_counter;
	// uint32_t tablaPag; // definir con memoria
	// double ejecutados_total;
	t_informacion *informacion;
	t_list *tablaSegmentos;
	uint32_t segmentos_size;
	t_registros registros;
	int socket;

} t_pcb;

enum tipo_mensaje
{
	PCB
};

typedef enum
{
	FIFO,
	RR,
	FEEDBACK
} t_tipo_algoritmo;

extern int contadorIdPCB;
typedef enum {
	INSTRUCCIONES,    				//entre consola-kernel
	TERMINAR_CONSOLA,				//entre consola-kernel
	DISPATCH_PCB,     				//entre kernel-cpu
	BLOCK_PCB_IO_TECLADO,			//entre kernel-cpu
	BLOCK_PCB_IO_PANTALLA,			//entre kernel-cpu
	BLOCK_PCB_IO,					//entre kernel-cpu
	BLOCK_PCB_PAGE_FAULT,			//entre kernel-cpu
	INTERRUPT_INTERRUPCION,			//entre kernel-cpu
	EXIT_PCB,						//entre kernel-cpu
	PASAR_A_READY,					//entre kernel-memoria
	SUSPENDER,						//entre kernel-memoria
	PASAR_A_EXIT,					//entre kernel-memoria
	CONFIG_DIR_LOG_A_FISICA,   	 	//entre cpu-memoria: ESTO ES PARA PASAR LA CONFIGURACION DE LAS DIRECCIONES, ES EN EL INIT DE LA CPU 
	ACCESO_MEMORIA_TABLA_DE_PAG,	//entre cpu-memoria: ESTO ES PARA ACCEDER A LA TABLA DE PAGINAS EN MEMORIA Y BUSCAR EL FRAME
	RESPUESTA_MEMORIA_MARCO_BUSCADO, //entre memoria-cpu: despues de buscar en la tabla de pagina y encontrar el marco buscado se lo retorna a cpu
	ACCESO_MEMORIA_LEER,			//entre cpu-memoria
	ACCESO_MEMORIA_ESCRIBIR,			//entre cpu-memoria
	HANDSHAKE_INICIAL,
	LIBERAR_RECURSOS,
	ASIGNAR_RECURSOS,
	SEGMENTATION_FAULT,				//entre cpu-kernel
	PAGE_FAULT, 					//entre memoria-cpu	
} t_tipoMensaje;

typedef enum
{
	CONSOLA,
	KERNEL,
	CPU,
	MEMORIA
} t_enviadoPor;

typedef enum
{
	CLOCK,
	CLOCK_MODIFICADO
} t_tipo_algoritmo_sustitucion;

typedef struct 
{
	int idPagina;
	int idSegmento;
	int PID;
}t_info_remplazo;

typedef struct
{
	t_tipoMensaje tipoMensaje;
	int tamanioMensaje;
	t_enviadoPor cliente;
} t_infoMensaje;

typedef struct
{
	t_infoMensaje header;
	void *mensaje;
} t_paqt;

typedef struct
{
	int cantEntradasPorTabla;
	int tamanioPagina;
} MSJ_MEMORIA_CPU_INIT;

typedef struct
{
	t_list* lista_block;
	pthread_mutex_t mutex_lista_blocked;
	sem_t contador_bloqueo;
	char* dispositivo;
	int tiempoEjecucion;

} t_dispositivo;

typedef struct 
{
	t_registro registro;
	char dispositivo;

} t_io_dispositivo;


typedef struct 
{
	uint32_t tiempo;
	char dispositivo;

} t_io_dispositivo_kernel;


void imprimirInstruccionesYSegmentos(t_informacion );


/*

 ██████╗██╗     ██╗███████╗███╗   ██╗████████╗███████╗
██╔════╝██║     ██║██╔════╝████╗  ██║╚══██╔══╝██╔════╝
██║     ██║     ██║█████╗  ██╔██╗ ██║   ██║   █████╗
██║     ██║     ██║██╔══╝  ██║╚██╗██║   ██║   ██╔══╝
╚██████╗███████╗██║███████╗██║ ╚████║   ██║   ███████╗
 ╚═════╝╚══════╝╚═╝╚══════╝╚═╝  ╚═══╝   ╚═╝   ╚══════╝

*/

int crear_conexion(char *ip, char *puerto);

void enviar_mensaje(char *mensaje, int socket_cliente);

t_paquete *crear_paquete(void);

t_paquete *crear_super_paquete(void);

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio);

void enviar_paquete(t_paquete *paquete, int socket_cliente);

void liberar_conexion(int socket_cliente);

void eliminar_paquete(t_paquete *paquete);

t_buffer *cargar_buffer_a_t_pcb(t_pcb t_pcb);

void cargar_buffer_a_paquete(t_buffer *buffer, int conexion);

t_pcb *deserializar_pcb(t_buffer *buffer);

void deserializar_paquete(int conexion);

void serializarPCB(int socket, t_pcb *pcb, t_tipoMensaje tipoMensaje);
void crearPaquete(t_buffer *buffer, t_tipoMensaje op, int unSocket);
t_paqueteActual *recibirPaquete(int socket);
t_pcb *deserializoPCB(t_buffer *buffer);
int calcularSizeInfo(t_informacion* );
/*
███████╗███████╗██████╗ ██╗   ██╗██╗██████╗  ██████╗ ██████╗
██╔════╝██╔════╝██╔══██╗██║   ██║██║██╔══██╗██╔═══██╗██╔══██╗
███████╗█████╗  ██████╔╝██║   ██║██║██║  ██║██║   ██║██████╔╝
╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██║██║  ██║██║   ██║██╔══██╗
███████║███████╗██║  ██║ ╚████╔╝ ██║██████╔╝╚██████╔╝██║  ██║
╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚═╝╚═════╝  ╚═════╝ ╚═╝  ╚═╝

*/

int iniciar_servidor(char *, char *);

int esperar_cliente(int);

t_list *recibir_paquete(int);

void recibir_mensaje(int);

int recibir_operacion(int);

void *recibir_buffer(int *, int);

/*

██████╗ ██╗      █████╗ ███╗   ██╗██╗███████╗██╗ ██████╗ █████╗  ██████╗██╗ ██████╗ ███╗   ██╗
██╔══██╗██║     ██╔══██╗████╗  ██║██║██╔════╝██║██╔════╝██╔══██╗██╔════╝██║██╔═══██╗████╗  ██║
██████╔╝██║     ███████║██╔██╗ ██║██║█████╗  ██║██║     ███████║██║     ██║██║   ██║██╔██╗ ██║
██╔═══╝ ██║     ██╔══██║██║╚██╗██║██║██╔══╝  ██║██║     ██╔══██║██║     ██║██║   ██║██║╚██╗██║
██║     ███████╗██║  ██║██║ ╚████║██║██║     ██║╚██████╗██║  ██║╚██████╗██║╚██████╔╝██║ ╚████║
╚═╝     ╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝     ╚═╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝


*/

typedef struct
{
	char *ipMemoria;
	char *puertoMemoria;
	char *ipCPU;
	char *puertoCPUDispatch;
	char *puertoCPUInterrupt;
	char *ipKernel;
	char *puertoEscucha;
	char *algoritmo;

	int gradoMultiprogramacion;
	char **dispositivosIO;
	char **tiemposIO;
	int quantum;
} t_configKernel;

extern t_configKernel configKernel;

extern t_log *logger;
extern t_log *loggerMinimo;


// extern t_log* loggerKernel;

// t_pcb* crear_pcb();

// LISTAS
extern t_list *LISTA_NEW;
extern t_list *LISTA_READY;
extern t_list *LISTA_EXEC;
extern t_list *LISTA_BLOCKED;
extern t_list *LISTA_EXIT;
extern t_list *LISTA_SOCKETS;
extern t_list *LISTA_READY_AUXILIAR;
extern t_list *LISTA_BLOCKED_PANTALLA;
extern t_list *LISTA_BLOCKED_TECLADO;
extern t_list *LISTA_BLOCKED_DISCO;
extern t_list *LISTA_BLOCKED_IMPRESORA;
extern t_list *LISTA_BLOCKED_GENERAL;
extern t_list *LISTA_TABLA_PAGINAS;
extern t_list *LISTA_BLOCK_PAGE_FAULT;
extern t_list *LISTA_INICIO_TABLA_PAGINA;
extern t_list *LISTA_BITMAP_MARCO;
extern t_list *LISTA_INFO_MARCO;
extern t_list *LISTA_MARCOS_POR_PROCESOS;
extern t_list *LISTA_BLOCKED_WIFI;
extern t_list *LISTA_BLOCKED_USB;
extern t_list *LISTA_BLOCKED_AUDIO;

// MUTEX
extern pthread_mutex_t mutex_creacion_ID;
extern pthread_mutex_t mutex_ID_Segmnento;
extern pthread_mutex_t mutex_lista_new;
extern pthread_mutex_t mutex_lista_ready;
extern pthread_mutex_t mutex_lista_exec;
extern pthread_mutex_t mutex_lista_blocked_disco;
extern pthread_mutex_t mutex_lista_blocked_impresora;
extern pthread_mutex_t mutex_lista_blocked_pantalla;
extern pthread_mutex_t mutex_conexion_memoria;
extern pthread_mutex_t mutex_lista_blocked_audio;
extern pthread_mutex_t mutex_lista_blocked_wifi;
extern pthread_mutex_t mutex_lista_blocked_usb;
extern pthread_mutex_t mutex_lista_blocked;
extern pthread_mutex_t mutex_lista_exit;
extern pthread_mutex_t mutex_lista_ready_auxiliar;
extern pthread_mutex_t mutex_creacion_ID_tabla;
extern pthread_mutex_t mutex_lista_tabla_paginas;
extern pthread_mutex_t mutex_lista_block_page_fault;
extern pthread_mutex_t mutex_lista_marco_por_proceso;
extern pthread_mutex_t mutex_lista_pagina_marco_por_proceso;
extern pthread_mutex_t mutex_lista_tabla_paginas_pagina;
extern pthread_mutex_t mutex_lista_marcos_por_proceso_pagina;
extern pthread_mutex_t mutex_lista_blockeados_por_dispositivo;


// SEMAFOROS
extern sem_t sem_planif_largo_plazo;
extern sem_t contador_multiprogramacion;
extern sem_t contador_pcb_running;
extern sem_t contador_bloqueo_teclado_running;
extern sem_t contador_bloqueo_pantalla_running;
extern sem_t contador_bloqueo_disco_running;
extern sem_t contador_bloqueo_impresora_running;
extern sem_t contador_bloqueo_wifi_running;
extern sem_t contador_bloqueo_usb_running;
extern sem_t contador_bloqueo_audio_running;
extern sem_t sem_ready;
extern sem_t sem_bloqueo;
extern sem_t sem_procesador;

extern sem_t sem_agregar_pcb;
extern sem_t sem_eliminar_pcb;
extern sem_t sem_hay_pcb_lista_new;
extern sem_t sem_hay_pcb_lista_ready;
extern sem_t sem_pasar_pcb_running;

extern sem_t sem_timer;
extern sem_t sem_desalojar_pcb;
extern sem_t sem_kill_trhread;

extern sem_t sem_llamar_feedback;

void *serializar_paquete_dos(t_paqueteActual *paquete, int bytes);

extern bool hayTimer;
#endif /* UTILS_H_ */