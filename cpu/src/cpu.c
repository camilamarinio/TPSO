#include "cpu.h"

int main(char argc, char **argv)
{

	logger = iniciar_logger("cpu.log", "CPU", LOG_LEVEL_DEBUG);
	loggerMinimo = iniciar_logger("cpuLoggsObligatorios.log", "CPU", LOG_LEVEL_INFO);

	config = iniciar_config("cpu.config");

	extraerDatosConfig(config);
	inicializarTLB();
	pthread_t thrDispatchKernel, thrInterruptKernel, thrMemoria;

	pthread_create(&thrDispatchKernel, NULL, (void *)iniciar_servidor_dispatch, NULL);
	pthread_create(&thrInterruptKernel, NULL, (void *)iniciar_servidor_interrupt, NULL);
	printf(PRINT_COLOR_BLUE "Conectando con modulo Memoria..." PRINT_COLOR_RESET "\n");
	pthread_create(&thrMemoria, NULL, (void *)conectar_memoria, NULL);

	pthread_join(thrDispatchKernel, NULL);
	pthread_join(thrInterruptKernel, NULL);
	pthread_join(thrMemoria, NULL);

	log_destroy(logger);
	config_destroy(config);
}

t_configCPU extraerDatosConfig(t_config *archivoConfig)
{

	configCPU.reemplazoTLB = string_new();
	configCPU.ipMemoria = string_new();
	configCPU.puertoMemoria = string_new();
	configCPU.puertoEscuchaDispatch = string_new();
	configCPU.puertoEscuchaInterrupt = string_new();

	configCPU.ipCPU = config_get_string_value(archivoConfig, "IP_CPU");

	configCPU.ipMemoria = config_get_string_value(archivoConfig, "IP_MEMORIA");
	configCPU.puertoMemoria = config_get_string_value(archivoConfig, "PUERTO_MEMORIA");
	configCPU.reemplazoTLB = config_get_string_value(archivoConfig, "REEMPLAZO_TLB");
	configCPU.puertoEscuchaDispatch = config_get_string_value(archivoConfig, "PUERTO_ESCUCHA_DISPATCH");
	configCPU.puertoEscuchaInterrupt = config_get_string_value(archivoConfig, "PUERTO_ESCUCHA_INTERRUPT");
	configCPU.retardoInstruccion = config_get_int_value(archivoConfig, "RETARDO_INSTRUCCION");
	configCPU.entradasTLB = config_get_int_value(archivoConfig, "ENTRADAS_TLB");

	return configCPU;
}

void iniciar_servidor_dispatch()
{
	int server_fd = iniciar_servidor(IP_SERVER, configCPU.puertoEscuchaDispatch); // socket(), bind(), listen()
	log_info(logger, "Servidor listo para recibir al dispatch kernel");
	pthread_mutex_lock(&mutex_lista_blocked_audio);
	socketAceptadoDispatch = esperar_cliente(server_fd);
	pthread_mutex_unlock(&mutex_lista_blocked_audio);

	while (1)
	{
		pthread_mutex_lock(&mutex_lista_blocked_audio);
		t_paqueteActual *paquete = recibirPaquete(socketAceptadoDispatch);
		pthread_mutex_unlock(&mutex_lista_blocked_audio);
		interrupciones = false;
		retornePCB = false;
		t_pcb *pcb = deserializoPCB(paquete->buffer);
		free(paquete->buffer->stream);
		free(paquete->buffer);
		free(paquete);

		// imprimirInstruccionesYSegmentos(*(pcb->informacion));

		log_info(logger, "se recibio pcb de running de kernel\n");

		do
		{
			retornePCB = cicloInstruccion(pcb);

			checkInterrupt(pcb, retornePCB);
			log_info(logger, "retornePCB %d\n", retornePCB);
		} while (!interrupciones && !retornePCB);
		log_info(logger, "Sali del while infinito\n");
	}
}

void iniciar_servidor_interrupt()
{
	int server_fd = iniciar_servidor(NULL, configCPU.puertoEscuchaInterrupt);
	log_info(logger, "Servidor listo para recibir al interrupt kernel");

	int cliente_fd = esperar_cliente(server_fd);
	while (1)
	{
		char *mensaje = recibirMensaje(cliente_fd);
		if (mensaje == NULL)
		{
			break;
		}
		log_info(logger, "Me llego el mensaje: %s\n", mensaje);

		interrupciones = true;
		free(mensaje);
	}
	log_error(logger, "Se desconecto interrupt\n");
}

void conectar_memoria()
{
	conexion = crear_conexion(configCPU.ipMemoria, configCPU.puertoMemoria);
	// enviar_mensaje("hola memoria, soy el cpu", conexion);
	enviarResultado(conexion, "hola memoria soy el cpu");

	log_debug(logger, "Buscando configuracion inicial de memoria");
	// socketMemoria = crear_conexion(configCPU.ipMemoria, configCPU.puertoMemoria);

	enviarMsje(conexion, CPU, NULL, 0, HANDSHAKE_INICIAL);
	log_debug(logger, "Se envio Handshake a MEMORIA");

	MSJ_INT *mensaje = malloc(sizeof(MSJ_INT));
	// mensaje->numero = 1;

	enviarMsje(conexion, CPU, mensaje, sizeof(MSJ_INT), CONFIG_DIR_LOG_A_FISICA);
	free(mensaje);
	log_debug(logger, "Esperando mensaje de memoria para config inicial");

	t_paqt paquete;

	// recibe el mensaje inicial de memoria
	recibirMsje(conexion, &paquete);

	// Reservo espacio para recibir las ENTRADAS_POR_TABLA y TAM_PAGINA
	MSJ_MEMORIA_CPU_INIT *infoDeMemoria = malloc(sizeof(MSJ_MEMORIA_CPU_INIT));

	infoDeMemoria = paquete.mensaje;

	log_debug(logger, "Se recibio la informacion de memoria: tamanio pagina= %i cantidad de Entradas por Tabla= %i", infoDeMemoria->tamanioPagina, infoDeMemoria->cantEntradasPorTabla);

	configCPU.cantidadEntradasPorTabla = infoDeMemoria->cantEntradasPorTabla;
	configCPU.tamanioPagina = infoDeMemoria->tamanioPagina;

	free(infoDeMemoria);
}

bool cicloInstruccion(t_pcb *pcb)
{
	t_list *instrucciones = pcb->informacion->instrucciones;
	t_instruccion *insActual = list_get(instrucciones, pcb->program_counter);
	log_info(logger, "insActual->instCode: %i", insActual->instCode);

	// decode
	if (insActual->instCode == MOV_IN || insActual->instCode == MOV_OUT)
	{
		log_debug(logger, "Requiere acceso a Memoria");
		// Hacer algo en proximo Checkpoint
	}

	// fetch
	fetch(pcb);

	// execute
	char *instruccion = string_new();
	string_append(&instruccion, instruccionToString(insActual->instCode));
	char *io = string_new();
	string_append(&io, insActual->paramIO);
	char *registro = string_new();
	string_append(&registro, registroToString(insActual->paramReg[0]));
	char *registro2 = string_new();
	string_append(&registro2, registroToString(insActual->paramReg[1]));

	// Instruccion Ejecutada
	log_info(loggerMinimo, "Instrucción Ejecutada: PID:  %i - Ejecutando: %s %s %s %s %i",
			  pcb->id, instruccion, io, registro, registro2, insActual->paramInt); // log minimo y obligatorio
	free(instruccion);

	// interrupciones = false;
	//  bool retornePCB = false;
	switch (insActual->instCode)
	{
	case SET:
		printf(PRINT_COLOR_CYAN "\nEjecutando instruccion SET - Etapa Execute \n" PRINT_COLOR_CYAN);
		usleep(configCPU.retardoInstruccion * 1000);

		asignarValorARegistro(pcb, insActual->paramReg[0], insActual->paramInt);

		log_debug(logger, "%s = %i", registro, insActual->paramInt);
		free(registro);
		free(registro2);
		free(io);
		log_info(logger, "estado de la interrupcion: %d", interrupciones);
		break;

	case ADD:
		printf(PRINT_COLOR_CYAN "\nEjecutando instruccion ADD - Etapa Execute \n" PRINT_COLOR_CYAN);
		usleep(configCPU.retardoInstruccion * 1000);

		uint32_t registroDestino = matchearRegistro(pcb->registros, insActual->paramReg[0]);
		uint32_t registroOrigen = matchearRegistro(pcb->registros, insActual->paramReg[1]);

		log_debug(logger, "Registro Destino -> %s = %i    &&    Registro Origen -> %s = %i \n Registro Destino = Registro Destino + Registro Origen ",
				  registro, registroDestino, registro2, registroOrigen);
		registroDestino = registroDestino + registroOrigen;
		free(registro2);

		asignarValorARegistro(pcb, insActual->paramReg[0], registroDestino);

		log_debug(logger, "%s = %i", registro, registroDestino);
		free(registro);
		free(io);
		break;

	case MOV_IN:
		printf(PRINT_COLOR_CYAN "\nEjecutando instruccion MOV_IN - Etapa Execute \n" PRINT_COLOR_CYAN);
		log_debug(logger, "Instruccion que lee el valor de memoria del segmento de Datos correspondiente a la DL %i", insActual->paramInt);
		// traducir valor paramint para asignarlo despues a registroCPU

		t_direccionFisica *dirFisicaMoveIn = malloc(sizeof(t_direccionFisica));

		dirFisicaMoveIn = calcular_direccion_fisica(insActual->paramInt, configCPU.cantidadEntradasPorTabla, configCPU.tamanioPagina, pcb); // Para el calculo de la DF no necesitariamos tambien incluir el indice de la tabla de paginas como parametro?????

		if (dirFisicaMoveIn->nroMarco == -1)
		{
			retornePCB = true;
			log_info(logger, "se envio info a kernel por page fault");
		}
		else if (dirFisicaMoveIn->nroMarco == -10)
		{
			log_info(logger, "Kernel finaliza el proceso por Segmentation Fault");
		}
		else
		{
			MSJ_MEMORIA_CPU_LEER *mensajeAMemoriaLeer = malloc(sizeof(MSJ_MEMORIA_CPU_LEER));

			mensajeAMemoriaLeer->desplazamiento = dirFisicaMoveIn->desplazamientoPagina;
			mensajeAMemoriaLeer->nroMarco = dirFisicaMoveIn->nroMarco;
			mensajeAMemoriaLeer->pid = pcb->id;
			enviarMsje(conexion, CPU, mensajeAMemoriaLeer, sizeof(MSJ_MEMORIA_CPU_LEER), ACCESO_MEMORIA_LEER);
			log_info(loggerMinimo, "Acceso Memoria: PID: %d - Acción: LEER - Segmento: %d - Pagina: %d - Dirección Fisica: %d %d", pcb->id, dirFisicaMoveIn->dl.nroSegmento, dirFisicaMoveIn->dl.nroPagina, mensajeAMemoriaLeer->nroMarco, mensajeAMemoriaLeer->desplazamiento);
			log_debug(logger, "Envie direccion fisica a memoria: MARCO: %d, OFFSET: %d\n", mensajeAMemoriaLeer->nroMarco, mensajeAMemoriaLeer->desplazamiento);

			t_paqt paqueteMemoria;
			recibirMsje(conexion, &paqueteMemoria);
			MSJ_INT *mensajeValorLeido = malloc(sizeof(MSJ_INT));
			mensajeValorLeido = paqueteMemoria.mensaje;

			asignarValorARegistro(pcb, insActual->paramReg[0], mensajeValorLeido->numero);
			uint32_t registroActual = matchearRegistro(pcb->registros, insActual->paramReg[0]);

			log_debug(logger, "Mensaje leido: %d", mensajeValorLeido->numero);
			log_debug(logger, "Registro %s = %d", registro, registroActual);

			free(dirFisicaMoveIn);
			free(mensajeAMemoriaLeer);
			free(mensajeValorLeido);
			free(registro);
			free(registro2);
			free(io);
		}
		break;

	case MOV_OUT:
		printf(PRINT_COLOR_CYAN "\nEjecutando instruccion MOV_OUT - Etapa Execute \n" PRINT_COLOR_CYAN);
		// MOV_OUT (Dirección Lógica, Registro): Lee el valor del Registro y lo escribe en la dirección física de
		// memoria del segmento de Datos obtenida a partir de la Dirección Lógica.

		log_debug(logger, "MOV_OUT %d %s", insActual->paramInt, registro);

		uint32_t registroActual = matchearRegistro(pcb->registros, insActual->paramReg[0]);
		log_info(logger, "%s = %d", registro, registroActual); // devuelve el valor que tiene dentro el registro

		t_direccionFisica *dirFisicaMoveOut = malloc(sizeof(t_direccionFisica));
		dirFisicaMoveOut = calcular_direccion_fisica(insActual->paramInt, configCPU.cantidadEntradasPorTabla, configCPU.tamanioPagina, pcb);
		if (dirFisicaMoveOut->nroMarco == -1)
		{
			retornePCB = true;
			log_info(logger, "se envio info a kernel por page fault");
		}
		else if (dirFisicaMoveOut->nroMarco == -10)
		{
			log_info(logger, "Kernel finaliza el proceso por Segmentation Fault");
		}
		else
		{

			log_debug(logger, "dir fisica: nroMarco= %d offset=%d", dirFisicaMoveOut->nroMarco, dirFisicaMoveOut->desplazamientoPagina);

			MSJ_MEMORIA_CPU_ESCRIBIR *mensajeAMemoriaEscribir = malloc(sizeof(MSJ_MEMORIA_CPU_ESCRIBIR));
			mensajeAMemoriaEscribir->desplazamiento = dirFisicaMoveOut->desplazamientoPagina;
			mensajeAMemoriaEscribir->nroMarco = dirFisicaMoveOut->nroMarco;
			mensajeAMemoriaEscribir->valorAEscribir = registroActual;
			mensajeAMemoriaEscribir->pid = pcb->id;

			log_debug(logger, "valor a escribir = %i", registroActual);

			enviarMsje(conexion, CPU, mensajeAMemoriaEscribir, sizeof(MSJ_MEMORIA_CPU_ESCRIBIR), ACCESO_MEMORIA_ESCRIBIR);
			log_info(loggerMinimo, "Acceso Memoria: PID: %d - Acción: ESCRIBIR - Segmento: %d - Pagina: %d - Dirección Fisica: %d %d", pcb->id, dirFisicaMoveOut->dl.nroSegmento, dirFisicaMoveOut->dl.nroPagina, mensajeAMemoriaEscribir->nroMarco, mensajeAMemoriaEscribir->desplazamiento);

			t_paqt paqueteMemoriaWrite;
			recibirMsje(conexion, &paqueteMemoriaWrite);

			char *mensajeWrite = string_new();
			string_append(&mensajeWrite, paqueteMemoriaWrite.mensaje);

			log_debug(logger, "Mensaje escrito: %s", mensajeWrite);

			free(mensajeWrite);
			free(dirFisicaMoveOut);
			free(mensajeAMemoriaEscribir);
			free(registro);
			free(registro2);
			free(io);
		}
		break;

	case IO:
		printf(PRINT_COLOR_CYAN "\nEjecutando instruccion IO - Etapa Execute \n" PRINT_COLOR_CYAN);
		// pcb->program_counter += 1;
		if (strcmp(insActual->paramIO, "TECLADO") == 0)
		{
			// enviar mensaje a kernel. priero envio el pcb
			// cargarlos valores del mensaje
			/*t_io_dispositivo* mensajeAKernel;
			mensajeAKernel->dispositivo;
			mensajeAKernel->registro;
			enviarMsje(conexionDispatch, CPU, mensajeAKernel,sizeof(t_io_dispositivo) ,BLOCK_PCB_IO_TECLADO);*/
			serializarPCB(socketAceptadoDispatch, pcb, BLOCK_PCB_IO_TECLADO);
			log_debug(logger, "Envie BLOCK al kernel por IO_TECLADO");
			retornePCB = true;
		}
		else if (strcmp(insActual->paramIO, "PANTALLA") == 0)
		{
			// enviar mensaje a kernel. priero envio el pcb
			// cargarlos valores del mensaje
			/*t_io_dispositivo* mensajeAKernel;
			mensajeAKernel->dispositivo;
			mensajeAKernel->registro;
			enviarMsje(conexionDispatch, CPU, mensajeAKernel,sizeof(t_io_dispositivo) ,BLOCK_PCB_IO_TECLADO);*/
			serializarPCB(socketAceptadoDispatch, pcb, BLOCK_PCB_IO_PANTALLA);
			log_debug(logger, "Envie BLOCK al kernel por IO_PANTALLA");
			retornePCB = true;
		}
		else
		{
			// enviar mensaje a kernel , antes enviar el pcb
			// cargarlos valores del mensaje
			/*t_io_dispositivo_kernel* mensajeAKernel;
			mensajeAKernel->dispositivo;
			mensajeAKernel->tiempo;
			enviarMsje(conexionDispatch, CPU, mensajeAKernel,sizeof(t_io_dispositivo_kernel) ,BLOCK_PCB_IO_TECLADO);*/
			serializarPCB(socketAceptadoDispatch, pcb, BLOCK_PCB_IO);
			log_debug(logger, "Envie BLOCK al kernel por IO");
			retornePCB = true;
		}

		free(pcb);
		free(registro);
		free(registro2);
		free(io);
		break;

	case EXIT:
		printf(PRINT_COLOR_CYAN "\nEjecutando instruccion EXIT - Etapa Execute\n" PRINT_COLOR_CYAN);
		serializarPCB(socketAceptadoDispatch, pcb, EXIT_PCB);
		log_debug(logger, "Envie EXIT al kernel");
		retornePCB = true;
		free(pcb);
		free(registro);
		free(registro2);
		free(io);
		log_info(logger, "\nLlegue al retorno: %d\n", retornePCB);

		if (habilitarTLB == 1)
		{
			limpiar_entradas_TLB();
		}
		break;
	default:
		break;
	}
	log_info(logger, "\nretornePCB%d\n", retornePCB);
	return retornePCB;
}

void fetch(t_pcb *pcb)
{

	uint32_t index = pcb->program_counter;
	pcb->program_counter += 1;

	log_info(logger, "insActual->pc: %i", index);
	log_info(logger, " Valor nuevo Program counter: %i", pcb->program_counter);
}

void checkInterrupt(t_pcb *pcb, bool retornePCB)
{

	if (interrupciones && !retornePCB)
	{
		// devuelvo pcb a kernel
		log_debug(logger, "Devuelvo pcb por interrupcion");
		serializarPCB(socketAceptadoDispatch, pcb, INTERRUPT_INTERRUPCION);
		retornePCB = true;
		// interrupciones = false;
		free(pcb);
		// limpiar_entradas_TLB();
	}
	else
	{
		log_debug(logger, "No hay interrupcion, sigo el ciclo");
	}
}

char *registroToString(t_registro registroCPU)
{
	switch (registroCPU)
	{
	case AX:
		return "AX";
		break;
	case BX:
		return "BX";
		break;
	case CX:
		return "CX";
		break;
	case DX:
		return "DX";
		break;
	default:
		return "";
		break;
	}
}

char *instruccionToString(t_instCode codigoInstruccion)
{
	char *string = string_new();
	switch (codigoInstruccion)
	{
	case SET:
		string_append(&string, "SET");
		return string;
		break;
	case ADD:
		string_append(&string, "ADD");
		return string;
		break;
	case MOV_IN:
		string_append(&string, "MOV_IN");
		return string;
		break;
	case MOV_OUT:
		string_append(&string, "MOV_OUT");
		return string;
		break;
	case IO:
		string_append(&string, "IO");
		return string;
		break;
	case EXIT:
		string_append(&string, "EXIT");
		return string;
		break;

	default:
		break;
	}
}

uint32_t matchearRegistro(t_registros registros, t_registro registro)
{
	switch (registro)
	{
	case AX:
		return registros.AX;
		break;
	case BX:
		return registros.BX;
		break;
	case CX:
		return registros.CX;
		break;
	case DX:
		return registros.DX;
		break;

	default:
		break;
	}
}

void asignarValorARegistro(t_pcb *pcb, t_registro registro, uint32_t valor)
{
	switch (registro)
	{
	case AX:
		pcb->registros.AX = valor;
		break;
	case BX:
		pcb->registros.BX = valor;
		break;
	case CX:
		pcb->registros.CX = valor;
		break;
	case DX:
		pcb->registros.DX = valor;
	default:
		break;
	}
}

/************** Traduccion */
// tam_max_segmento = cant_entradas_por_tabla * tam_pagina
// num_segmento = floor(dir_logica / tam_max_segmento)
// desplazamiento_segmento = dir_logica % tam_max_segmento
// num_pagina = floor(desplazamiento_segmento  / tam_pagina)
// desplazamiento_pagina = desplazamiento_segmento % tam_pagina

t_direccionFisica *calcular_direccion_fisica(int direccionLogica, int cant_entradas_por_tabla, int tam_pagina, t_pcb *pcb)
{

	printf(PRINT_COLOR_GREEN "\n----------------------------------------------------------------------------" PRINT_COLOR_RESET);
	log_info(logger, "MMU entrando en acción...");
	log_info(logger, "Traduccion de la dirección logica");
	log_info(logger, "direccionLogica: %d", direccionLogica);
	t_direccionFisica *dir_fisica = malloc(sizeof(t_direccionFisica));

	int tamanio_maximo_segmento = tamanioMaximoPorSegmento(cant_entradas_por_tabla, tam_pagina);
	log_info(logger, "Tamanio Maximo Por Segmento = %d * %d = %d", cant_entradas_por_tabla, tam_pagina, tamanio_maximo_segmento);

	int numero_segmento = numeroDeSegmento(direccionLogica, tamanio_maximo_segmento);
	log_info(logger, "Número de Segmento = %d / %d = %d", direccionLogica, tamanio_maximo_segmento, numero_segmento);

	int desplazamiento_Segmento = desplazamientoSegmento(direccionLogica, tamanio_maximo_segmento);
	log_info(logger, "Desplazamiento Segmento = %d ·/. %d = %d", direccionLogica, tamanio_maximo_segmento, desplazamiento_Segmento);

	int numero_pagina = numeroPagina(desplazamiento_Segmento, tam_pagina); // numero_pagina CAMBIAR A UINT_32 ??? o esta bien en int?? la misma pregunta por todos los demas int
	log_info(logger, "Número Pagina = %d / %d = %d", desplazamiento_Segmento, tam_pagina, numero_pagina);

	int desplazamiento_pagina = desplazamientoPagina(desplazamiento_Segmento, tam_pagina);
	log_info(logger, "Desplazamiento Pagina = %d ·/. %d = %d", desplazamiento_Segmento, tam_pagina, desplazamiento_pagina);

	printf(PRINT_COLOR_GREEN "\n----------------------------------------------------------------------------" PRINT_COLOR_RESET);

	dir_fisica->dl.nroPagina = numero_pagina;
	dir_fisica->dl.nroSegmento = numero_segmento;

	t_tabla_segmentos *segmento = list_get(pcb->tablaSegmentos, numero_segmento);
	// segmento = list_get(pcb->tablaSegmentos, numero_segmento);
	log_info(logger, "El id del segmento es: %d\n", segmento->id);

	log_info(logger, "El id de la tabla es: %d\n", segmento->indiceTablaPaginas);
	int nroMarco = -1;

	// 1ero Chequear SEGMENTATION FAULT
	printf(PRINT_COLOR_MAGENTA "Chequeando que no haya SEGMENTATION FAULT \n" PRINT_COLOR_RESET);
	log_info(logger, "desplazamiento_Segmento:%d > segmento->tamanio: %d ???\n", desplazamiento_Segmento, segmento->tamanio);

	if (desplazamiento_Segmento >= segmento->tamanio)
	{ // Uso el tamanio real
		// Devolvemos el pcb a nuestro bello kernel
		serializarPCB(socketAceptadoDispatch, pcb, SEGMENTATION_FAULT);
		log_info(loggerMinimo, "SEGMENTATION FAULT: devuelvo el proceso a kernel para ser finalizado...");
		retornePCB = true;
		dir_fisica->nroMarco = -10;
		return dir_fisica;
	}

	if (habilitarTLB == 1)
	{
		nroMarco = buscar_en_TLB(numero_pagina, numero_segmento, pcb->id);
	}

	if (nroMarco != -1)
	{ // significa que La PAGINA ESTA EN LA TLB
		// Direccion fisica = Numero de marco * tamaño de marco + offset
		// dirFisica = malloc(sizeof(t_direccionFisica));
		dir_fisica->nroMarco = nroMarco;
		dir_fisica->desplazamientoPagina = desplazamiento_pagina;
	}
	else if (nroMarco == -1)
	{ // COMO LA PAG NO ESTA EN LA TLB, TRADUCIR DIR CON MMU -> TLB MISS

		int respuestaMemoriaPrimerAcceso = primer_acceso(numero_pagina, segmento->indiceTablaPaginas, pcb->id);
		if (respuestaMemoriaPrimerAcceso == -1)
		{ // respuesta PAGE FAULT
			dir_fisica->nroMarco = respuestaMemoriaPrimerAcceso;
			// Devolvemos el pcb a nuestro bello kernel
			MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT *mensajeAKernelPageFault = malloc(sizeof(MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT));
			mensajeAKernelPageFault->nro_pagina = numero_pagina;
			mensajeAKernelPageFault->nro_segmento = numero_segmento;
			pcb->program_counter--;

			// mensajeAKernelPageFault->pcb =pcb;

			serializarPCB(socketAceptadoDispatch, pcb, BLOCK_PCB_PAGE_FAULT);
			enviarMsje(socketAceptadoDispatch, CPU, mensajeAKernelPageFault, sizeof(MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT), BLOCK_PCB_PAGE_FAULT);
			// log_debug(logger,"Page Fault PID: %d - Segmento: %d - Pagina: %d",pcb->id,numero_segmento,numero_pagina);
			// Page Fault
			log_info(loggerMinimo, "Page Fault PID: %d - Segmento: %d - Pagina: %d", pcb->id, numero_segmento, numero_pagina);

			log_debug(logger, "Envie de Nuevo el proceso a Kernel sin actualizar Program Counter (para bloquear por PAGE FAULT)");
			// free(pcb);
			free(mensajeAKernelPageFault);
		}
		else
		{
			dir_fisica->nroMarco = respuestaMemoriaPrimerAcceso;
			dir_fisica->desplazamientoPagina = desplazamiento_pagina; // Checkear

			log_debug(logger, "El valor del marco es: %d", dir_fisica->nroMarco);
			log_debug(logger, "El valor del offset es: %d", dir_fisica->desplazamientoPagina);

			if (habilitarTLB == 1)
			{
				actualizar_TLB(numero_pagina, dir_fisica->nroMarco, numero_segmento, pcb->id);
			}
		}
	}

	return dir_fisica;
}


int tamanioMaximoPorSegmento(int cant_entradas_por_tabla, int tam_pagina)
{
	int tam_max_segmento = cant_entradas_por_tabla * tam_pagina;
	return tam_max_segmento;
}

int numeroDeSegmento(int dir_logica, int tam_max_segmento)
{
	int num_segmento = floor(dir_logica / tam_max_segmento);
	return num_segmento;
}

int desplazamientoSegmento(int dir_logica, int tam_max_segmento)
{
	int desplazamiento_segmento = dir_logica % tam_max_segmento;
	return desplazamiento_segmento;
}

int numeroPagina(int desplazamiento_segmento, int tam_pagina)
{
	int num_pagina = floor(desplazamiento_segmento / tam_pagina);
	return num_pagina;
}

int desplazamientoPagina(int desplazamiento_segmento, int tam_pagina)
{
	int desplazamiento_pagina = desplazamiento_segmento % tam_pagina;
	return desplazamiento_pagina;
}

int primer_acceso(int numero_pagina, uint32_t indiceTablaPaginas, uint32_t pid)
{
	MSJ_MEMORIA_CPU_ACCESO_TABLA_DE_PAGINAS *mensajeAMemoriaAccesoTP = malloc(sizeof(MSJ_MEMORIA_CPU_ACCESO_TABLA_DE_PAGINAS));
	mensajeAMemoriaAccesoTP->idTablaDePaginas = indiceTablaPaginas;
	mensajeAMemoriaAccesoTP->pagina = numero_pagina;
	mensajeAMemoriaAccesoTP->pid = pid;
	enviarMsje(conexion, CPU, mensajeAMemoriaAccesoTP, sizeof(MSJ_MEMORIA_CPU_ACCESO_TABLA_DE_PAGINAS), ACCESO_MEMORIA_TABLA_DE_PAG);
	free(mensajeAMemoriaAccesoTP);

	log_debug(logger, "Envie mensaje a memoria para acceder a Tabla de Paginas con Indice: %d", indiceTablaPaginas);

	t_paqt paqueteMemoria;
	recibirMsje(conexion, &paqueteMemoria);
	MSJ_INT *mensajePrimerAcceso = malloc(sizeof(MSJ_INT));
	mensajePrimerAcceso = paqueteMemoria.mensaje;
	switch (paqueteMemoria.header.tipoMensaje)
	{
	case PAGE_FAULT:
		return mensajePrimerAcceso->numero;
		break;
	case RESPUESTA_MEMORIA_MARCO_BUSCADO:
		int nroFrame = mensajePrimerAcceso->numero;
		log_info(logger, "(primer acceso) EL MARCO BUSCADO ES: %d", nroFrame);
		free(mensajePrimerAcceso);
		return nroFrame;
		break;

	default:
		break;
	}
}

/*----------------------TLB------------------------------*/

void inicializarTLB()
{

	int cantidadEntradasTLB = configCPU.entradasTLB;

	if (cantidadEntradasTLB == 0)
	{
		habilitarTLB = 0;
		return;
	}
	habilitarTLB = 1;

	TLB = malloc(sizeof(tlb));
	TLB->cant_entradas = cantidadEntradasTLB;
	TLB->entradas = list_create();
	TLB->algoritmo = configCPU.reemplazoTLB; // FIFO o LRU
}

int tlbTieneEntradasLibres()
{ // Podria ser un Boole
	return TLB->cant_entradas > TLB->entradas->elements_count;
}

// En este caso, la TLB tiene una o mas entradas libres // Revisar
void llenar_TLB(int nroPagina, int nroFrame, int nroSegmento, int pid)
{
	entrada_tlb *entrada = malloc(sizeof(entrada_tlb));
	entrada->nroPagina = nroPagina;
	entrada->nroFrame = nroFrame;
	entrada->nroSegmento = nroSegmento;
	entrada->pid = pid;
	entrada->ultimaReferencia = obtenerMomentoActual();
	entrada->instanteDeCarga = obtenerMomentoActual();

	list_add(TLB->entradas, entrada);

	char *tiempo = calcularHorasMinutosSegundos(entrada->ultimaReferencia);

	printf(PRINT_COLOR_MAGENTA "----------SE MODIFICA LA TLB------" PRINT_COLOR_RESET);
	printf(PRINT_COLOR_MAGENTA "Se llena la entrada de TLB con: PID: %d , Nro pagina: %d, Nro Frame: %d, Nro Segmento: %d, instante de Referencia: %s , instante de Carga %s \n" PRINT_COLOR_RESET, entrada->pid, entrada->nroPagina, entrada->nroFrame, entrada->nroSegmento, tiempo, tiempo);
	free(tiempo);
}

int buscar_en_TLB(int nroPagina, int nroSegmento, int pid)
{ // int nroSegmento, int pid //devuelve numero de frame, si esta en la tlb, devuelve -1 si no esta en la tlb
	entrada_tlb *entradaActual;
	for (int i = 0; i < TLB->entradas->elements_count; i++)
	{
		entradaActual = list_get(TLB->entradas, i);
		if (entradaActual->nroPagina == nroPagina && entradaActual->nroSegmento == nroSegmento && entradaActual->pid == pid)
		{

			// log_debug(logger, "TLB Hit: PID: %i - TLB HIT - Segmento: %i - Pagina: %i \n", entradaActual->pid, entradaActual->nroSegmento, entradaActual->nroPagina);
			// Tlb Hit
			log_info(loggerMinimo, "TLB Hit: PID: %i - TLB HIT - Segmento: %i - Pagina: %i \n", entradaActual->pid, entradaActual->nroSegmento, entradaActual->nroPagina);

			printf(PRINT_COLOR_MAGENTA "TLB Hit: PID: %d TLB HIT - Segmento: %d - Pagina: %d - Frame: %d \n" PRINT_COLOR_RESET, entradaActual->pid, entradaActual->nroSegmento, entradaActual->nroPagina, entradaActual->nroFrame);

			log_info(logger, "----------------Actualizando ultima referencia--------------------");

			char *tiempoAnterior = calcularHorasMinutosSegundos(entradaActual->ultimaReferencia);
			char *instanteDeCarga = calcularHorasMinutosSegundos(entradaActual->instanteDeCarga);
			log_info(logger, "Referencia anterior:");
			log_debug(logger, "PID: %i - TLB HIT - Segmento: %i - Pagina: %i, Tiempo de Referencia: %s , Instante de Carga: %s \n", entradaActual->pid, entradaActual->nroSegmento, entradaActual->nroPagina, tiempoAnterior, instanteDeCarga);
			log_info(logger, "Referencia nueva:");
			entradaActual->ultimaReferencia = obtenerMomentoActual();
			char *tiempoNuevo = calcularHorasMinutosSegundos(entradaActual->ultimaReferencia);
			log_info(loggerMinimo, "PID: %i - TLB HIT - Segmento: %i - Pagina: %i, Tiempo de Referencia: %s, Instante de Carga: %s \n", entradaActual->pid, entradaActual->nroSegmento, entradaActual->nroPagina, tiempoNuevo, instanteDeCarga);

			log_info(logger, "------------------------------------------------------------------");

			imprimirModificacionTlb();

			return entradaActual->nroFrame;
		}
	}

	// TLB Miss: “PID: <PID> - TLB MISS - Segmento: <NUMERO_SEGMENTO> - Pagina: <NUMERO_PAGINA>”

	log_debug(logger, "TLB MISS - pagina no encontrada en TLB\n");

	log_info(loggerMinimo, "TLB Miss: PID: %i - TLB MISS - Segmento: %i - Pagina: %i \n", pid, nroSegmento, nroPagina);

	return -1;
}

int obtenerMomentoActual()
{
	return time(NULL);
}

void actualizar_TLB(int nroPagina, int nroFrame, int nroSegmento, int pid)
{

	bool coincideMarcoYpid(void *_entrada)
	{ // Parece funcionar bien, probar con varios casos;

		entrada_tlb *entrada = (entrada_tlb *)_entrada;
		return (entrada->nroFrame == nroFrame && entrada->pid == pid);
	};

	if (list_find(TLB->entradas, &coincideMarcoYpid))
	{

		printf("---------------------------------------------------------------------------------------------------------------------------------------------------\n");

		log_info(logger, "El marco %d  del PID: %d ya se encontraba en TLB pero asociado a otra pagina y segmento.\t Borrando entrada desactualizada.", nroFrame, pid);
		list_remove_and_destroy_by_condition(TLB->entradas, &coincideMarcoYpid, &free);
	}

	if (tlbTieneEntradasLibres())
	{
		llenar_TLB(nroPagina, nroFrame, nroSegmento, pid);
		imprimirModificacionTlb();
		return;
	}
	else
	{
		log_debug(logger, "La TLB ya no tiene mas entradas disponilbles");
		log_debug(logger, "Algoritmos de reemplazo entrando en acción");

		usarAlgoritmosDeReemplazoTlb(nroPagina, nroFrame, nroSegmento, pid);
	}
}

void usarAlgoritmosDeReemplazoTlb(int nroPagina, int nroFrame, int nroSegmento, int pid)
{

	// REEMPLAZO
	if (strcmp(TLB->algoritmo, "LRU") == 0)
	{
		reemplazo_algoritmo_lru(nroPagina, nroFrame, nroSegmento, pid);
		printf(PRINT_COLOR_MAGENTA "----------ACTUALIZACION DE TLB----------" PRINT_COLOR_RESET);
		entradaConMenorTiempoDeReferencia();
		imprimirModificacionTlb();
	}
	else
	{
		reemplazo_algoritmo_fifo(nroPagina, nroFrame, nroSegmento, pid);
		printf(PRINT_COLOR_MAGENTA "----------ACTUALIZACION DE TLB----------" PRINT_COLOR_RESET);
		imprimirModificacionTlb();
	}
}

// Modificación de la TLB -> Imprimir todas las entradas de la TLB ordenadas por número de entrada
void imprimirModificacionTlb()
{
	entrada_tlb *entrada;
	for (int i = 0; i < TLB->entradas->elements_count; i++)
	{
		entrada = list_get(TLB->entradas, i);
		char *tiempo = calcularHorasMinutosSegundos(entrada->ultimaReferencia);
		char *instanteDeCarga = calcularHorasMinutosSegundos(entrada->instanteDeCarga);

		log_info(loggerMinimo, " %i | PID: %i | SEGMENTO: %i | PAGINA: %i  | MARCO: %i | TIEMPO DE ULTIMA REFERENCIA: %s | INSTANTE DE CARGA: %s \n", i, entrada->pid, entrada->nroSegmento, entrada->nroPagina, entrada->nroFrame, tiempo, instanteDeCarga);
		free(tiempo);
		free(instanteDeCarga);
	}
	// entradaConMenorTiempoDeReferencia();
	printf("--------------------------------------------------------------------------------------------------------------------------------\n");
}

char *calcularHorasMinutosSegundos(int valor)
{

	int horas = (valor / 3600);
	int minutos = ((valor - horas * 3600) / 60);
	int segundos = valor - (horas * 3600 + minutos * 60);

	char *formato = string_new();
	string_append_with_format(&formato, "min:%d seg:%d", minutos, segundos);
	return formato;
}

int entradaConMenorTiempoDeReferencia()
{
	void *esMenor(void *_unaEntrada, void *_otraEntrada)
	{

		entrada_tlb *unaEntrada = (entrada_tlb *)_unaEntrada;
		entrada_tlb *otraEntrada = (entrada_tlb *)_otraEntrada;

		if (unaEntrada->ultimaReferencia <= otraEntrada->ultimaReferencia)
		{

			return unaEntrada;
		}
		else
		{

			return otraEntrada;
		}
	}

	entrada_tlb *entradaVictima = list_get_minimum(TLB->entradas, &esMenor);

	char *tiempo = calcularHorasMinutosSegundos(entradaVictima->ultimaReferencia);

	int posicion;

	for (int i = 0; i < configCPU.entradasTLB; i++)
	{
		entrada_tlb *entradaAuxiliar = list_get(TLB->entradas, i);

		if (entradaVictima->pid == entradaAuxiliar->pid && entradaVictima->nroSegmento == entradaAuxiliar->nroSegmento && entradaVictima->nroPagina == entradaAuxiliar->nroPagina && entradaVictima->nroFrame == entradaAuxiliar->nroFrame)
		{
			posicion = i;
			break;
		}
	}
	printf(PRINT_COLOR_MAGENTA "ENTRADA VICTIMA:PID Entrada con menor tiempo de referencia: %d, Tiempo De Referencia: %s\n" PRINT_COLOR_RESET, entradaVictima->pid, tiempo);
	free(tiempo);
	return posicion;
}

int entradaConMenorInstanteDeCarga()
{
	void *esMenor(void *_unaEntrada, void *_otraEntrada)
	{

		entrada_tlb *unaEntrada = (entrada_tlb *)_unaEntrada;
		entrada_tlb *otraEntrada = (entrada_tlb *)_otraEntrada;

		if (unaEntrada->instanteDeCarga <= otraEntrada->instanteDeCarga)
		{

			return unaEntrada;
		}
		else
		{

			return otraEntrada;
		}
	}

	entrada_tlb *entradaVictima = list_get_minimum(TLB->entradas, &esMenor);

	char *tiempo = calcularHorasMinutosSegundos(entradaVictima->instanteDeCarga);

	int posicion;

	for (int i = 0; i < configCPU.entradasTLB; i++)
	{
		entrada_tlb *entradaAuxiliar = list_get(TLB->entradas, i);

		if (entradaVictima->pid == entradaAuxiliar->pid && entradaVictima->nroSegmento == entradaAuxiliar->nroSegmento && entradaVictima->nroPagina == entradaAuxiliar->nroPagina && entradaVictima->nroFrame == entradaAuxiliar->nroFrame)
		{
			posicion = i;
			break;
		}
	}
	printf(PRINT_COLOR_MAGENTA "ENTRADA VICTIMA:PID Entrada con menor tiempo de carga: %d, Tiempo De Carga: %s\n" PRINT_COLOR_RESET, entradaVictima->pid, tiempo);
	free(tiempo);
	return posicion;
}

void reemplazo_algoritmo_lru(int nroPagina, int nroFrame, int nroSegmento, int pid)
{
	printf(PRINT_COLOR_MAGENTA "Reemplazo por algoritmo LRU" PRINT_COLOR_RESET);

	int posicionVictima = entradaConMenorTiempoDeReferencia();

	entrada_tlb *nuevaEntrada = malloc(sizeof(entrada_tlb));

	nuevaEntrada->nroPagina = nroPagina;
	nuevaEntrada->nroFrame = nroFrame;
	nuevaEntrada->nroSegmento = nroSegmento;
	nuevaEntrada->pid = pid;
	nuevaEntrada->ultimaReferencia = obtenerMomentoActual();
	nuevaEntrada->instanteDeCarga = obtenerMomentoActual();

	entrada_tlb *entradaVictima = list_replace(TLB->entradas, posicionVictima, nuevaEntrada);

	char *tiempoVictima = calcularHorasMinutosSegundos(entradaVictima->ultimaReferencia);
	char *tiempoNuevo = calcularHorasMinutosSegundos(nuevaEntrada->ultimaReferencia);
	// log_warning(logger, "Reemplaza pagina: %d por nueva pagina %d", entradaVictima->nroPagina, nuevaEntrada->nroPagina);

	log_warning(logger, "Entrada anterior: | PID: %i | SEGMENTO: %i | PAGINA: %i  | MARCO: %i | TIEMPO: %s\n", entradaVictima->pid, entradaVictima->nroSegmento, entradaVictima->nroPagina, entradaVictima->nroFrame, tiempoVictima);
	log_warning(logger, "Entrada nueva: | PID: %i | SEGMENTO: %i | PAGINA: %i  | MARCO: %i | TIEMPO: %s\n ", nuevaEntrada->pid, nuevaEntrada->nroSegmento, nuevaEntrada->nroPagina, nuevaEntrada->nroFrame, tiempoNuevo);

	// entradaVictima = list_remove(TLB->entradas, entradaVictima);
}

void reemplazo_algoritmo_fifo(int nroPagina, int nroFrame, int nroSegmento, int pid)
{
	printf(PRINT_COLOR_MAGENTA "Reemplazo por algoritmo FIFO" PRINT_COLOR_RESET);
	int posicionVictima = entradaConMenorInstanteDeCarga();
	entrada_tlb *entradaNueva = malloc(sizeof(entrada_tlb));

	entradaNueva->nroPagina = nroPagina;
	entradaNueva->nroFrame = nroFrame;
	entradaNueva->nroSegmento = nroSegmento;
	entradaNueva->pid = pid;
	entradaNueva->ultimaReferencia = obtenerMomentoActual();
	entradaNueva->instanteDeCarga = obtenerMomentoActual();

	entrada_tlb *entradaVictima = list_replace(TLB->entradas, posicionVictima, entradaNueva); // Selecciono la primer entrada de la lista de entradas

	// log_warning(logger, "Reemplazo de pagina: %d por nueva pagina %d", entradaAReemplazar->nroPagina, entradaNueva->nroPagina);
	// printf(PRINT_COLOR_YELLOW "Reemplazo de pagina: %d por nueva pagina %d" PRINT_COLOR_RESET, entradaAReemplazar->nroPagina, entradaNueva->nroPagina);

	char *tiempoVictima = calcularHorasMinutosSegundos(entradaVictima->ultimaReferencia);
	char *tiempoNuevo = calcularHorasMinutosSegundos(entradaNueva->ultimaReferencia);
	char *instanteVictima = calcularHorasMinutosSegundos(entradaVictima->instanteDeCarga);
	char *instanteNuevo = calcularHorasMinutosSegundos(entradaNueva->instanteDeCarga);

	log_warning(logger, "Entrada anterior: | PID: %i | SEGMENTO: %i | PAGINA: %i  | MARCO: %i | TIEMPO DE REFERENCIA: %s | INSTANTE DE CARGA: %s \n", entradaVictima->pid, entradaVictima->nroSegmento, entradaVictima->nroPagina, entradaVictima->nroFrame, tiempoVictima, instanteVictima);
	log_warning(logger, "Entrada nueva: | PID: %i | SEGMENTO: %i | PAGINA: %i  | MARCO: %i | TIEMPO DE REFERENCIA: %s | INSTANTE DE CARGA: %s\n ", entradaNueva->pid, entradaNueva->nroSegmento, entradaNueva->nroPagina, entradaNueva->nroFrame, tiempoNuevo, instanteNuevo);

	// list_remove(TLB->entradas, 0);					   // Elimino la entrada victima
	// list_add_in_index(TLB->entradas, 0, entradaNueva); // Agrego la nueva entrada en la primera posicion de la lista
	// free(entradaAReemplazar);
}

// Freedoooom(?

void destruir_entrada(void *entrada)
{
	entrada_tlb *entradaTlb = (entrada_tlb *)entrada;
	free(entradaTlb);
}

void limpiar_entradas_TLB()
{
	list_clean_and_destroy_elements(TLB->entradas, destruir_entrada);
}

void cerrar_TLB()
{

	entrada_tlb *entradaActual;
	// Va destruyendo todas las entradas
	for (int i = 0; i < TLB->entradas->elements_count; i++)
	{
		entradaActual = list_get(TLB->entradas, i);
		list_remove_and_destroy_element(TLB->entradas, i, destruir_entrada);
	}
	free(TLB->algoritmo);
	free(TLB);
}