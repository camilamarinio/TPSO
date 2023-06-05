#include "server.h"

void conectar_y_mostrar_mensajes_de_cliente(char *IP, char *PUERTO, t_log *logger)
{

	int server_fd = iniciar_servidor(IP, PUERTO); // socket(), bind() listen()
	log_info(logger, "Servidor listo para recibir al cliente");

	crear_hilos(server_fd);
}

void crear_hilos(int server_fd)
{
	while (1)
	{

		// esto se podria cambiar como int* cliente_fd= malloc(sizeof(int)); si lo ponemos, va antes del while
		int cliente_fd = esperar_cliente(server_fd);
		// aca hay un log que dice que se conecto un cliente
		log_info(logger, "Consola conectada, paso a crear el hilo");

		pthread_t thr1;

		pthread_create(&thr1, NULL, (void *)mostrar_mensajes_del_cliente, cliente_fd);

		pthread_detach(&thr1);
	}
	// return EXIT_SUCCESS;
}

void mostrar_mensajes_del_cliente(int cliente_fd)
{
	t_list *lista;
	int y = 1;
	while (y)
	{
		int cod_op = recibir_operacion(cliente_fd);

		switch (cod_op)
		{
		case MENSAJE:
			recibir_mensaje(cliente_fd);

			break;
		case PAQUETE:
			lista = recibir_paquete(cliente_fd);
			log_info(logger, "Me llegaron los siguientes valores:");
			list_iterate(lista, (void *)iterator);
			break;
		case NEW:
			log_info(logger, "Llegaron las instrucciones y los segmentos");

			t_informacion info = recibir_informacion(cliente_fd);

			enviarResultado(cliente_fd, "Quedate tranqui Consola, llego todo lo que mandaste ;)\n");

			// aca deberia hacer que la consola se quede esperando

			// t_pcb *pcb = crear_pcb(&info, cliente_fd);

			// pasar_a_new(pcb);
			// log_debug(logger, "Estado Actual: NEW , proceso id: %d", pcb->id);

			printf("Cant de elementos de new: %d\n", list_size(LISTA_NEW));

			sem_post(&sem_hay_pcb_lista_new);
			sem_post(&sem_planif_largo_plazo);
			sem_post(&sem_agregar_pcb);

			break;

		case -1:
			// liberar_conexion(cliente_fd); //esto lo va a mandar kernel cuando lo necesite
			// log_error(logger, "el cliente se desconecto. Terminando servidor");
			y = 1;
			break;
		default:
			log_warning(logger, "Operacion desconocida. No quieras meter la pata");
			break;
		}
	}
}

t_informacion recibir_informacion(cliente_fd)
{
	int size;
	void *buffer = recibir_buffer(&size, cliente_fd);
	t_informacion programa;
	int offset = 0;

	memcpy(&(programa.instrucciones_size), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(&(programa.segmentos_size), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	programa.instrucciones = list_create();
	t_instruccion *instruccion;

	programa.segmentos = list_create();
	uint32_t segmento;

	int k = 0;
	int l = 0;

	while (k < (programa.instrucciones_size))
	{
		instruccion = malloc(sizeof(t_instruccion));

		memcpy(&instruccion->instCode, buffer + offset, sizeof(t_instCode));
		offset += sizeof(t_instCode);
		memcpy(&instruccion->paramInt, buffer + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t);
		memcpy(&instruccion->sizeParamIO, buffer + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t);
		instruccion->paramIO = malloc(instruccion->sizeParamIO);
		memcpy(instruccion->paramIO, buffer + offset, instruccion->sizeParamIO);
		offset += instruccion->sizeParamIO;
		memcpy(&instruccion->paramReg[0], buffer + offset, sizeof(t_registro));
		offset += sizeof(t_registro);
		memcpy(&instruccion->paramReg[1], buffer + offset, sizeof(t_registro));
		offset += sizeof(t_registro);

		list_add(programa.instrucciones, instruccion);
		k++;
	}

	while (l < (programa.segmentos_size))
	{
		// segmento = malloc(sizeof(uint32_t));
		memcpy(&segmento, buffer + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t);
		list_add(programa.segmentos, segmento);
		l++;
	}

	// imprimirInstruccionesYSegmentos(programa);
	free(buffer);

	return programa;
}

void iterator(char *value)
{

	log_info(logger, "%s", value);
}

void pasar_a_new(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_new);
	list_add(LISTA_NEW, pcb);
	pthread_mutex_unlock(&mutex_lista_new);
	printf("\ninstrucciones en new\n");
	// imprimirInstruccionesYSegmentos(*(pcb->informacion));

	printf("\n imprimo todas las intrucciones y segmentos de todos los pcb en lista new\n");
	for (size_t i = 0; i < list_size(LISTA_NEW); i++)
	{
		t_pcb *pcb = list_get(LISTA_NEW, i);
		printf("\nposicion pcb %d\n tamanio segmentos %d", i, list_size(pcb->informacion->segmentos));
		// imprimirInstruccionesYSegmentos(*(pcb->informacion));
	}

	log_debug(logger, "Paso a NEW el proceso %d", pcb->id);
}

void pasar_a_ready(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_ready);
	list_add(LISTA_READY, pcb);
	pthread_mutex_unlock(&mutex_lista_ready);
	//printf("\ninstrucciones en ready\n");
	// imprimirInstruccionesYSegmentos(*(pcb->informacion));
	log_debug(logger, "Paso a READY el proceso %d", pcb->id);
}

void pasar_a_ready_auxiliar(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_ready_auxiliar);
	list_add(LISTA_READY_AUXILIAR, pcb);
	pthread_mutex_unlock(&mutex_lista_ready_auxiliar);

	log_debug(logger, "Paso a READY aux el proceso %d", pcb->id);
}

void pasar_a_exec(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_exec);
	list_add(LISTA_EXEC, pcb);
	pthread_mutex_unlock(&mutex_lista_exec);

	log_debug(logger, "Paso a EXEC el proceso %d", pcb->id);
}

void pasar_a_block_disco(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_blocked_disco);
	list_add(LISTA_BLOCKED_DISCO, pcb);
	pthread_mutex_unlock(&mutex_lista_blocked_disco);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}

void pasar_a_block_wifi(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_blocked_wifi);
	list_add(LISTA_BLOCKED_WIFI, pcb);
	pthread_mutex_unlock(&mutex_lista_blocked_wifi);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}
void pasar_a_block_usb(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_blocked_usb);
	list_add(LISTA_BLOCKED_USB, pcb);
	pthread_mutex_unlock(&mutex_lista_blocked_usb);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}
void pasar_a_block_audio(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_blocked_audio);
	list_add(LISTA_BLOCKED_AUDIO, pcb);
	pthread_mutex_unlock(&mutex_lista_blocked_audio);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}
void pasar_a_block_impresora(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_blocked_impresora);
	list_add(LISTA_BLOCKED_IMPRESORA, pcb);
	pthread_mutex_unlock(&mutex_lista_blocked_impresora);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}

void pasar_a_block_pantalla(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_blocked_pantalla);
	list_add(LISTA_BLOCKED_PANTALLA, pcb);
	pthread_mutex_unlock(&mutex_lista_blocked_pantalla);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}

void pasar_a_block_teclado(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_conexion_memoria);
	list_add(LISTA_BLOCKED_TECLADO, pcb);
	pthread_mutex_unlock(&mutex_conexion_memoria);

	log_debug(logger, "Paso a BLOCK el proceso %d", pcb->id);
}

void pasar_a_exit(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_exit);
	list_add(LISTA_EXIT, pcb);
	pthread_mutex_unlock(&mutex_lista_exit);

	log_debug(logger, "Paso a EXIT el proceso %d", pcb->id);
}

void pasar_a_block_page_fault(t_pcb *pcb)
{
	pthread_mutex_lock(&mutex_lista_block_page_fault);
	list_add(LISTA_BLOCK_PAGE_FAULT, pcb);
	pthread_mutex_unlock(&mutex_lista_block_page_fault);

	log_debug(logger, "Paso a READY el proceso %d", pcb->id);
}

void iniciar_listas_y_semaforos()
{
	// listas
	LISTA_NEW = list_create();
	LISTA_READY = list_create();
	LISTA_EXEC = list_create();
	LISTA_BLOCKED = list_create();
	LISTA_BLOCKED_PANTALLA = list_create();
	LISTA_BLOCKED_TECLADO = list_create();
	LISTA_SOCKETS = list_create();
	LISTA_EXIT = list_create();
	LISTA_READY_AUXILIAR = list_create();
	LISTA_BLOCKED_IMPRESORA = list_create();
	LISTA_BLOCKED_DISCO = list_create();
	LISTA_BLOCK_PAGE_FAULT = list_create();
	LISTA_TABLA_PAGINAS = list_create();
	LISTA_INICIO_TABLA_PAGINA = list_create();
	LISTA_MARCOS_POR_PROCESOS = list_create();
	LISTA_BITMAP_MARCO = list_create();
	LISTA_BLOCKED_WIFI = list_create();
	LISTA_BLOCKED_USB = list_create();
	LISTA_BLOCKED_AUDIO = list_create();
	LISTA_BLOCKED_GENERAL = list_create();

	// mutex
	pthread_mutex_init(&mutex_creacion_ID, NULL);
	pthread_mutex_init(&mutex_ID_Segmnento, NULL);
	pthread_mutex_init(&mutex_lista_new, NULL);
	pthread_mutex_init(&mutex_lista_ready, NULL);
	pthread_mutex_init(&mutex_lista_exec, NULL);
	pthread_mutex_init(&mutex_lista_blocked_usb, NULL);
	pthread_mutex_init(&mutex_lista_blocked_wifi, NULL);
	pthread_mutex_init(&mutex_lista_blocked_audio, NULL);
	pthread_mutex_init(&mutex_lista_blocked_impresora, NULL);
	pthread_mutex_init(&mutex_lista_blocked_disco, NULL);
	pthread_mutex_init(&mutex_lista_blocked_pantalla, NULL);
	pthread_mutex_init(&mutex_conexion_memoria, NULL);

	pthread_mutex_init(&mutex_lista_blocked, NULL);

	pthread_mutex_init(&mutex_lista_ready_auxiliar, NULL);
	pthread_mutex_init(&mutex_creacion_ID_tabla, NULL);
	pthread_mutex_init(&mutex_lista_tabla_paginas, NULL);
	pthread_mutex_init(&mutex_lista_block_page_fault, NULL);
	pthread_mutex_init(&mutex_lista_tabla_paginas_pagina, NULL);
	pthread_mutex_init(&mutex_lista_pagina_marco_por_proceso, NULL);
	pthread_mutex_init(&mutex_lista_marco_por_proceso, NULL);
	pthread_mutex_init(&mutex_lista_marcos_por_proceso_pagina, NULL);

	// semaforos
	sem_init(&sem_ready, 0, 0);
	sem_init(&sem_bloqueo, 0, 0);
	sem_init(&sem_planif_largo_plazo, 0, 0);
	sem_init(&sem_hay_pcb_lista_new, 0, 0);
	sem_init(&sem_hay_pcb_lista_ready, 0, 0);
	sem_init(&sem_agregar_pcb, 0, 0);
	sem_init(&sem_eliminar_pcb, 0, 0);
	sem_init(&sem_pasar_pcb_running, 0, 0);
	sem_init(&sem_timer, 0, 0);
	sem_init(&sem_desalojar_pcb, 0, 0);
	sem_init(&sem_kill_trhread, 0, 0);
	sem_init(&contador_multiprogramacion, 0, configKernel.gradoMultiprogramacion);
	sem_init(&contador_pcb_running, 0, 1);
	sem_init(&contador_bloqueo_teclado_running, 0, 1);
	// sem_init(&contador_bloqueo_pantalla_running, 0, 1);
	sem_init(&contador_bloqueo_disco_running, 0, 1);
	sem_init(&contador_bloqueo_impresora_running, 0, 1);
	sem_init(&contador_bloqueo_wifi_running, 0, 1);
	sem_init(&contador_bloqueo_usb_running, 0, 1);
	sem_init(&contador_bloqueo_audio_running, 0, 1);
	sem_init(&sem_llamar_feedback, 0, 0);
}

void iteratorInt(int value)
{

	log_info(logger, "Segmento = %d", value);
}
void cargarListaReadyIdPCB(t_list *listaReady)
{
	for (int i = 0; i < list_size(listaReady); i++)
	{
		t_pcb *pcb = list_get(listaReady, i);
		log_info(loggerMinimo, "Cola ready %s: [%d]", configKernel.algoritmo, pcb->id);
	}
}

t_tipo_algoritmo obtenerAlgoritmo()
{

	char *algoritmo = configKernel.algoritmo;

	t_tipo_algoritmo algoritmoResultado;

	if (!strcmp(algoritmo, "FIFO"))
	{
		algoritmoResultado = FIFO;
	}
	else if (!strcmp(algoritmo, "RR"))
	{
		algoritmoResultado = RR;
	}
	else
	{
		algoritmoResultado = FEEDBACK;
	}

	return algoritmoResultado;
}

t_pcb *algoritmo_fifo(t_list *lista)
{
	t_pcb *pcb = (t_pcb *)list_remove(lista, 0);
	return pcb;
}

void implementar_feedback()
{

	pthread_mutex_lock(&mutex_lista_ready);
	if (list_size(LISTA_READY) == 0)
	{

		pthread_mutex_unlock(&mutex_lista_ready);
		// log_info(loggerMinimo,"Entrando fifo auxiliar");
		cargarListaReadyIdPCB(LISTA_READY_AUXILIAR);
		implementar_fifo_auxiliar();
	}
	else
	{
		pthread_mutex_unlock(&mutex_lista_ready);
		// log_info(loggerMinimo,"Entrando round robin");
		cargarListaReadyIdPCB(LISTA_READY);
		implementar_rr();
	}
}

void implementar_fifo()
{

	t_pcb *pcb = algoritmo_fifo(LISTA_READY);
	log_info(logger, "Agregando UN pcb a lista exec");
	pasar_a_exec(pcb);
	log_info(logger, "Cant de elementos de exec: %d\n", list_size(LISTA_EXEC));

	log_debug(logger, "Estado Anterior: READY , proceso id: %d", pcb->id);
	log_debug(logger, "Estado Actual: EXEC , proceso id: %d", pcb->id);

	// Cambio de estado
	log_info(loggerMinimo, "Cambio de Estado: PID %d - Estado Anterior: READY , Estado Actual: EXEC", pcb->id);

	sem_post(&sem_pasar_pcb_running);
}

void implementar_fifo_auxiliar()
{

	t_pcb *pcb = algoritmo_fifo(LISTA_READY_AUXILIAR);
	log_info(logger, "Agregando un pcb a lista exec");
	pasar_a_exec(pcb);
	log_info(logger, "Cant de elementos de exec: %d\n", list_size(LISTA_EXEC));

	log_debug(logger, "Estado Anterior: READY , proceso id: %d", pcb->id);
	log_debug(logger, "Estado Actual: EXEC , proceso id: %d", pcb->id);

	// Cambio de estado

	log_info(loggerMinimo, "Cambio de Estado: PID %d - Estado Anterior: READY , Estado Actual: EXEC", pcb->id);

	sem_post(&sem_pasar_pcb_running);
}

void implementar_rr()
{
	t_pcb *pcb = algoritmo_fifo(LISTA_READY);

	pthread_t thrTimer;

	int hiloTimerCreado = pthread_create(&thrTimer, NULL, (void *)hilo_timer, NULL);

	int detach = pthread_detach(thrTimer);
	log_info(logger, "se creo el hilo timer correctamente?: %d, %d\n ", hiloTimerCreado, detach);
	hayTimer = true;
	log_info(logger, "Agregando UN pcb a lista exec rr");
	pasar_a_exec(pcb);
	log_info(logger, "Cant de elementos de exec: %d\n", list_size(LISTA_EXEC));

	log_debug(logger, "Estado Anterior: READY , proceso id: %d", pcb->id);
	log_debug(logger, "Estado Actual: EXEC , proceso id: %d", pcb->id);

	// Cambio de estado
	log_info(loggerMinimo, "Cambio de Estado: PID %d - Estado Anterior: READY , Estado Actual: EXEC", pcb->id);

	sem_post(&sem_timer);
	sem_post(&sem_pasar_pcb_running);

	log_info(logger, "Esperando matar el timer\n");
	sem_wait(&sem_kill_trhread);

	// pthread_cancel(thrTimer);

	if (pthread_cancel(thrTimer) == 0)
	{
		log_info(logger, "Hilo cancelado con exito");
	}
	else
	{
		log_info(logger, "No mate el hilo");
	}
	log_warning(logger, "saliendo de RR\n");
}

void hilo_timer()
{
	sem_wait(&sem_timer);
	log_info(logger, "voy a dormir, soy el timer\n");
	usleep(configKernel.quantum * 1000);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	log_info(logger, "me desperte!\n");
	sem_post(&sem_desalojar_pcb);

	log_info(logger, "envie post desalojar pcb\n");
}

void serializarValor(uint32_t valorRegistro, int socket, t_tipoMensaje tipoMensaje)
{

	t_buffer *buffer = malloc(sizeof(t_buffer));

	buffer->size = sizeof(uint32_t) * 2;
	void *stream = malloc(buffer->size);
	int offset = 0;

	memcpy(stream + offset, &(valorRegistro), sizeof(uint32_t));
	offset += sizeof(uint32_t);

	buffer->stream = stream;
	crearPaquete(buffer, tipoMensaje, socket);
}

uint32_t *deserializarValor(t_buffer *buffer, int socket)
{
	uint32_t *valorRegistro = malloc(sizeof(uint32_t));
	void *stream = buffer->stream;

	memcpy(&(valorRegistro), stream, sizeof(uint32_t));
	stream += sizeof(uint32_t);

	return valorRegistro;
}
