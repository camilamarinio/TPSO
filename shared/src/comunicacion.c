#include "comunicacion.h"

void enviarResultado(int socket, char* mensajeAEnviar){
	
		enviarMensaje(socket,mensajeAEnviar);
}

int enviarMensaje(int socket, char *msj)
{
	size_t size_stream;

	void *stream = serializarMensaje(msj, &size_stream);


	return enviarStream(socket, stream, size_stream);
}

void enviarMsje(int socket, t_enviadoPor unModulo, void* mensaje, int tamanioMensaje, t_tipoMensaje tipoMensaje){
	t_paqt* paquete = malloc(sizeof(t_paqt));

	paquete->header.tipoMensaje = tipoMensaje;
	paquete->header.cliente = unModulo;
	uint32_t r = 0;
	if(tamanioMensaje<=0 || mensaje==NULL){
		paquete->header.tamanioMensaje = sizeof(uint32_t);
		paquete->mensaje = &r;
	} else {
		paquete->header.tamanioMensaje = tamanioMensaje;
		paquete->mensaje = mensaje;
	}
	enviarPaquete(socket, paquete);
	free(paquete);
}

void enviarPaquete(int socket, t_paqt* paquete) {
	uint32_t cantAEnviar = sizeof(t_infoMensaje) + paquete->header.tamanioMensaje;
	void* datos = malloc(cantAEnviar);
	memcpy(datos, &(paquete->header), sizeof(t_infoMensaje));

	if (paquete->header.tamanioMensaje > 0){
		memcpy(datos + sizeof(t_infoMensaje), (paquete->mensaje), paquete->header.tamanioMensaje);
	}

	uint32_t enviado = 0; //bytes enviados
	uint32_t totalEnviado = 0;

	do {
		enviado = send(socket, datos + totalEnviado, cantAEnviar - totalEnviado, MSG_NOSIGNAL);
		totalEnviado += enviado;
		if(enviado==-1){
			break;
		}
	} while (totalEnviado != cantAEnviar);

	free(datos);
}


int enviarStream(int socket, void *stream, size_t stream_size)
{

	if (send(socket, stream, stream_size, 0) == -1)
	{
		free(stream);
		return ERROR;
	}

	free(stream);
	return SUCCESS;
}

void *serializarMensaje(char *msj, size_t *size_stream)
{

	*size_stream = strlen(msj) + 1;

	void *stream = malloc(sizeof(*size_stream) + *size_stream);

	memcpy(stream, size_stream, sizeof(*size_stream));
	memcpy(stream + sizeof(*size_stream), msj, *size_stream);

	*size_stream += sizeof(*size_stream);

	return stream;
}


char *recibirMensaje(int socket)
{
	size_t *tamanio_mensaje;
	char *msj;

	tamanio_mensaje = recibirStream(socket, sizeof(*tamanio_mensaje));

	if (tamanio_mensaje )
	{

		if ((msj = recibirStream(socket, *tamanio_mensaje)))
		{
			free(tamanio_mensaje);
			return msj;
		}

		free(tamanio_mensaje);
	}

	return NULL;
}

void recibirMsje(int socket, t_paqt* paquete) {

	paquete->mensaje = NULL;
	int resul = recibirDatos(&(paquete->header), socket, sizeof(t_infoMensaje));

	if (resul > 0 && paquete->header.tamanioMensaje > 0) {
		paquete->mensaje = malloc(paquete->header.tamanioMensaje);
		resul = recibirDatos(paquete->mensaje, socket, paquete->header.tamanioMensaje);
	}
}

int recibirDatos(void* paquete, int socket, uint32_t cantARecibir) {
	void* datos = malloc(cantARecibir);
	int recibido = 0;
	int totalRecibido = 0;

	/*do {
		recibido = recv(socket, datos + totalRecibido, cantARecibir - totalRecibido, 0);
		totalRecibido += recibido;
	} while (totalRecibido != cantARecibir && recibido > 0);
*/


recibido = recv(socket, datos + totalRecibido, cantARecibir - totalRecibido, MSG_WAITALL);
	if (recibido <= 0)
	{
		return NULL;
	}
totalRecibido += recibido;
	
while (totalRecibido != cantARecibir && recibido > 0){
	recibido = recv(socket, datos + totalRecibido, cantARecibir - totalRecibido, MSG_WAITALL);totalRecibido += recibido;}
	memcpy(paquete, datos, cantARecibir);
	free(datos);
	return recibido;
}

void *recibirStream(int socket, size_t stream_size)
{
	void *stream = malloc(stream_size);

	if (recv(socket, stream, stream_size, 0) == -1)
	{
		free(stream);
		stream = NULL;
		exit(-1);
	}

	return stream;
}