#include "memoria.h"

int main(int argc, char **argv)
{

	// Parte Server
	config = iniciar_config("memoria.config");
	logger = iniciar_logger("memoria.log", "MEMORIA", LOG_LEVEL_DEBUG);
	loggerMinimo = iniciar_logger("memoriaLoggsObligatorios.log", "MEMORIA", LOG_LEVEL_INFO);

	// creo el struct

	extraerDatosConfig(config);

	swap = abrirArchivo(configMemoria.pathSwap);
	// ftruncate(swap, configMemoria.tamanioSwap);
	truncate(configMemoria.pathSwap, configMemoria.tamanioSwap);
	// agregar_tabla_pag_en_swap();

	memoriaRAM = malloc(configMemoria.tamMemoria);

	iniciar_listas_y_semaforos(); // despues ver porque kernel tambien lo utiliza y por ahi lo esta pisando, despues ver si lo dejamos solo aca
	tamanio = configMemoria.tamMemoria / configMemoria.tamPagina;
	tamanioSgtePagina = 0;

	inicializar_bitmap();
	// printf("\n%d",bitmap_marco[0].uso);
	contadorIdTablaPag = 0;

	crear_hilos_memoria();

	log_destroy(logger);

	config_destroy(config);

	fclose(swap);

	// crear una lista con el tamaño de los marcos/segmanetos para ir guardado y remplazando
	// en el caso de que esten ocupados , con los algoritmos correcpondientes

	// en elarchivo swap se van a guardar las tablas enteras que voy a leer segun en los bytes que esten
	// lo voy a buscar con el fseeck y ahi agregar , reemplazar , los tatos quedan ahi es como disco
}

t_configMemoria extraerDatosConfig(t_config *archivoConfig)
{
	configMemoria.puertoEscuchaUno = string_new();
	configMemoria.puertoEscuchaDos = string_new();
	configMemoria.algoritmoReemplazo = string_new();
	configMemoria.pathSwap = string_new();

	configMemoria.puertoEscuchaUno = config_get_string_value(archivoConfig, "PUERTO_ESCUCHA_UNO");
	configMemoria.puertoEscuchaDos = config_get_string_value(archivoConfig, "PUERTO_ESCUCHA_DOS");
	configMemoria.retardoMemoria = config_get_int_value(archivoConfig, "RETARDO_MEMORIA");
	configMemoria.algoritmoReemplazo = config_get_string_value(archivoConfig, "ALGORITMO_REEMPLAZO");
	configMemoria.pathSwap = config_get_string_value(archivoConfig, "PATH_SWAP");

	configMemoria.tamMemoria = config_get_int_value(archivoConfig, "TAM_MEMORIA");
	configMemoria.tamPagina = config_get_int_value(archivoConfig, "TAM_PAGINA");
	configMemoria.entradasPorTabla = config_get_int_value(archivoConfig, "ENTRADAS_POR_TABLA");
	configMemoria.marcosPorProceso = config_get_int_value(archivoConfig, "MARCOS_POR_PROCESO");
	configMemoria.retardoSwap = config_get_int_value(archivoConfig, "RETARDO_SWAP");
	configMemoria.tamanioSwap = config_get_int_value(archivoConfig, "TAMANIO_SWAP");

	return configMemoria;
}

void crear_hilos_memoria()
{
	pthread_t thrKernel, thrCpu;

	pthread_create(&thrKernel, NULL, (void *)iniciar_servidor_hacia_kernel, NULL);
	pthread_create(&thrCpu, NULL, (void *)iniciar_servidor_hacia_cpu, NULL);

	pthread_detach(thrKernel);
	pthread_join(thrCpu, NULL);
}

void iniciar_servidor_hacia_kernel()
{
	int server_fd = iniciar_servidor(IP_SERVER, configMemoria.puertoEscuchaUno);
	log_info(logger, "Servidor listo para recibir al kernel");
	socketAceptadoKernel = esperar_cliente(server_fd);
	char *mensaje = recibirMensaje(socketAceptadoKernel);

	log_info(logger, "Mensaje de confirmacion del Kernel: %s\n", mensaje);

	while (1)
	{
		log_info(logger, "Estoy esperando paquete, soy memoria\n");
		t_paqueteActual *paquete = recibirPaquete(socketAceptadoKernel);
		//  printf("\nRecibi el paquete del kernel%d\n", paquete->codigo_operacion);
		t_pcb *pcb = deserializoPCB(paquete->buffer);

		switch (paquete->codigo_operacion)
		{
		case ASIGNAR_RECURSOS:

			log_info(logger, "Mi cod de op es: %d", paquete->codigo_operacion);
			// pthread_t thrTablaPaginasCrear;
			// printf("\nEntro a asignar recursos\n");
			crearTablasPaginas(pcb);
			// pthread_create(&thrTablaPaginasCrear, NULL, (void *)crearTablasPaginas, (void *)pcb);
			// pthread_detach(thrTablaPaginasCrear);
			break;

		case LIBERAR_RECURSOS:
			pthread_t thrTablaPaginasEliminar;
			pthread_create(&thrTablaPaginasEliminar, NULL, (void *)eliminarTablasPaginas, (void *)pcb);
			pthread_detach(thrTablaPaginasEliminar);
			break;

		case PAGE_FAULT:
			// recibir del kernel pagina , segmento , id pcb
			log_info(logger, "Estoy en page fault de memoria\n");
			t_paqt paquete;
			recibirMsje(socketAceptadoKernel, &paquete);
			MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT *mensaje = malloc(sizeof(MSJ_CPU_KERNEL_BLOCK_PAGE_FAULT));
			mensaje = paquete.mensaje;

			t_info_remplazo *infoRemplazo = malloc(sizeof(t_info_remplazo));
			infoRemplazo->idPagina = mensaje->nro_pagina;
			infoRemplazo->idSegmento = mensaje->nro_segmento;
			infoRemplazo->PID = pcb->id;

			log_info(logger, "El id de pagina es: %d", infoRemplazo->idPagina);
			log_info(logger, "El id de seg es: %d", infoRemplazo->idSegmento);
			log_info(logger, "El id de pcb es: %d\n", infoRemplazo->PID);

			t_marcos_por_proceso *marcoPorProceso = buscarMarcosPorProcesos(infoRemplazo);

			log_info(logger, "El id de pcb es: %d\n", marcoPorProceso->idPCB);

			asignacionDeMarcos(infoRemplazo, marcoPorProceso);
			free(mensaje);
	
			break;
		}
		free(paquete);
	}
}

t_marcos_por_proceso *buscarMarcosPorProcesos(t_info_remplazo *infoRemplazo)
{
	t_marcos_por_proceso *marcoPorProceso = NULL;
	for (int i = 0; i < list_size(LISTA_MARCOS_POR_PROCESOS); i++)
	{

		marcoPorProceso = list_get(LISTA_MARCOS_POR_PROCESOS, i);
		if (marcoPorProceso->idPCB == infoRemplazo->PID)
		{
			return marcoPorProceso;
		}
	}
	return marcoPorProceso;
}

void imprimirMarcosPorProceso()
{
	for (int i = 0; i < list_size(LISTA_MARCOS_POR_PROCESOS); i++)
	{

		t_marcos_por_proceso *marcoPorProceso = list_get(LISTA_MARCOS_POR_PROCESOS, i);
		log_info(logger, "Marco por proceso , el proceso es PID: %d", marcoPorProceso->idPCB);
		log_info(logger, "Marco por proceso , el marco siguiente es: %d", marcoPorProceso->marcoSiguiente);

		for (int j = 0; j < list_size(marcoPorProceso->paginas); j++)
		{
			t_pagina *pagina = list_get(marcoPorProceso->paginas, j);
			log_info(logger, "Bit de uso:  %d", pagina->uso);
			log_info(logger, "Numero de marco:  %d", pagina->nroMarco);
			log_info(logger, "Numero de pagina:  %d", pagina->nroPagina);
			log_info(logger, "Numero de segmento:  %d", pagina->nroSegmento);
		}
	}
}

void iniciar_servidor_hacia_cpu()
{
	int socketAceptadoCPU = 0;
	int server_fd = iniciar_servidor(IP_SERVER, configMemoria.puertoEscuchaDos); // socket(), bind()listen()

	log_info(logger, "Servidor listo para recibir al cpu");

	socketAceptadoCPU = esperar_cliente(server_fd);
	char *mensaje = recibirMensaje(socketAceptadoCPU);

	log_info(logger, "Mensaje de confirmacion del CPU: %s\n", mensaje);

	t_paqt paqueteCPU;

	recibirMsje(socketAceptadoCPU, &paqueteCPU);

	if (paqueteCPU.header.cliente == CPU)
	{

		log_debug(logger, "HANDSHAKE se conecto CPU");

		conexionCPU(socketAceptadoCPU);
	}

	log_info(logger, "Termine \n");
	mostrar_mensajes_del_cliente(socketAceptadoCPU);
	// free(&paqueteCPU);
}

void conexionCPU(int socketAceptado)
{
	t_paqt paquete;

	int pid;
	int pagina;
	int idTablaPagina;
	uint32_t valorAEscribir;

	t_direccionFisica *direccionFisica;

	MSJ_MEMORIA_CPU_ACCESO_TABLA_DE_PAGINAS *infoMemoriaCpuTP;
	MSJ_MEMORIA_CPU_LEER *infoMemoriaCpuLeer;
	MSJ_MEMORIA_CPU_ESCRIBIR *infoMemoriaCpuEscribir;

	while (1)
	{
		//  direccionFisica = malloc(sizeof(t_direccionFisica));
		//  infoMemoriaCpuTP = malloc(sizeof(MSJ_MEMORIA_CPU_ACCESO_TABLA_DE_PAGINAS));
		//  infoMemoriaCpuLeer = malloc(sizeof(MSJ_MEMORIA_CPU_LEER));
		//  infoMemoriaCpuEscribir = malloc(sizeof(MSJ_MEMORIA_CPU_ESCRIBIR));

		recibirMsje(socketAceptado, &paquete);

		switch (paquete.header.tipoMensaje)
		{
		case CONFIG_DIR_LOG_A_FISICA:
			configurarDireccionesCPU(socketAceptado);
			break;
		case ACCESO_MEMORIA_TABLA_DE_PAG:
			infoMemoriaCpuTP = paquete.mensaje;
			idTablaPagina = infoMemoriaCpuTP->idTablaDePaginas;
			pagina = infoMemoriaCpuTP->pagina;
			pid = infoMemoriaCpuTP->pid;
			accesoMemoriaTP(idTablaPagina, pagina, pid, socketAceptado);
			break;
		case ACCESO_MEMORIA_LEER:
			infoMemoriaCpuLeer = paquete.mensaje;
			direccionFisica->nroMarco = infoMemoriaCpuLeer->nroMarco;
			direccionFisica->desplazamientoPagina = infoMemoriaCpuLeer->desplazamiento;
			pid = infoMemoriaCpuLeer->pid;
			accesoMemoriaLeer(direccionFisica, pid, socketAceptado);
			break;
		case ACCESO_MEMORIA_ESCRIBIR:
			log_info(logger, "llegue aqui\n");
			infoMemoriaCpuEscribir = paquete.mensaje;
			direccionFisica->nroMarco = infoMemoriaCpuEscribir->nroMarco;
			direccionFisica->desplazamientoPagina = infoMemoriaCpuEscribir->desplazamiento;
			valorAEscribir = infoMemoriaCpuEscribir->valorAEscribir;
			pid = infoMemoriaCpuEscribir->pid;
			accesoMemoriaEscribir(direccionFisica, valorAEscribir, pid, socketAceptado);
			break;
		default: // TODO CHEKEAR: SI FINALIZO EL CPU ANTES QUE MEMORIA, SE PRODUCE UNA CATARATA DE LOGS. PORQUE? NO HAY PORQUE
			log_error(logger, "No se reconoce el tipo de mensaje, tas metiendo la patita");
			break;
		}
	}
}

void configurarDireccionesCPU(int socketAceptado)
{
	// SE ENVIAN LAS ENTRADAS_POR_TABLA y TAM_PAGINA AL CPU PARA PODER HACER LA TRADUCCION EN EL MMU
	log_debug(logger, "Se envian las ENTRADAS_POR_TABLA y TAM_PAGINA al CPU ");

	MSJ_MEMORIA_CPU_INIT *infoAcpu = malloc(sizeof(MSJ_MEMORIA_CPU_INIT));

	infoAcpu->cantEntradasPorTabla = configMemoria.entradasPorTabla;

	infoAcpu->tamanioPagina = configMemoria.tamPagina;

	// usleep(configMemoria.retardoMemoria * 1000); // CHEQUEAR, SI LO DESCOMENTAS NO PASA POR LAS OTRAS LINEAS

	enviarMsje(socketAceptado, MEMORIA, infoAcpu, sizeof(MSJ_MEMORIA_CPU_INIT), CONFIG_DIR_LOG_A_FISICA);

	free(infoAcpu);

	log_debug(logger, "Informacion de la cantidad de entradas por tabla y tamaño pagina enviada al CPU");
}

void crearTablasPaginas(void *pcb) // directamente asignar el la posswap aca para no recorrer 2 veces
{
	t_pcb *pcbActual = (t_pcb *)pcb;

	t_marcos_por_proceso *marcoPorProceso = malloc(sizeof(t_marcos_por_proceso));

	for (int i = 0; i < list_size(pcbActual->tablaSegmentos); i++)
	{
		t_tabla_paginas *tablaPagina = malloc(sizeof(t_tabla_paginas));
		t_tabla_segmentos *tablaSegmento = list_get(pcbActual->tablaSegmentos, i);

		tablaPagina->paginas = list_create();
		tablaPagina->idTablaPag = i;
		tablaSegmento->indiceTablaPaginas = i;

		/* 	pthread_mutex_lock(&mutex_creacion_ID_tabla);
			tablaSegmento->indiceTablaPaginas = contadorIdTablaPag;
			tablaPagina->idTablaPag = contadorIdTablaPag;
			contadorIdTablaPag++;
			pthread_mutex_unlock(&mutex_creacion_ID_tabla); */

		tablaPagina->idPCB = pcbActual->id;

		for (int i = 0; i < configMemoria.entradasPorTabla; i++)
		{
			t_pagina *pagina = malloc(sizeof(t_pagina));

			pagina->modificacion = 0;
			pagina->presencia = 0;
			pagina->uso = 0;
			pagina->nroMarco = 0;
			pagina->nroPagina = i;
			pagina->nroSegmento = tablaPagina->idTablaPag;

			pagina->posicionSwap = tamanioSgtePagina; // tamanioSgtePagina = OFFSET = desplazamiento
			// // fseek(swap, pagina->posicionSwap, SEEK_CUR);

			// agrego pagina a lista de paginas de la tabla pagina
			agregar_pagina_a_tabla_paginas(tablaPagina, pagina);
			tamanioSgtePagina += configMemoria.tamPagina;
		}

		// agrego tabla pagina a la lista de tablas pagina
		agregar_tabla_paginas(tablaPagina);
		log_info(logger, "la cant de elem que tiene listatablapaginas es: %d\n", list_size(LISTA_TABLA_PAGINAS));

		// log_info(logger, "PID: %d - Segmento: %d - TAMAÑO: %d paginas", tablaPagina->idPCB, tablaPagina->idTablaPag, list_size(tablaPagina->paginas));
		// Creacion
		log_info(loggerMinimo, "Creación de Tabla de Páginas: PID: %d - Segmento: %d - TAMAÑO: %d paginas", tablaPagina->idPCB, tablaPagina->idTablaPag, list_size(tablaPagina->paginas));
	}

	marcoPorProceso->idPCB = pcbActual->id;
	marcoPorProceso->marcoSiguiente = 0;
	marcoPorProceso->paginas = list_create();

	// agrego marco por proceso a la lista de marcos por procesos
	agregar_marco_por_proceso(marcoPorProceso);

	serializarPCB(socketAceptadoKernel, pcbActual, ASIGNAR_RECURSOS);

	free(pcbActual);
}

void eliminarTablasPaginas(void *pcb)
{
	t_pcb *pcbActual = (t_pcb *)pcb;

	filtrarYEliminarMarcoPorPIDTabla(pcbActual->id);

	t_list *tablasDelPCB = filtrarPorPIDTabla(pcbActual->id);

	log_info(logger, "Tamaño tabla pagina %d :\n", list_size(tablasDelPCB));

	for (int i = 0; i < list_size(tablasDelPCB); i++)
	{
		log_info(logger, "Tabla de pagina  %d\n", i);
		t_tabla_paginas *tablaPagina = list_get(tablasDelPCB, i);

		for (int j = 0; j < list_size(tablaPagina->paginas); j++)
		{
			t_pagina *pagina = list_get(tablaPagina->paginas, j);

			pagina->posicionSwap = -1;
			pagina->presencia = -1;
			pagina->nroMarco = -1;
			pagina->modificacion = -1;
			pagina->uso = -1;
		}
		// log_info(logger, "PID: %d - Segmento: %d - TAMAÑO: %d paginas", tablaPagina->idPCB, tablaPagina->idTablaPag, list_size(tablaPagina->paginas));
		// Destruccion de paginas
		log_info(loggerMinimo, "Destrucción de Tabla de Páginas: PID %d - Segmento: %d - TAMAÑO: %d paginas", tablaPagina->idPCB, tablaPagina->idTablaPag, list_size(tablaPagina->paginas));
	}

	enviarResultado(socketAceptadoKernel, "se liberaron las estructuras");
}

FILE *abrirArchivo(char *filename)
{

	if (filename == NULL)
	{
		log_error(logger, "Error: debe informar un path correcto");
		exit(1);
	}

	return fopen(filename, "w+");
}

void agregar_tabla_paginas(t_tabla_paginas *tablaPagina)
{
	pthread_mutex_lock(&mutex_lista_tabla_paginas);
	list_add(LISTA_TABLA_PAGINAS, tablaPagina);
	pthread_mutex_unlock(&mutex_lista_tabla_paginas);
}

void agregar_pagina_a_tabla_paginas(t_tabla_paginas *tablaPagina, t_pagina *pagina)
{
	pthread_mutex_lock(&mutex_lista_tabla_paginas_pagina);
	list_add(tablaPagina->paginas, pagina);
	pthread_mutex_unlock(&mutex_lista_tabla_paginas_pagina);
}

/*Acceso a tabla de páginas
El módulo deberá responder el número de marco correspondiente, en caso de no encontrarse,
se deberá retornar Page Fault.*/

void accesoMemoriaTP(int idTablaPagina, int nroPagina, int pid, int socketAceptado)
{
	// CPU SOLICITA CUAL ES EL MARCO DONDE ESTA LA PAGINA DE ESA TABLA DE PAGINA
	log_debug(logger, "ACCEDIENDO A TABLA DE PAGINA CON INDICE: %d NRO_PAGINA: %d PID: %d",
			  idTablaPagina, nroPagina, pid);

	int marcoBuscado;
	t_pagina *pagina;
	t_tabla_paginas *tabla_de_paginas;
	int indice;
	bool corte = false;
	// busco la pagina que piden
	pthread_mutex_lock(&mutex_lista_tabla_paginas);
	t_list *tablasFiltradasPorPid = filtrarPorPIDTabla(pid);
	for (int i = 0; i < list_size(tablasFiltradasPorPid) && !corte; i++)
	{
		tabla_de_paginas = list_get(tablasFiltradasPorPid, i);
		if (tabla_de_paginas->idTablaPag == idTablaPagina)
		{
			for (int j = 0; j < list_size(tabla_de_paginas->paginas) && !corte; j++)
			{
				pagina = list_get(tabla_de_paginas->paginas, j);
				if (pagina->nroPagina == nroPagina)
				{
					log_debug(logger, "BUSCO PAGINA %d", pagina->nroPagina);
					indice = j;
					corte = true;
				}
			}
		}
	}
	pthread_mutex_unlock(&mutex_lista_tabla_paginas);

	if (pagina->presencia == 1)
	{
		//  pthread_mutex_unlock(&mutex_lista_tabla_paginas);
		marcoBuscado = pagina->nroMarco;
		log_debug(logger, "[ACCESO_TABLA_PAGINAS] LA PAGINA ESTA EN RAM");

		MSJ_INT *mensaje = malloc(sizeof(MSJ_INT));
		mensaje->numero = marcoBuscado;

		enviarMsje(socketAceptado, MEMORIA, mensaje, sizeof(MSJ_INT), RESPUESTA_MEMORIA_MARCO_BUSCADO);
		free(mensaje);
		//  log_debug(logger, "Acceso a Tabla de Páginas: “PID: %d - Página: %d - Marco: %d ",
		//		  pid, pagina->nroPagina, marcoBuscado); // LOG OBLIGATORIO
		// Acceso a Tabla de Paginas
		log_info(loggerMinimo, "Acceso a Tabla de Páginas: PID: %d - Página: %d - Marco: %d",
				  pid, pagina->nroPagina, marcoBuscado);

		log_debug(logger, "[FIN - TRADUCCION_DIR] FRAME BUSCADO = %d ,DE LA PAGINA: %d DE TABLA DE PAG CON INDICE: %d ENVIADO A CPU",
				  marcoBuscado, pagina->nroPagina, tabla_de_paginas->idTablaPag); // chequear y borrar
	}
	else if (pagina->presencia == 0)
	{ // la pag no esta en ram. Retornar PAGE FAULT

		MSJ_INT *mensaje = malloc(sizeof(MSJ_INT));
		mensaje->numero = -1;
		enviarMsje(socketAceptado, MEMORIA, mensaje, sizeof(MSJ_INT), PAGE_FAULT);
		free(mensaje);
		log_info(loggerMinimo, "Acceso a Tabla de Páginas: PID: %d - Página: %d - Marco: PAGE FAULT",
				  pid, pagina->nroPagina);
	}
}

void accesoMemoriaLeer(t_direccionFisica *df, int pid, int socketAceptado)
{
	// log_debug(logger,"Acceso a espacio de usuario: “PID: %d - Acción: LEER - Dirección física: %d  %d",pid,df->nroMarco, df->desplazamientoPagina);
	// Acceso a espacio de Usuario
	log_info(loggerMinimo, "Acceso a espacio de usuario: PID: %d - Acción: LEER - Dirección física: %d  %d", pid, df->nroMarco, df->desplazamientoPagina);

	log_debug(logger, "[ACCESO_MEMORIA_LEER] DIR_FISICA: %d  %d",
			  df->nroMarco, df->desplazamientoPagina);

	int nroFrame = df->nroMarco;
	int desplazamiento = df->desplazamientoPagina;
	uint32_t *aLeer = malloc(sizeof(uint32_t));
	MSJ_STRING *msjeError;

	// valido que el offset sea valido
	if (desplazamiento > configMemoria.tamPagina)
	{
		usleep(configMemoria.retardoMemoria * 1000);
		msjeError = malloc(sizeof(MSJ_STRING));
		string_append(&msjeError->cadena, "ERROR_DESPLAZAMIENTO");
		enviarMsje(socketAceptado, MEMORIA, msjeError, sizeof(MSJ_STRING), ACCESO_MEMORIA_LEER);
		free(msjeError);
		log_error(logger, "[ACCESO_MEMORIA_LEER] OFFSET MAYOR AL TAMANIO DEL FRAME.  DIR_FISICA: %d%d",
				  df->nroMarco, df->desplazamientoPagina);
		return;
	}

	pthread_mutex_lock(&mutex_void_memoria_ram);
	memcpy(aLeer, memoriaRAM + (nroFrame * configMemoria.tamPagina) + desplazamiento, sizeof(uint32_t));
	printf("aLeer%d", *(uint32_t *)aLeer);
	//	printf("Hola %d",  (uint32_t  *)memoriaRAM + (nroFrame * configMemoria.tamPagina) + desplazamiento);
	pthread_mutex_unlock(&mutex_void_memoria_ram);
	//*puntero es el contenido
	//&variable es la direccion

	log_debug(logger, "Valor Leido: %d", *(uint32_t *)aLeer);

	usleep(configMemoria.retardoMemoria * 1000);

	/***********************************************/
	t_pagina *pagina;

	pthread_mutex_lock(&mutex_lista_marco_por_proceso);
	t_marcos_por_proceso *marcosPorProceso = buscarMarcosPorProcesosAccesos(pid);
	pthread_mutex_unlock(&mutex_lista_marco_por_proceso);
	printf("memorialeeeer\n");
	//  pagina = list_get(marcosPorProceso->paginas, nroFrame % configMemoria.marcosPorProceso);
	//  pagina->uso = 1;

	for (int i = 0; i < marcosPorProceso->paginas->elements_count; i++)
	{
		pagina = list_get(marcosPorProceso->paginas, i);
		if (pagina->nroMarco == nroFrame)
		{
			pagina->uso = 1;

			break;
		}
	}
	printf("memorialeeeer2\n");
	/***********************************************/
	MSJ_INT *mensajeRead = malloc(sizeof(MSJ_INT));
	mensajeRead->numero = *(uint32_t *)aLeer;
	printf("casteanding a int: %d \n", mensajeRead->numero);
	enviarMsje(socketAceptado, MEMORIA, mensajeRead, sizeof(MSJ_INT), ACCESO_MEMORIA_LEER);
	free(mensajeRead);

	log_debug(logger, "ACCESO_MEMORIA_LEER DIR_FISICA: frame%d offset%d",
			  df->nroMarco, df->desplazamientoPagina);
}

void accesoMemoriaEscribir(t_direccionFisica *dirFisica, uint32_t valorAEscribir, int pid, int socketAceptado)
{
	// log_debug(logger,"Acceso a espacio de usuario: “PID: %d - Acción: ESCRIBIR - Dirección física: %d  %d",pid,dirFisica->nroMarco, dirFisica->desplazamientoPagina);
	// Acceso a Espacio de Usuario
	log_info(loggerMinimo, "Acceso a espacio de usuario: PID: %d - Acción: ESCRIBIR - Dirección física: %d  %d", pid, dirFisica->nroMarco, dirFisica->desplazamientoPagina);

	log_debug(logger, "[ACCESO_MEMORIA_ESCRIBIR] DIR_FISICA: nroMarco %d offset %d, VALOR: %d",
			  dirFisica->nroMarco, dirFisica->desplazamientoPagina, valorAEscribir);

	int nroMarco = dirFisica->nroMarco;
	int desplazamiento = dirFisica->desplazamientoPagina;

	// valido que el desplazamiento sea valido
	if (desplazamiento > configMemoria.tamPagina)
	{
		usleep(configMemoria.retardoMemoria * 1000);
		char *mensajeError = string_new();
		string_append(&mensajeError, "ERROR_DESPLAZAMIENTO");
		enviarMsje(socketAceptado, MEMORIA, mensajeError, strlen(mensajeError) + 1, ACCESO_MEMORIA_ESCRIBIR);
		free(mensajeError);
		log_error(logger, "[ACCESO_MEMORIA_WRITE] DESPLAZAMIENTO MAYOR AL TAMANIO DEL FRAME.  DIR_FISICA: %d %d, VALOR: %d",
				  dirFisica->nroMarco, dirFisica->desplazamientoPagina, valorAEscribir);
		return;
	}

	pthread_mutex_lock(&mutex_void_memoria_ram);
	uint32_t *aEscribir = &valorAEscribir;
	printf("aEscribir%d\n", *aEscribir);
	memcpy(memoriaRAM + (nroMarco * configMemoria.tamPagina) + desplazamiento, aEscribir, sizeof(uint32_t)); // tercer arg strlen(string_itoa(valorAEscribir))
	pthread_mutex_unlock(&mutex_void_memoria_ram);
	printf("aEscribir%d\n", *aEscribir);
	// printf("%d",  *(uint32_t  *)memoriaRAM + (nroMarco * configMemoria.tamPagina) + desplazamiento);

	// busco la pagina que piden y actualizo el bit de modificado porque se hizo write
	t_pagina *pagina;

	pthread_mutex_lock(&mutex_lista_marco_por_proceso);
	t_marcos_por_proceso *marcosPorProceso = buscarMarcosPorProcesosAccesos(pid);
	pthread_mutex_unlock(&mutex_lista_marco_por_proceso);
	printf("llegue acaaaaaaa\n");

	for (int i = 0; i < marcosPorProceso->paginas->elements_count; i++)
	{
		pagina = list_get(marcosPorProceso->paginas, i);
		if (pagina->nroMarco == nroMarco)
		{
			pagina->uso = 1;
			pagina->modificacion = 1;
			usleep(configMemoria.retardoMemoria * 1000);
			break;
		}
	}

	printf("llegue aca\n");

	char *cadena = string_new();
	string_append(&cadena, "OK");

	enviarMsje(socketAceptado, MEMORIA, cadena, strlen("OK") + 1, ACCESO_MEMORIA_ESCRIBIR);

	free(cadena);
	log_debug(logger, "[FIN - ACCESO_MEMORIA_WRITE] DIR_FISICA: %d %d, VALOR: %d",
			  dirFisica->nroMarco, dirFisica->desplazamientoPagina, valorAEscribir);
}

t_marcos_por_proceso *buscarMarcosPorProcesosAccesos(int pid)
{
	t_marcos_por_proceso *marcoPorProceso = NULL;
	for (int i = 0; i < list_size(LISTA_MARCOS_POR_PROCESOS); i++)
	{

		marcoPorProceso = list_get(LISTA_MARCOS_POR_PROCESOS, i);
		if (marcoPorProceso->idPCB == pid)
		{
			return marcoPorProceso;
		}
	}
	return marcoPorProceso;
}

bool esta_vacio_el_archivo(FILE *nombreFile)
{

	if (NULL != nombreFile)
	{
		fseek(nombreFile, 0, SEEK_END);
		size_t size = ftell(nombreFile);

		if (0 == size)
		{
			return true;
		}
		return false;
	}
}

bool buscar_pagina_en_memoria()
{

	t_tabla_paginas *tablaPagina;
	tablaPagina->idTablaPag;

	// list_find(t_list *, bool(*closure)(void*));
}

void *conseguir_puntero_a_base_memoria(int nro_marco, void *memoriaRAM)
{ // aca conseguimos el puntero que apunta a la posicion donde comienza el marco

	void *aux = memoriaRAM;

	return (aux + nro_marco * configMemoria.tamPagina);
}

void *conseguir_puntero_al_desplazamiento_memoria(int nro_marco, void *memoriaRAM, int desplazamiento)
{ // aca conseguimos el puntero al desplazamiento respecto al marco

	return (conseguir_puntero_a_base_memoria(nro_marco, memoriaRAM) + desplazamiento);
}

void asignacionDeMarcos(t_info_remplazo *infoRemplazo, t_marcos_por_proceso *marcosPorProceso)
{
	log_info(logger, "Entro a asignacion de marcos\n");
	if (chequearCantidadMarcosPorProceso(infoRemplazo))
	{
		log_info(logger, "La cantidad de marcos del proceso es correcta\n");
		asignarPaginaAMarco(marcosPorProceso, infoRemplazo);
	}
	else
	{
		log_info(logger, "Entro a los algoritmos de reemplazo\n");
		implementa_algoritmo_susticion(infoRemplazo);
		enviarResultado(socketAceptadoKernel, "Algoritmos de reemoplazo realizados correctamente");
	}
}

void algoritmo_reemplazo_clock(t_info_remplazo *infoRemplazo)
{
	pthread_mutex_lock(&mutex_lista_marco_por_proceso);
	t_marcos_por_proceso *marcosPorProceso = buscarMarcosPorProcesos(infoRemplazo);
	pthread_mutex_unlock(&mutex_lista_marco_por_proceso);
	log_info(logger, "Estoy en algoritmo clock\n");
	primer_recorrido_paginas_clock(marcosPorProceso, infoRemplazo);
}

void primer_recorrido_paginas_clock(t_marcos_por_proceso *marcosPorProceso, t_info_remplazo *infoRemplazo)
{

	log_info(logger, "\n Estoy en algoritmo clock primer recorrido \n");
	log_info(logger, "\n Marco que llega %d\n", marcosPorProceso->marcoSiguiente);
	// marcosPorProceso->marcoSiguiente = recorrer_marcos(marcosPorProceso->marcoSiguiente);
	// int i = marcosPorProceso->marcoSiguiente;
	bool encontrado = false;
	while (!encontrado)
	{
		log_debug(logger, "entre a reemplazar");

		t_pagina *pagina = list_get(marcosPorProceso->paginas, marcosPorProceso->marcoSiguiente);
		log_info(logger, "\n marcos por proceso siguiente %d\n", marcosPorProceso->marcoSiguiente);
		log_info(logger, "\nentrando a la pagina %d , segmento %d, bit uso %d\n", pagina->nroPagina, pagina->nroSegmento, pagina->uso);
		// log_info(loggerMinimo, "Reemplazo - PID: %d", infoRemplazo->PID);

		if (pagina->uso == 0)
		{
			if (pagina->modificacion == 1)
			{

				escribirEnSwap(pagina->nroMarco, pagina->posicionSwap);

				//  Escritura de Pagina en SWAP
				log_info(loggerMinimo, "Escritura de Página en SWAP: SWAP OUT -  PID: %d - Marco: %d - Page Out: %d|%d", marcosPorProceso->idPCB, pagina->nroMarco, pagina->nroSegmento, pagina->nroPagina);
			}

			encontrado = true;

			t_pagina *newPagina = buscarPagina(infoRemplazo);

			newPagina->nroMarco = pagina->nroMarco;
			newPagina->uso = 1;
			newPagina->presencia = 1;
			newPagina->modificacion = 0;

			leerEnSwap(newPagina->nroMarco, newPagina->posicionSwap);
			log_info(loggerMinimo, "Lectura de Página de SWAP: SWAP IN -  PID: %d - Marco: %d - Page In: %d|%d", marcosPorProceso->idPCB, newPagina->nroMarco, newPagina->nroSegmento, newPagina->nroPagina);

			t_pagina *paginaVictima = list_replace(marcosPorProceso->paginas, marcosPorProceso->marcoSiguiente, newPagina);
			// printf("\npagina victima: segmento %d , pagina%d \n",paginaVictima->nroSegmento, paginaVictima->nroPagina );
			//  no es necesario que los cambiemos , una vez que vuelva a pasar se va a cargar el bit de uso en 0 y el modificado tambien
			//  donde se carga el modificado en 0?
			paginaVictima->uso = 0;
			paginaVictima->modificacion = 0;
			paginaVictima->presencia = 0;
			// printf("\npagina victima: segmento %d , pagina%d \n",paginaVictima->nroSegmento, paginaVictima->nroPagina);

			log_info(loggerMinimo, "Reemplazo Página: REEMPLAZO - PID: %d - Marco: %d - Page Out: %d | %d - Page In: %d | %d", infoRemplazo->PID, newPagina->nroMarco, paginaVictima->nroSegmento, paginaVictima->nroPagina, infoRemplazo->idSegmento, infoRemplazo->idPagina);

			// arranca a reemplazar en el marco 1
		}
		else if (pagina->uso == 1)
		{
			log_info(logger, "\nAsigne el bit de uso en 0\n");
			pagina->uso = 0;
		}

		if (marcosPorProceso->marcoSiguiente < (configMemoria.marcosPorProceso - 1))
		{

			marcosPorProceso->marcoSiguiente++;
		}
		else
		{

			marcosPorProceso->marcoSiguiente = 0;
		}

		log_info(logger, "\n marcos por proceso siguiente %d\n", marcosPorProceso->marcoSiguiente);
	}
}

void escribirEnSwap(int nroMarco, int posicionSwap)
{
	mem_hexdump(conseguir_puntero_a_base_memoria(nroMarco, memoriaRAM), configMemoria.tamPagina);
	fseek(swap, posicionSwap, SEEK_SET);
	size_t valorEscribir = fwrite(conseguir_puntero_a_base_memoria(nroMarco, memoriaRAM), 1, configMemoria.tamPagina, swap);
	usleep(configMemoria.retardoSwap * 1000);
}

void leerEnSwap(int nroMarco, int posicionSwap)
{
	void *paginaBuffer = malloc(configMemoria.tamPagina);
	fseek(swap, posicionSwap, SEEK_SET);
	fread(paginaBuffer, 1, configMemoria.tamPagina, swap);

	usleep(configMemoria.retardoSwap * 1000);
	mem_hexdump(paginaBuffer, configMemoria.tamPagina);
	memcpy(conseguir_puntero_a_base_memoria(nroMarco, memoriaRAM), paginaBuffer, configMemoria.tamPagina);
	free(paginaBuffer);
}

t_pagina *buscarPagina(t_info_remplazo *infoRemplazo)
{
	t_list *tablasNewPagina;
	tablasNewPagina = filtrarPorPIDTabla(infoRemplazo->PID);
	log_info(logger, "Cant de tablas en pcb : %d\n", list_size(tablasNewPagina));
	for (int i = 0; i < list_size(tablasNewPagina); i++)
	{
		log_info(logger, "Entre al for antes del list_get\n");
		t_tabla_paginas *tablaPagina = list_get(tablasNewPagina, i);
		log_info(logger, "Sali del list_get\n");
		log_info(logger, "TablaPagina = %d", tablaPagina->idTablaPag);

		if (tablaPagina->idTablaPag == infoRemplazo->idSegmento)
		{
			log_info(logger, "Concuerda el id de la tabla con el segmento, encontramos la tabla!!\n");
			for (int j = 0; j < list_size(tablaPagina->paginas); j++)
			{
				t_pagina *pagina = list_get(tablaPagina->paginas, j);

				if (infoRemplazo->idPagina == pagina->nroPagina)
				{
					log_info(logger, "Concuerda el id de la pagina, con la pagina que queriamos encontrar, encontramos la pagina!!\n");
					return pagina;
				}
			}
		}
	}
}

int recorrer_marcos(int marcoSiguiente)
{

	if (marcoSiguiente < (configMemoria.marcosPorProceso - 1))
	{

		return marcoSiguiente++;
	}
	else
	{

		return 0;
	}
}

void algoritmo_reemplazo_clock_modificado(t_info_remplazo *infoRemplazo)
{
	log_info(logger, "Entre a clock modificado\n");
	pthread_mutex_lock(&mutex_lista_marco_por_proceso);
	t_marcos_por_proceso *marcosPorProceso = buscarMarcosPorProcesos(infoRemplazo);
	pthread_mutex_unlock(&mutex_lista_marco_por_proceso);

	t_pagina *pagina = NULL;

	while (pagina == NULL)
	{
		log_info(logger, "Entre a clock modificado while\n");
		pagina = buscarMarcoSegun(marcosPorProceso, infoRemplazo, 0, 0);
		if (pagina == NULL)
		{ // si no encontro pagina en (0-0) cambio uso
			pagina = buscarMarcoSegun(marcosPorProceso, infoRemplazo, 0, 1);
		}
	}
	log_info(logger, "Entre a clock modificado");

	if (pagina->modificacion == 1)
	{
		// Escritura de pagina en Swap
		escribirEnSwap(pagina->nroMarco, pagina->posicionSwap);

		log_info(loggerMinimo, "Escritura de Página en SWAP: SWAP OUT -  PID: %d - Marco: %d - Page Out: %d|%d", marcosPorProceso->idPCB, pagina->nroMarco, pagina->nroSegmento, pagina->nroPagina);
	}

	t_pagina *newPagina = buscarPagina(infoRemplazo);

	newPagina->nroMarco = pagina->nroMarco;
	newPagina->uso = 1;
	newPagina->presencia = 1;
	newPagina->modificacion = 0;

	leerEnSwap(newPagina->nroMarco, newPagina->posicionSwap);
	log_info(loggerMinimo, "Lectura de Página de SWAP: SWAP IN -  PID: %d - Marco: %d - Page In: %d|%d", marcosPorProceso->idPCB, newPagina->nroMarco, newPagina->nroSegmento, newPagina->nroPagina);

	printf("\ncantidad de paginas por marcos por proceso %d :\n", list_size(marcosPorProceso->paginas));
	t_pagina *paginaVictima = list_replace(marcosPorProceso->paginas, marcosPorProceso->marcoSiguiente, newPagina);
	paginaVictima->uso = 0;
	paginaVictima->modificacion = 0;
	paginaVictima->presencia = 0;
	log_info(loggerMinimo, "Reemplazo Página: REEMPLAZO - PID: %d - Marco: %d - Page Out: %d | %d - Page In: %d | %d", infoRemplazo->PID, paginaVictima->nroMarco, paginaVictima->nroSegmento, paginaVictima->nroPagina, infoRemplazo->idSegmento, infoRemplazo->idPagina);

	if (marcosPorProceso->marcoSiguiente < (configMemoria.marcosPorProceso - 1))
	{

		marcosPorProceso->marcoSiguiente++;
	}
	else
	{
		marcosPorProceso->marcoSiguiente = 0;
	}
	// marcosPorProceso->marcoSiguiente = recorrer_marcos(marcosPorProceso->marcoSiguiente); // para que el puntero al siguiente siga abanzando
}

t_pagina *buscarMarcoSegun(t_marcos_por_proceso *marcosPorProceso, t_info_remplazo *infoRemplazo, int uso, int modificado)
{

	int i = marcosPorProceso->marcoSiguiente;
	printf("\n marcos por proceso arranca %d\n", i);

	while (1)
	{
		t_pagina *pagina = list_get(marcosPorProceso->paginas, marcosPorProceso->marcoSiguiente);
		if (pagina->uso == uso && pagina->modificacion == modificado)
		{
			//marcosPorProceso->marcoSiguiente = marcosPorProceso->marcoSiguiente;
			return pagina;
		}
		// solo entra en busqueda de (0-1)
		if (modificado == 1)
		{
			printf("\nAsigne el bit de uso en 0\n");
			pagina->uso = 0;
		}
		if (marcosPorProceso->marcoSiguiente < (configMemoria.marcosPorProceso - 1))
		{
			marcosPorProceso->marcoSiguiente++;
		}
		else
		{
			marcosPorProceso->marcoSiguiente = 0;
		}
		if (i == marcosPorProceso->marcoSiguiente)
		{ // si pego la vuelta, inicio
			return NULL;
		}
		printf("\n marcos por proceso siguiente %d\n", marcosPorProceso->marcoSiguiente);
	}
}

int buscar_marco_vacio() // devuelve la primera posicion del marco vacio, ver bien esto, tiene que incrementar
{
	for (int i = 0; i < list_size(LISTA_BITMAP_MARCO); i++)
	{
		t_bitmap_marcos_libres *marcoLibre = list_get(LISTA_BITMAP_MARCO, i);

		if (marcoLibre->uso == 0)
		{
			log_info(logger, "Encontre un marco libre!!\n");
			// marcoLibre->nroMarco = i;
			marcoLibre->uso = 1;
			log_info(logger, "NroMArco: %d, bit de uso: %d\n", marcoLibre->nroMarco, marcoLibre->uso);
			return i;
		}
	}

	return -1;
}

void inicializar_bitmap()
{
	for (int i = 0; i < tamanio; i++)
	{
		t_bitmap_marcos_libres *marcoLibre = malloc(sizeof(t_bitmap_marcos_libres));
		marcoLibre->nroMarco = i;

		marcoLibre->uso = 0;

		list_add(LISTA_BITMAP_MARCO, marcoLibre);
	}
}

void asignarPaginaAMarco(t_marcos_por_proceso *marcosPorProceso, t_info_remplazo *infoReemplazo)
{
	log_info(logger, "Entro a asignar marcos por proceso");
	int posicionMarcoLibre = buscar_marco_vacio();

	log_info(logger, "La posicion del marco libre es: %d\n", posicionMarcoLibre);

	t_pagina *pagina = buscarPagina(infoReemplazo);

	pagina->nroMarco = posicionMarcoLibre;
	pagina->uso = 1;
	pagina->presencia = 1;

	log_info(logger, "El numero de marco asignado a la pagina es: %d", pagina->nroMarco);

	agregar_pagina_a_lista_de_paginas_marcos_por_proceso(marcosPorProceso, pagina);

	leerEnSwap(pagina->nroMarco, pagina->posicionSwap);
	log_info(loggerMinimo, "Lectura de Página de SWAP: SWAP IN -  PID: %d - Marco: %d - Page In: %d|%d", marcosPorProceso->idPCB, pagina->nroMarco, pagina->nroSegmento, pagina->nroPagina);

	incrementarMarcoSiguiente(marcosPorProceso);

	enviarResultado(socketAceptadoKernel, " asignacion de marcos realizada correctamente");
	log_info(logger, "La cant de pag del pcb %d del marcoPorProceso es: %d\n", marcosPorProceso->idPCB, list_size(marcosPorProceso->paginas));
}

void incrementarMarcoSiguiente(t_marcos_por_proceso *marcosPorProceso)
{
	if (marcosPorProceso->marcoSiguiente < configMemoria.marcosPorProceso - 1)
	{
		marcosPorProceso->marcoSiguiente++;
	}
	else
	{
		marcosPorProceso->marcoSiguiente = 0;
	}
}

void implementa_algoritmo_susticion(t_info_remplazo *infoRemplazo)
{
	switch (obtenerAlgoritmoSustitucion())
	{
	case CLOCK:
		log_info(logger, "Entrando a algoritmo Clock\n");
		algoritmo_reemplazo_clock(infoRemplazo);
		break;

	case CLOCK_MODIFICADO:
		log_info(logger, "Entrando a algoritmo Clock Modificado\n");
		algoritmo_reemplazo_clock_modificado(infoRemplazo);
		break;

	default:
		break;
	}
}

t_tipo_algoritmo_sustitucion obtenerAlgoritmoSustitucion()
{

	char *algoritmo = configMemoria.algoritmoReemplazo;

	t_tipo_algoritmo_sustitucion algoritmoResultado;

	if (!strcmp(algoritmo, "CLOCK"))
	{
		algoritmoResultado = CLOCK;
	}
	else if (!strcmp(algoritmo, "CLOCK_MODIFICADO"))
	{
		algoritmoResultado = CLOCK_MODIFICADO;
	}

	return algoritmoResultado;
}

void agregar_marco_por_proceso(t_marcos_por_proceso *marcosPorProceso)
{
	pthread_mutex_lock(&mutex_lista_marco_por_proceso);
	list_add(LISTA_MARCOS_POR_PROCESOS, marcosPorProceso);
	pthread_mutex_unlock(&mutex_lista_marco_por_proceso);

	log_info(logger, "La cantidad de marcos por proceso es: %d", list_size(LISTA_MARCOS_POR_PROCESOS));
}

void agregar_pagina_a_lista_de_paginas_marcos_por_proceso(t_marcos_por_proceso *marcosPorProceso, t_pagina *pagina)
{
	pthread_mutex_lock(&mutex_lista_marcos_por_proceso_pagina);
	list_add(marcosPorProceso->paginas, pagina);
	pthread_mutex_unlock(&mutex_lista_marcos_por_proceso_pagina);
}

bool chequearCantidadMarcosPorProceso(t_info_remplazo *infoRemplazo)
{
	pthread_mutex_lock(&mutex_lista_marco_por_proceso);
	t_marcos_por_proceso *marcosPorProcesoActual = buscarMarcosPorProcesos(infoRemplazo);
	pthread_mutex_unlock(&mutex_lista_marco_por_proceso);

	return list_size(marcosPorProcesoActual->paginas) < configMemoria.marcosPorProceso;
}

t_list *filtrarPorPIDTabla(int PID)
{
	t_list *listaTabla = list_create();
	int i = 0;

	while (i < list_size(LISTA_TABLA_PAGINAS))
	{
		t_tabla_paginas *tablaPagina = list_get(LISTA_TABLA_PAGINAS, i);

		if (tablaPagina->idPCB == PID)
		{
			list_add(listaTabla, tablaPagina);
		}
		i++;
	}

	return listaTabla;
}

void filtrarYEliminarMarcoPorPIDTabla(int PID)
{
	int i = 0;

	while (i < list_size(LISTA_MARCOS_POR_PROCESOS))
	{
		t_marcos_por_proceso *marcoPorProceso = list_get(LISTA_MARCOS_POR_PROCESOS, i);

		if (marcoPorProceso->idPCB == PID)
		{
			t_marcos_por_proceso *marcoAEliminar = list_remove(LISTA_MARCOS_POR_PROCESOS, i);

			eliminarEstructura(marcoAEliminar);
		}
		i++;
	}
}

void eliminarEstructura(t_marcos_por_proceso *marcoAEliminar)
{
	for (int i = 0; i < list_size(marcoAEliminar->paginas); i++)
	{
		t_pagina *paginaAEliminar = list_remove(marcoAEliminar->paginas, i);

		//  free(paginaAEliminar);
	}
	free(marcoAEliminar);
}