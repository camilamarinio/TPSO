#include "kernel.h"

int main(int argc, char **argv)
{

	iniciar_kernel();

	crear_hilos_kernel();
}

t_configKernel extraerDatosConfig(t_config *archivoConfig)
{
	configKernel.ipMemoria = string_new();
	configKernel.puertoMemoria = string_new();
	configKernel.ipCPU = string_new();
	configKernel.puertoCPUDispatch = string_new();
	configKernel.puertoCPUInterrupt = string_new();
	configKernel.puertoEscucha = string_new();
	configKernel.algoritmo = string_new();

	configKernel.ipMemoria = config_get_string_value(archivoConfig, "IP_MEMORIA");
	configKernel.puertoMemoria = config_get_string_value(archivoConfig, "PUERTO_MEMORIA");
	configKernel.ipCPU = config_get_string_value(archivoConfig, "IP_CPU");
	configKernel.puertoCPUDispatch = config_get_string_value(archivoConfig, "PUERTO_CPU_DISPATCH");
	configKernel.puertoCPUInterrupt = config_get_string_value(archivoConfig, "PUERTO_CPU_INTERRUPT");
	configKernel.puertoEscucha = config_get_string_value(archivoConfig, "PUERTO_ESCUCHA");
	configKernel.algoritmo = config_get_string_value(archivoConfig, "ALGORITMO_PLANIFICACION");
	configKernel.gradoMultiprogramacion = config_get_int_value(archivoConfig, "GRADO_MAX_MULTIPROGRAMACION");
	configKernel.dispositivosIO = config_get_array_value(archivoConfig, "DISPOSITIVOS_IO");
	configKernel.tiemposIO = config_get_array_value(archivoConfig, "TIEMPOS_IO");

	configKernel.quantum = config_get_int_value(archivoConfig, "QUANTUM_RR");
	return configKernel;
}

void crear_hilos_kernel()
{
	pthread_t thrCpu, thrMemoria, thrPlanificadorLargoPlazo, thrPlanificadorCortoPlazo, thrBloqueo, thrConsola;

	pthread_create(&thrConsola, NULL, (void *)crear_hilo_consola, NULL);
	pthread_create(&thrCpu, NULL, (void *)crear_hilo_cpu, NULL);
	pthread_create(&thrMemoria, NULL, (void *)conectar_memoria, NULL);
	pthread_create(&thrPlanificadorLargoPlazo, NULL, (void *)planifLargoPlazo, NULL);
	pthread_create(&thrPlanificadorCortoPlazo, NULL, (void *)planifCortoPlazo, NULL);

	pthread_detach(thrCpu);
	pthread_detach(thrPlanificadorCortoPlazo);
	pthread_detach(thrMemoria);
	pthread_detach(thrPlanificadorLargoPlazo);
	// crear_hilo_consola();
	pthread_join(thrConsola, NULL);

	log_destroy(logger);
	config_destroy(config);
}

void crear_hilo_consola()
{
	int server_fd = iniciar_servidor(IP_SERVER, configKernel.puertoEscucha); // socket(), bind() listen()
	log_info(logger, "Servidor listo para recibir al cliente");

	while (1)
	{
		pthread_t hilo_atender_consola;
		t_args_pcb *argumentos = malloc(sizeof(t_args_pcb));
		// esto se podria cambiar como int* cliente_fd= malloc(sizeof(int)); si lo ponemos, va antes del while
		argumentos->socketCliente = esperar_cliente(server_fd);

		int cod_op = recibir_operacion(argumentos->socketCliente);

		log_info(logger, "Llegaron las instrucciones y los segmentos");
		argumentos->informacion = recibir_informacion(argumentos->socketCliente);

		enviarResultado(argumentos->socketCliente, "Quedate tranqui Consola, llego todo lo que mandaste ;)\n");
		pthread_create(&hilo_atender_consola, NULL, (void *)crear_pcb, (void *)argumentos);
		pthread_detach(hilo_atender_consola);
	}

	log_error(logger, "Muere hilo multiconsolas");
}

void crear_hilo_cpu()
{

	pthread_t thrDispatch, thrInterrupt;

	pthread_create(&thrDispatch, NULL, (void *)conectar_dispatch, NULL);
	pthread_create(&thrInterrupt, NULL, (void *)conectar_interrupt, NULL);

	pthread_detach(thrDispatch);
	pthread_detach(thrInterrupt);
}

void conectar_dispatch()
{
	// VALGRID que no haya ... y que no se esten modificando las cargas de memoria en la ejecucion , se va a ver en las pruebas de estabilidad
	//  Enviar PCB
	pthread_mutex_lock(&mutex_lista_blocked_audio);
	conexionDispatch = crear_conexion(configKernel.ipCPU, configKernel.puertoCPUDispatch);
	pthread_mutex_unlock(&mutex_lista_blocked_audio);

	while (1)
	{
		// recomendacion: hacer que solo se lea el codigo del paquete para hacer los case , pero despues el recibir los mensajes
		// y pcbs esten dentro de cada case particular y ahi mandar lo que se necesita
		// En los io no leer la instruccion desde el quernel sino que directamente el cpu nos mande el dispositivo a ejecutar y en el caso
		//  de los del kernel tambien el tiempo de ejecucion

		sem_wait(&sem_pasar_pcb_running);
		log_info(logger, "Llego un pcb a dispatch");
		pthread_mutex_lock(&mutex_lista_blocked_audio);

		serializarPCB(conexionDispatch, list_get(LISTA_EXEC, 0), DISPATCH_PCB);
		pthread_mutex_unlock(&mutex_lista_blocked_audio);

		log_info(logger, "Se envio pcb a cpu\n");
		void *pcbAEliminar = list_remove(LISTA_EXEC, 0);
		free(pcbAEliminar);
		log_info(logger, "Cantidad de elementos en lista exec: %d\n", list_size(LISTA_EXEC));

		// Recibir PCB
		pthread_mutex_lock(&mutex_lista_blocked_audio);
		t_paqueteActual *paquete = recibirPaquete(conexionDispatch);
		pthread_mutex_unlock(&mutex_lista_blocked_audio);
		log_info(logger, "Recibi de nuevo el pcb\n");
		log_info(logger, "estoy en %d: ", paquete->codigo_operacion);
		t_pcb *pcb = deserializoPCB(paquete->buffer);
		log_info(logger, "estoy en %d: ", paquete->codigo_operacion);
		log_info(logger, "Id proceso nuevo que llego de cpu: %d", pcb->id);

		t_instruccion *insActual = list_get(pcb->informacion->instrucciones, pcb->program_counter - 1);
		// printf("\n dispositivo %s" , dispositivoToString(insActual->paramIO));

		char *dispositivoIO;

		if (hayTimer == true)
		{
			log_info(logger, "sempostkilltrhread\n");
			sem_post(&sem_kill_trhread);
			hayTimer = false;
		}

		switch (paquete->codigo_operacion)
		{
		case SEGMENTATION_FAULT:
		case EXIT_PCB:
			if(paquete->codigo_operacion == 20){
				log_info(loggerMinimo, "Motivo de finalizacion: Segmentation Fault");
			}else if(paquete->codigo_operacion == 8){
				log_info(loggerMinimo, "Motivo de finalizacion: Exit");
			}
			
			pasar_a_exec(pcb);
			eliminar_pcb();
			pthread_mutex_lock(&mutex_conexion_memoria);
			serializarPCB(conexionMemoria, pcb, LIBERAR_RECURSOS);
			pthread_mutex_unlock(&mutex_conexion_memoria);

			pthread_mutex_lock(&mutex_conexion_memoria);
			char *mensaje = recibirMensaje(conexionMemoria);
			pthread_mutex_unlock(&mutex_conexion_memoria);

			log_info(logger, "Mensaje de confirmacion de memoria: %s\n", mensaje);

			serializarValor(0, pcb->socket, TERMINAR_CONSOLA); // esto le mando a la consola
			break;

		case BLOCK_PCB_IO_PANTALLA:
			pthread_t thrBloqueoPantalla;
			dispositivoIO = insActual->paramIO;

			pasar_a_block_pantalla(pcb);
			log_info(loggerMinimo, "Motivo de Bloqueo: PID %d - Bloqueado por: %s ", pcb->id, dispositivoIO);
			pthread_create(&thrBloqueoPantalla, NULL, (void *)manejar_bloqueo_pantalla, (void *)insActual);

			pthread_detach(thrBloqueoPantalla);
			sem_post(&contador_pcb_running);
			break;

		case BLOCK_PCB_IO_TECLADO:
			pthread_t thrBloqueoTeclado;
			dispositivoIO = insActual->paramIO;

			pasar_a_block_teclado(pcb);

			// log_debug(logger, "Ejecutada: 'PID:  %d - Bloqueado por: %s '", pcb->id, dispositivoIO);
			// Motivo de Bloqueo:
			log_info(loggerMinimo, "Motivo de Bloqueo: PID  %d - Bloqueado por: %s ", pcb->id, dispositivoIO);

			pthread_create(&thrBloqueoTeclado, NULL, (void *)manejar_bloqueo_teclado, (void *)insActual);

			pthread_detach(thrBloqueoTeclado);
			sem_post(&contador_pcb_running);

			break;

		case BLOCK_PCB_IO:
			// crear una estructura para poder hacer la ejecucion de io de cualquier dispositivo que traiga el config kernel
			// la estructura va a tener la lista , los semaforos y mutex necesario , el dispositivo que es string
			// podemos tenes harcodeado teclado y pantalla pero los dispositivos de config kernel no
			log_debug(logger, "entro al case block io\n");
			pthread_t thrBloqueoGeneral;
			dispositivoIO = insActual->paramIO;
			t_dispositivo *dispositivoEnKernel = buscarDispositivoBlocked(dispositivoIO);

			log_info(logger, "dispositivoKernel %s\n", dispositivoEnKernel->dispositivo);
			if (dispositivoEnKernel == NULL)
			{
				log_info(logger, "El dispositivo no se encontro\n");
			}
			agregar_a_lista_blokeados(dispositivoEnKernel, pcb);

			pthread_create(&thrBloqueoGeneral, NULL, (void *)manejar_bloqueo_general, (void *)insActual);
			pthread_detach(thrBloqueoGeneral);

			sem_post(&contador_pcb_running);
			// pasar_a_block(pcb);

			// log_debug(logger, "Ejecutada: 'PID:  %d - Bloqueado por: %s '", pcb->id, dispositivoIO);
			// Motivo de Bloqueo:
			log_info(loggerMinimo, "Motivo de Bloqueo: PID %d - Bloqueado por: %s ", pcb->id, dispositivoIO);

			break;

		case BLOCK_PCB_PAGE_FAULT:
			log_info(logger, "Entre al case de page fault");

			// que se lea el mensaje de cpu desde aca y no desde otro hilo
			pthread_t thrBloqueoPageFault;

			pasar_a_block_page_fault(pcb);

			log_info(logger, "Estoy en la funcion de manejo de page fault");

			log_info(logger, "Estoy en la funcion de manejo de page fault");

			t_paqt paquete;
			pthread_mutex_lock(&mutex_lista_blocked_audio);
			recibirMsje(conexionDispatch, &paquete);
			pthread_mutex_unlock(&mutex_lista_blocked_audio);

			pthread_create(&thrBloqueoPageFault, NULL, (void *)manejar_bloqueo_page_fault, (void *)&paquete);

			pthread_detach(thrBloqueoPageFault);

			sem_post(&contador_pcb_running);

			break;
		case INTERRUPT_INTERRUPCION:

			pthread_t thrInterrupt;
			log_debug(logger, "Fin de Quantum: PID %d - Desalojado por fin de Quantum", pcb->id);
			// Fin de quantum
			log_info(loggerMinimo, "Fin de Quantum: PID %d - Desalojado por fin de Quantum", pcb->id);

			log_info(logger, "entrando a manejar interrupcion\n");
			t_tipo_algoritmo algoritmo = obtenerAlgoritmo();
			log_info(logger, "%d\n", algoritmo);
			if (algoritmo == FEEDBACK)
			{
				// log_info(logger, "Paso a ready auxiliar - FIFO");
				pasar_a_ready_auxiliar(pcb);
				sem_post(&sem_hay_pcb_lista_ready);
			}
			else if (algoritmo == RR)
			{
				log_info(logger, "El algoritmo obtenido es: %d\n", obtenerAlgoritmo());
				log_info(logger, "Cantidad de elementos en lista exec: %d\n", list_size(LISTA_EXEC));

				pasar_a_ready(pcb);
				log_info(logger, "Cantidad de elementos en ready: %d\n", list_size(LISTA_READY));
				sem_post(&sem_hay_pcb_lista_ready);
				log_info(logger, "Cantidad de elementos en ready: %d\n", list_size(LISTA_READY));
			}
			log_info(logger, "Termine de manejar la interrupcion");
			sem_post(&contador_pcb_running);
			break;

		default:
			break;
		}
		free(paquete->buffer->stream);
		free(paquete->buffer);
		free(paquete);
		// free(dispositivoIO);
	}
}

void manejar_bloqueo_teclado(void *insActual)
{
	// sem_wait(&contador_bloqueo_teclado_running);
	t_instruccion *instActualConsola = (t_instruccion *)insActual;
	uint32_t valorRegistroTeclado;

	t_pcb *pcb = algoritmo_fifo(LISTA_BLOCKED_TECLADO);
	serializarValor(1, pcb->socket, BLOCK_PCB_IO_TECLADO);
	enviarResultado(pcb->socket, "solicito el ingreso de un valor por teclado");

	t_paqueteActual *paquete = recibirPaquete(pcb->socket);

	valorRegistroTeclado = deserializarValor(paquete->buffer, pcb->socket);
	log_info(logger, "El valor de teclado es:%d\n", valorRegistroTeclado);
	switch (instActualConsola->paramReg[0])
	{
	case AX:
		pcb->registros.AX = valorRegistroTeclado;
		log_info(logger, "El valor del registro ingresado por consola es: %d", pcb->registros.AX);
		break;
	case BX:
		pcb->registros.BX = valorRegistroTeclado;
		log_info(logger, "El valor del registro ingresado por consola es: %d", pcb->registros.BX);
		break;
	case CX:
		pcb->registros.CX = valorRegistroTeclado;
		log_info(logger, "El valor del registro ingresado por consola es: %d", pcb->registros.CX);
		break;
	case DX:
		pcb->registros.DX = valorRegistroTeclado;
		log_info(logger, "El valor del registro ingresado por consola es: %d", pcb->registros.DX);
		break;
	}

	pasar_a_ready(pcb);
	// sem_post(&contador_bloqueo_teclado_running);
	sem_post(&sem_hay_pcb_lista_ready);
}

void manejar_bloqueo_pantalla(void *insActual)
{
	// sem_wait(&contador_bloqueo_pantalla_running);
	t_instruccion *instActualPantalla = (t_instruccion *)insActual;

	uint32_t valorRegistro;

	t_pcb *pcb = algoritmo_fifo(LISTA_BLOCKED_PANTALLA);

	log_info(logger, "%d", instActualPantalla->paramReg[0]);

	switch (instActualPantalla->paramReg[0])
	{
	case AX:
		valorRegistro = pcb->registros.AX;
		log_info(logger, "valor registro %d", valorRegistro);

		break;
	case BX:
		valorRegistro = pcb->registros.BX;
		log_info(logger, "valor registro %d", valorRegistro);
		break;
	case CX:
		valorRegistro = pcb->registros.CX;
		log_info(logger, "valor registro %d", valorRegistro);
		break;
	case DX:
		valorRegistro = pcb->registros.DX;
		log_info(logger, "valor registro %d", valorRegistro);
		break;
	}

	//  Serializamos valor registro y se envia a la consola
	log_info(logger, "El valor del registro que se muestra por pantalla es: %d", valorRegistro);
	;
	serializarValor(valorRegistro, pcb->socket, BLOCK_PCB_IO_PANTALLA);
	char *mensaje = recibirMensaje(pcb->socket);
	log_info(logger, "Me llego el mensaje: %s\n", mensaje);

	pasar_a_ready(pcb);
	// sem_post(&contador_bloqueo_pantalla_running);
	sem_post(&sem_hay_pcb_lista_ready);
}

void manejar_bloqueo_general(void *insActual)
{
	// sem_wait(&contador_bloqueo_impresora_running);
	t_instruccion *instActualBloqueoGeneral = (t_instruccion *)insActual;
	char *dispositivoCpu = instActualBloqueoGeneral->paramIO;

	t_dispositivo *dispositivoEnKernel = buscarDispositivoBlocked(dispositivoCpu);

	if (dispositivoEnKernel == NULL)
	{
		log_info(logger, "El dispositivo no se encontro\n");
	}
	printf("DispositivoKernel %s\n", dispositivoEnKernel->dispositivo);
	sem_wait(&dispositivoEnKernel->contador_bloqueo);
	uint32_t duracionUnidadDeTrabajo;

	if (!strcmp(dispositivoEnKernel->dispositivo, dispositivoCpu))
	{
		duracionUnidadDeTrabajo = dispositivoEnKernel->tiempoEjecucion * instActualBloqueoGeneral->paramInt;

		log_info(logger, "Ejecutando el dispositivo %s", dispositivoCpu);
		log_info(logger, "Por un tiempo de: %d", duracionUnidadDeTrabajo);

		t_pcb *pcb = algoritmo_fifo(dispositivoEnKernel->lista_block);

		usleep(duracionUnidadDeTrabajo * 1000);
		pasar_a_ready(pcb);
		sem_post(&sem_hay_pcb_lista_ready);
	}
	// free(dispositivoCpu);
	sem_post(&dispositivoEnKernel->contador_bloqueo);
}

t_dispositivo *buscarDispositivoBlocked(char *dispositivo)
{
	t_dispositivo *dispositivoIO = NULL;
	for (int i = 0; i < list_size(LISTA_BLOCKED); i++)
	{
		t_dispositivo *dispositivoNuevo = list_get(LISTA_BLOCKED, i);

		if (!strcmp(dispositivoNuevo->dispositivo, dispositivo))
		{
			dispositivoIO = dispositivoNuevo;
		}
	}

	return dispositivoIO;
}

void cargarDispositivos()
{
	for (int i = 0; i < size_char_array(configKernel.dispositivosIO); i++)
	{
		log_info(logger, "Tamaño %d\n", size_char_array(configKernel.dispositivosIO));
		char *dispositivoNuevo = configKernel.dispositivosIO[i];
		t_dispositivo *dispositivo = malloc(sizeof(t_dispositivo));

		dispositivo->dispositivo = dispositivoNuevo;
		dispositivo->lista_block = list_create();
		dispositivo->tiempoEjecucion = atoi(configKernel.tiemposIO[i]);
		sem_init(&dispositivo->contador_bloqueo, 0, 1);
		pthread_mutex_init(&dispositivo->mutex_lista_blocked, NULL);

		log_info(logger, "Dispositivos %s , %d \n", dispositivo->dispositivo, dispositivo->tiempoEjecucion);
		agregrar_dispositivo(dispositivo);
	}
}

bool noEstaEnBlocked(char *dispositivo)
{
	bool valor = true;
	for (int i = 0; i < list_size(LISTA_BLOCKED); i++)
	{
		t_dispositivo *dispositivoNuevo = list_get(LISTA_BLOCKED, i);

		if (!strcmp(dispositivoNuevo->dispositivo, dispositivo))
		{
			agregar_a_lista_blokeados(dispositivoNuevo, dispositivo);
			valor = false;
		}
	}
	return valor;
}

void agregar_a_lista_blokeados(t_dispositivo *dispositivo, t_pcb *pcb)
{
	pthread_mutex_lock(&dispositivo->mutex_lista_blocked);
	list_add(dispositivo->lista_block, pcb);
	pthread_mutex_unlock(&dispositivo->mutex_lista_blocked);
}

void agregrar_dispositivo(t_dispositivo *dispositivo)
{
	pthread_mutex_lock(&mutex_lista_blocked);
	list_add(LISTA_BLOCKED, dispositivo);
	pthread_mutex_unlock(&mutex_lista_blocked);
}

void manejar_bloqueo_page_fault(void *paquete)
{
	t_paqt *paqueteActual = (t_paqt *)paquete;
	MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT *mensajePaquete = malloc(sizeof(MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT));
	mensajePaquete = paqueteActual->mensaje;
	t_pcb *pcb = algoritmo_fifo(LISTA_BLOCK_PAGE_FAULT);

	log_info(loggerMinimo, "Page Fault PID: %d - Segmento: %d - Pagina: %d", pcb->id, mensajePaquete->nro_segmento, mensajePaquete->nro_pagina);
	// enviar a memoria
	serializarPCB(conexionMemoria, pcb, PAGE_FAULT);

	enviarMsje(conexionMemoria, KERNEL, paqueteActual->mensaje, sizeof(MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT), PAGE_FAULT);

	// recibo de memoria
	char *mensaje = recibirMensaje(conexionMemoria);

	log_info(logger, "Mensaje recibido por memoria:%s", mensaje);

	pasar_a_ready(pcb);

	sem_post(&sem_hay_pcb_lista_ready);
}

void manejar_interrupcion(void *pcbElegida)
{
	log_info(logger, "Entrando a manejar interrupcion\n");
	t_tipo_algoritmo algoritmo = obtenerAlgoritmo();
	log_info(logger, "%d\n", algoritmo);
	t_pcb *pcb = (t_pcb *)pcbElegida;
	if (algoritmo == FEEDBACK)
	{
		log_info(logger, "pasar a ready aux");
		pasar_a_ready_auxiliar(pcb);
		sem_post(&sem_hay_pcb_lista_ready);
	}
	else if (algoritmo == RR)
	{
		log_info(logger, "El algoritmo obtenido es: %d\n", obtenerAlgoritmo());
		log_info(logger, "cantidad de elementos en lista exec: %d\n", list_size(LISTA_EXEC));

		pasar_a_ready(pcb);
		log_info(logger, "cantidad de elementos en ready: %d\n", list_size(LISTA_READY));
		sem_post(&sem_hay_pcb_lista_ready);
		log_info(logger, "cantidad de elementos en ready: %d\n", list_size(LISTA_READY));
	}
	log_info(logger, "termine de manejar la interrupcion");
}

void conectar_interrupt()
{
	conexionInterrupt = crear_conexion(configKernel.ipCPU, configKernel.puertoCPUInterrupt);

	while (1)
	{
		sem_wait(&sem_desalojar_pcb);
		log_info(logger, "desalojo pcb\n");
		enviarResultado(conexionInterrupt, "interrupcion de la instruccion");
	}
}

void conectar_memoria()
{
	conexionMemoria = crear_conexion(configKernel.ipMemoria, configKernel.puertoMemoria);
	enviarResultado(conexionMemoria, "hola memoria soy el kernel");
}

void iniciar_kernel()
{

	// Parte Server
	logger = iniciar_logger("kernel.log", "KERNEL", LOG_LEVEL_DEBUG);
	loggerMinimo = iniciar_logger("kernelLoggsObligatorios.log", "KERNEL", LOG_LEVEL_INFO);

	config = iniciar_config("kernel.config");

	// creo el struct
	extraerDatosConfig(config);

	iniciar_listas_y_semaforos();
	cargarDispositivos();

	contadorIdPCB = 1;
	contadorIdSegmento = 0;
	hayTimer = false;
}

void crear_pcb(void *argumentos)
{
	log_info(logger, "Consola conectada, paso a crear el hilo");
	t_args_pcb *args = (t_args_pcb *)argumentos;
	t_pcb *pcb = malloc(sizeof(t_pcb));

	pcb->socket = args->socketCliente;
	pcb->program_counter = 0;
	pcb->informacion = &(args->informacion);
	pcb->registros.AX = 0;
	pcb->registros.BX = 0;
	pcb->registros.CX = 0;
	pcb->registros.DX = 0;
	pcb->tablaSegmentos = list_create();

	for (int i = 0; i < list_size(pcb->informacion->segmentos); i++)
	{
		t_tabla_segmentos *tablaSegmento = malloc(sizeof(t_tabla_segmentos));
		uint32_t segmento = list_get(pcb->informacion->segmentos, i);

		tablaSegmento->tamanio = segmento;

		pthread_mutex_lock(&mutex_ID_Segmnento);
		tablaSegmento->id = contadorIdSegmento;
		contadorIdSegmento++;
		pthread_mutex_unlock(&mutex_ID_Segmnento);

		list_add(pcb->tablaSegmentos, tablaSegmento);
	}

	// printf("\nsocket del pcb: %d", pcb->socket);

	pthread_mutex_lock(&mutex_creacion_ID);
	pcb->id = contadorIdPCB;
	contadorIdPCB++;
	pthread_mutex_unlock(&mutex_creacion_ID);

	pasar_a_new(pcb);
	log_debug(logger, "Estado Actual: NEW , proceso id: %d", pcb->id);
	log_info(loggerMinimo, "Creación de Proceso: se crea el proceso %d en NEW", pcb->id); // Creacion de Proceso?
	log_info(logger, "Cant de elementos de new: %d", list_size(LISTA_NEW));

	sem_post(&sem_agregar_pcb);
}

char *dispositivoToString(t_IO dispositivo)
{
	char *string = string_new();
	switch (dispositivo)
	{
	case DISCO:
		string_append(&string, "DISCO");
		return string;
		break;
	case TECLADO:
		string_append(&string, "TECLADO");
		return string;
		break;
	case PANTALLA:
		string_append(&string, "PANTALLA");
		return string;
		break;
	case IMPRESORA:
		string_append(&string, "IMPRESORA");
		return string;
		break;
	case WIFI:
		string_append(&string, "WIFI");
		return string;
		break;
	case AUDIO:
		string_append(&string, "AUDIO");
		return string;
		break;
	case USB:
		string_append(&string, "USB");
		return string;
		break;
	default:
		log_error(logger, "No existe el dispositivo");
		break;
	}
}

void planifLargoPlazo()
{
	while (1)
	{
		sem_wait(&sem_agregar_pcb);
		agregar_pcb();
	}
}

void planifCortoPlazo()
{
	while (1)
	{
		sem_wait(&sem_hay_pcb_lista_ready);
		log_info(logger, "Llego pcb a plani corto plazo");
		t_tipo_algoritmo algoritmo = obtenerAlgoritmo();

		sem_wait(&contador_pcb_running);

		switch (algoritmo)
		{
		case FIFO:
			log_debug(logger, "Implementando algoritmo FIFO");
			log_debug(logger, " Cola Ready FIFO:");
			cargarListaReadyIdPCB(LISTA_READY);
			implementar_fifo();

			break;
		case RR:
			log_debug(logger, "Implementando algoritmo RR");
			log_debug(logger, " Cola Ready RR:");
			cargarListaReadyIdPCB(LISTA_READY);
			implementar_rr();

			break;
		case FEEDBACK:
			log_debug(logger, "Implementando algoritmo FEEDBACK");
			implementar_feedback();

			break;

		default:
			break;
		}
	}
}


void agregar_pcb()
{
	sem_wait(&contador_multiprogramacion);

	log_info(logger, "Agregando un pcb a lista ready");

	pthread_mutex_lock(&mutex_lista_new);

	for (int i = 0; i < list_size(LISTA_NEW); i++)
	{
		t_pcb *pcbAux = list_get(LISTA_NEW, i);
		printf("\nposicion pcb %d, pcbid:%d, tamanio segmentos %d", i, pcbAux->id, list_size(pcbAux->informacion->segmentos));
		// imprimirInstruccionesYSegmentos(*(pcbAux->informacion));
	}

	t_pcb *pcb = list_remove(LISTA_NEW, 0); // aca esta el problema, imprimir el list_size

	log_info(logger, "\nCant de elementos de new: %d\n", list_size(LISTA_NEW));
	pthread_mutex_unlock(&mutex_lista_new);

	// solicito que memoria inicialice sus estructuras
	pthread_mutex_lock(&mutex_conexion_memoria);
	serializarPCB(conexionMemoria, pcb, ASIGNAR_RECURSOS);
	pthread_mutex_unlock(&mutex_conexion_memoria);
	free(pcb);
	log_info(logger, "Envio recursos a memoria\n");

	// memoria me devuelve el pcb modificado
	pthread_mutex_lock(&mutex_conexion_memoria);
	t_paqueteActual *paquete = recibirPaquete(conexionMemoria);
	// pthread_mutex_unlock(&mutex_conexion_memoria);
	log_info(logger, "Recibo recursos de memoria\n");
	if (paquete == NULL)
	{
		log_error(logger, "Paquete nulo\n");
	}
	else
	{
		log_info(logger, "Paquete no nulo\n");
	}
	// pthread_mutex_lock(&mutex_conexion_memoria);
	pcb = deserializoPCB(paquete->buffer);
	pthread_mutex_unlock(&mutex_conexion_memoria);

	// imprimirInstruccionesYSegmentos(*(pcb->informacion));
	/*for (int i = 0; i < list_size(pcb->tablaSegmentos); i++)
	{
		t_tabla_segmentos *tablaSegmento = malloc(sizeof(t_tabla_segmentos));

		t_tabla_segmentos *segmento = list_get(pcb->tablaSegmentos, i);
		log_info(logger,"El id del segmento es: %d\n", segmento->id);

		log_info(logger,"El id de la tabla es: %d\n", segmento->indiceTablaPaginas);
	}*/

	pasar_a_ready(pcb);

	log_debug(logger, "Estado Anterior: NEW , proceso id: %d", pcb->id);
	log_debug(logger, "Estado Actual: READY , proceso id: %d", pcb->id);
	// Cambio de estado
	log_info(loggerMinimo, "Cambio de Estado: PID %d - Estado Anterior: NEW, Estado Actual: READY", pcb->id);

	log_info(logger, "Cant de elementos de ready: %d\n", list_size(LISTA_READY));

	sem_post(&sem_hay_pcb_lista_ready);

	log_info(logger, "Envie a memoria los recursos para asignar");
}

void eliminar_pcb()
{
	pthread_mutex_lock(&mutex_lista_exec);
	t_pcb *pcb = algoritmo_fifo(LISTA_EXEC);
	pthread_mutex_unlock(&mutex_lista_exec);

	// solicito que memoria libere sus estructuras
	// serializarPCB(conexionMemoria, pcb, LIBERAR_RECURSOS);

	// memoria me devuelve el pcb modificado
	// t_paqueteActual *paquete = recibirPaquete(conexionMemoria);

	// pcb = deserializoPCB(paquete->buffer);

	pasar_a_exit(pcb);

	sem_post(&contador_pcb_running);
	log_debug(logger, "Estado Anterior: EXEC , proceso id: %d", pcb->id);
	log_debug(logger, "Estado, proceso Actual: EXIT  id: %d", pcb->id);
	// Cambio de estado
	log_info(loggerMinimo, "Cambio de Estado: PID %d - Estado Anterior: EXEC , Estado Actual: EXIT", pcb->id);

	for (int i = 0; i < list_size(LISTA_EXIT); i++)
	{
		t_pcb *pcb = list_get(LISTA_EXIT, i);
		log_debug(logger, "Procesos finalizados: %d", pcb->id);
	}

	sem_post(&contador_multiprogramacion);
}