#include "consola.h"

int main(int argc, char **argv)
{

	/* ---------------- LOGGING ---------------- */

	logger = iniciar_logger("consola.log", "CONSOLA", LOG_LEVEL_INFO);


	log_info(logger, "\n Iniciando consola...");

	/* ---------------- ARCHIVOS DE CONFIGURACION ---------------- */

	obtenerArgumentos(argc, argv); // Recibe 3 argumentos, ./consola, la ruta del archivoConfig y la ruta de las instrucciones de pseudocodigo

	/* ---------------- LEER DE CONSOLA ---------------- */

	FILE *instructionsFile = abrirArchivo(argv[2]);

	t_informacion *informacion = crearInformacion();

	agregarInstruccionesDesdeArchivo(instructionsFile, informacion->instrucciones);

	t_paquete *nuevoPaquete = crear_paquete_programa(informacion);

	conexionConsola = crear_conexion(configConsola.ipKernel, configConsola.puertoKernel);

	log_warning(logger,"\nconexion consola %d\n", conexionConsola);

	// Armamos y enviamos el paquete
	enviar_paquete(nuevoPaquete, conexionConsola);
	eliminar_paquete(nuevoPaquete);
	liberar_programa(informacion);

	log_info(logger, "Se enviaron todas las instrucciones y los segmentos!\n");

	char *mensaje = recibirMensaje(conexionConsola);
	log_info(logger, "Mensaje de confirmacion del Kernel : %s\n", mensaje);
	free(mensaje);
	while (1)
	{
		log_info(logger, "Consola en espera de nuevos mensajes del kernel..");
		t_paqueteActual *paquete = recibirPaquete(conexionConsola);
		uint32_t valor;
		switch (paquete->codigo_operacion)
		{
		case BLOCK_PCB_IO_PANTALLA:
			valor = deserializarValor(paquete->buffer, conexionConsola);
			log_info(logger ,"Valor por pantalla recibido desde kernel: %d", valor);
			// printf("\nValor por pantalla recibido desde kernel: %d\n", valor);
			usleep(configConsola.tiempoPantalla * 1000);
			enviarResultado(conexionConsola, "se mostro el valor por pantalla\n");
			break;

		case BLOCK_PCB_IO_TECLADO:
			char *mensaje = recibirMensaje(conexionConsola);
			log_info(logger, "Me llego el mensaje: %s\n", mensaje);

			char *valorConsola;
			// printf("\nIngresa un valor por consola: \n");
			log_info(logger,"Ingresa un valor por consola: ");
			valorConsola = readline("> ");

			valor = atoi(valorConsola);
			
			serializarValor(valor, conexionConsola, BLOCK_PCB_IO_TECLADO);

			free(valorConsola);
			break;
		case TERMINAR_CONSOLA:
			log_info(logger , "Termino la consola");
			// printf("\nTermino la consola\n");
			liberar_conexion(conexionConsola);
			return EXIT_SUCCESS;
		}

		
		// terminar_programa(conexion, logger, config);
	}
}

void leerConfig(char *rutaConfig)
{
	config = iniciar_config(rutaConfig);
	extraerDatosConfig(config);

	printf(PRINT_COLOR_GREEN "\n===== Archivo de configuracion =====\n IP: %s \n PUERTO: %s \n TIEMPO PANTALLA: %d \n SEGMENTOS: [", configConsola.ipKernel, configConsola.puertoKernel, configConsola.tiempoPantalla);

	for (int i = 0; i < size_char_array(configConsola.segmentos); i++)
	{
		printf("%s", configConsola.segmentos[i]);

		if ((i + 1) < size_char_array(configConsola.segmentos))
		{
			printf(", ");
		}
	}
	printf("]" PRINT_COLOR_RESET);
}

void obtenerArgumentos(int argc, char **argv)
{

	if (argc != 3)
	{
		printf(PRINT_COLOR_RED "\nError: cantidad de argumentos incorrecta:" PRINT_COLOR_RESET "\n");
		// return -1;
	}
	else
	{
		rutaArchivoConfiguracion = argv[1];
		rutaInstrucciones = argv[2];

		leerConfig(rutaArchivoConfiguracion);

		printf(PRINT_COLOR_GREEN "\nCantidad de argumentos de entrada Correctos!" PRINT_COLOR_RESET "\n");
	}

	printf("=== Argumentos de entrada ===\n rutaArchivoConfig: %s \n rutaArchivoDeInstrucciones: %s \n\n", rutaArchivoConfiguracion, rutaInstrucciones);
}

FILE *abrirArchivo(char *filename)
{
	if (filename == NULL)
	{
		log_error(logger, "Error: debe informar un archivo de instrucciones.");
		exit(1);
	}
	return fopen(filename, "r");
}

t_configConsola extraerDatosConfig(t_config *archivoConfig)
{
	configConsola.ipKernel = string_new();
	configConsola.puertoKernel = string_new();
	configConsola.segmentos = string_array_new();

	configConsola.ipKernel = config_get_string_value(archivoConfig, "IP_KERNEL");
	configConsola.puertoKernel = config_get_string_value(archivoConfig, "PUERTO_KERNEL");
	configConsola.segmentos = config_get_array_value(archivoConfig, "SEGMENTOS");
	configConsola.tiempoPantalla = config_get_int_value(archivoConfig, "TIEMPO_PANTALLA");

	return configConsola;
}

t_list *listaSegmentos()
{
	t_list *listaDeSegmentos = list_create();

	for (int i = 0; i < size_char_array(configConsola.segmentos); i++)
	{
		char *segmento = string_new();
		string_append(&segmento, configConsola.segmentos[i]);

		uint32_t segmentoResultado = atoi(segmento);

		printf("\nsegmento lista:%d\n",segmentoResultado);
		list_add(listaDeSegmentos, segmentoResultado);
	}

	return listaDeSegmentos;
}

t_informacion *crearInformacion()
{
	t_informacion *informacion = malloc(sizeof(t_informacion));
	informacion->instrucciones = list_create();
	informacion->segmentos = listaSegmentos();
	return informacion;
}

void liberar_programa(t_informacion *informacion)
{
	list_destroy_and_destroy_elements(informacion->instrucciones, free);
	free(informacion);
}

t_paquete *crear_paquete_programa(t_informacion *informacion)
{
	log_info(logger,"\n entro a serializar\n");
	t_buffer *buffer = malloc(sizeof(t_buffer));
	buffer->size = sizeof(uint32_t) * 2 + calcularSizeInfo(informacion) // instrucciones_size
				   + list_size(informacion->segmentos) * sizeof(uint32_t); // segmentos_size

	void *stream = malloc(buffer->size);

	int offset = 0; // Desplazamiento
	memcpy(stream + offset, &(informacion->instrucciones->elements_count), sizeof(uint32_t));
	offset += sizeof(uint32_t);

	memcpy(stream + offset, &(informacion->segmentos->elements_count), sizeof(uint32_t));
	offset += sizeof(uint32_t);
	// Serializa las instrucciones
	int i = 0;
	int j = 0;


	while (i < list_size(informacion->instrucciones))
	{
		t_instruccion* instrucccion = list_get(informacion->instrucciones, i);
		memcpy(stream + offset,&instrucccion->instCode, sizeof(t_instCode));
		offset += sizeof(t_instCode);
		memcpy(stream + offset,&instrucccion->paramInt, sizeof(uint32_t));
		offset += sizeof(uint32_t);
		memcpy(stream + offset,&instrucccion->sizeParamIO, sizeof(uint32_t));
		offset += sizeof(uint32_t);
		//printf("\ndispositivo%s\n" ,instrucccion->paramIO);
		memcpy(stream + offset,instrucccion->paramIO,instrucccion->sizeParamIO);
		offset += instrucccion->sizeParamIO;
		memcpy(stream + offset,&instrucccion->paramReg[0], sizeof(t_registro));
		offset += sizeof(t_registro);
		memcpy(stream + offset,&instrucccion->paramReg[1], sizeof(t_registro));
		offset += sizeof(t_registro);
		i++;
		//printf(PRINT_COLOR_MAGENTA "Estoy serializando las instruccion %d" PRINT_COLOR_RESET "\n", i);
	}

	while (j < list_size(informacion->segmentos))
	{
		uint32_t segmento = list_get(informacion->segmentos, j);
		memcpy(stream + offset, &segmento, sizeof(uint32_t));
		printf("\nsegmento: %d\n", segmento);
		offset += sizeof(uint32_t);
		j++;
		printf(PRINT_COLOR_YELLOW "Estoy serializando el segmento: %d" PRINT_COLOR_RESET "\n", j);
	}

	buffer->stream = stream; // Payload

	// free(informacion->instrucciones);
	// free(informacion->segmentos);

	// lleno el paquete
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = NEW;
	paquete->buffer = buffer;
	return paquete;
}


