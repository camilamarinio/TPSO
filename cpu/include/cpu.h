#ifndef CPU_H
#define CPU_H
#include <stdlib.h>
#include <stdio.h>
#include <commons/log.h>
#include <stdbool.h>
#include "client.h"
#include "server.h"
#include "comunicacion.h"
// #include <commons/collections/list.h>

t_config *config;
int conexion;
int conexionMemoria;

int contadorIdPCB;
typedef struct
{
	int entradasTLB;
	char *reemplazoTLB;
	int retardoInstruccion;
	char *ipMemoria;
	char *puertoMemoria;
	char *puertoEscuchaDispatch;
	char *puertoEscuchaInterrupt;
	int cantidadEntradasPorTabla;
	int tamanioPagina;
	char *ipCPU;

} t_configCPU;

t_configCPU configCPU;

int socketMemoria;

t_configKernel configKernel;

bool interrupciones;
bool retornePCB;

int socketAceptadoDispatch;

t_configCPU extraerDatosConfig(t_config *);
void recibir_config_memoria();
void iniciar_servidor_dispatch();
void iniciar_servidor_interrupt();
void conectar_memoria();
char *registroToString(t_registro);
char *instruccionToString(t_instCode);
char *ioToString(t_IO);
uint32_t matchearRegistro(t_registros, t_registro);
void asignarValorARegistro(t_pcb *, t_registro, uint32_t);
bool cicloInstruccion(t_pcb *);
t_direccionFisica *calcular_direccion_fisica(int direccionLogica, int cant_entradas_por_tabla, int tam_pagina, t_pcb *pcb);
void validarSegmentationFault(t_pcb *pcb, int desplazamientoSegmento, int indice);
t_direccionFisica *traduccion_de_direccion(int direccionLogica, int cant_entradas_por_tabla, int tam_pagina, t_pcb *pcb);
int tamanioMaximoPorSegmento(int cant_entradas_por_tabla, int tam_pagina);
int numeroDeSegmento(int dir_logica, int tam_max_segmento);
int desplazamientoSegmento(int dir_logica, int tam_max_segmento);
int numeroPagina(int desplazamiento_segmento, int tam_pagina);
int desplazamientoPagina(int desplamiento_segmento, int tam_pagina);
int primer_acceso(int numero_pagina, uint32_t indiceTablaPaginas, uint32_t pid);

int conexionDispatch;
int conexionConsola;
int conexionInterrupt;
int contadorIdTablaPag;

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
pthread_mutex_t mutex_creacion_ID_tabla;
pthread_mutex_t mutex_lista_tabla_paginas;
pthread_mutex_t mutex_lista_block_page_fault;
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

pthread_mutex_t mutex_lista_ready_auxiliar;
sem_t sem_llamar_feedback;

bool hayTimer;

/**********TLB***********/
typedef struct
{
	int nroPagina;
	int nroFrame;
	int nroSegmento;
	int pid;
	int ultimaReferencia;
	int instanteDeCarga; // Instante de carga en memoria
} entrada_tlb;

typedef struct tlb
{
	t_list *entradas;
	int cant_entradas; // cantidad de entradas
	char *algoritmo;
} tlb;

tlb *TLB;
int habilitarTLB;
pthread_mutex_t mutexTLB;

int obtenerMomentoActual();
void inicializarTLB();
void actualizar_TLB(int nroPagina, int nroFrame, int nroSegmento, int pid);
int buscar_en_TLB(int nroPagina, int nroSegmento, int pid);
void limpiar_entrada_TLB(int nroPagina, int pid);
void limpiar_entradas_TLB();
void reemplazo_algoritmo_fifo(int nroPagina, int nroFrame, int nroSegmento, int pid);
void reemplazo_algoritmo_lru(int nroPagina, int nroFrame, int nroSegmento, int pid);
void cerrar_TLB();
void destruir_entrada(void *entry);
void imprimirModificacionTlb();
void usarAlgoritmosDeReemplazoTlb(int nroPagina, int nroFrame, int nroSegmento, int pid);
char *calcularHorasMinutosSegundos(int valor);
int entradaConMenorTiempoDeReferencia();
int entradaConMenorInstanteDeCarga();
void *esMenor(void *_unaEntrada, void *_otraEntrada);
bool coincideMarcoYpid(void *_entrada);
#endif