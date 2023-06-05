#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>
#include <pthread.h>
#include "utils.h"
#include "../globals.h"



void conectar_y_mostrar_mensajes_de_cliente(char*, char*, t_log*);
void mostrar_mensajes_del_cliente(int);
void crear_hilos(int );
void iterator(char* );
void iteratorInt(int );
void cargarListaReadyIdPCB(t_list*);
//t_pcb *crear_pcb(t_informacion* , int );
#define IP_SERVER "0.0.0.0"

void cambiaValor();



t_pcb* algoritmo_fifo(t_list *);
t_pcb *algoritmo_rr(t_list *);
t_tipo_algoritmo obtenerAlgoritmo();
t_informacion recibir_informacion(int cliente_fd);
void implementar_fifo();
void implementar_rr();
void implementar_feedback();
void implementar_fifo_auxiliar();
void hilo_timer();
void pasar_a_new(t_pcb *);
void pasar_a_ready(t_pcb *);
void pasar_a_ready_auxiliar(t_pcb *);
void pasar_a_exec(t_pcb *);
void pasar_a_block_impresora(t_pcb *);
void pasar_a_block_disco(t_pcb *);
void pasar_a_block_pantalla(t_pcb *);
void pasar_a_block_teclado(t_pcb *);
void pasar_a_block_wifi(t_pcb *);
void pasar_a_block_audio(t_pcb *);
void pasar_a_block_usb(t_pcb *);
void pasar_a_exit(t_pcb *);
void pasar_a_block_page_fault(t_pcb *);
uint32_t* deserializarValor(t_buffer*, int);
void serializarValor(uint32_t , int ,t_tipoMensaje);


#endif /* SERVER_H_ */
