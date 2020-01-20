// N U E V A   V E R S I O N  D E L   C O D I G O   D E L   S E R V I D O R 
//---------------------------------------------------------------------------------- 
// Se ha reestructurado por motivos de complejidad y resolucin de algunos problemas.

// INCLUDES 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mysql.h>
#include <pthread.h>

// DEFINES 
#define TRUE 1
#define FALSE 0
#define MAX_NUM_USUARIOS 100
#define PORT 9105
#define MAX_FORMAT_USER 20
#define MAX_FORMAT_INPUT 80
#define MAX_CHAR 512

//mysql defines ---> para cambiar de host en localhost a shiva, solo hay que cambiar
//el "localhost" por "tu direcci�n de shiva"
#define HOST "localhost"
#define USR "root"
#define PSWD "mysql"

// ESTRUCTURAS PARA EL FORMATO DE NOMBRES, ETC
/*
con esto podemos declarar variables del tipo X para que sea mas facil de entender
a la hora de cambiar est o es mas escalable.
*/

typedef char nombre[MAX_FORMAT_USER]; //nuevo formato para los usuarios
typedef char contrasena[MAX_FORMAT_USER]; //nuevo formato para la contrasena
typedef char input[MAX_FORMAT_INPUT];
typedef char buffer[MAX_CHAR];
typedef char query[MAX_CHAR];

// LISTAS Y CLASES

//Estructura de usuario y su socket
typedef struct{
	nombre username;
	int socket;
}Usuario;

//Esctructura de todos los usuarios
typedef struct{
	Usuario usuario[MAX_NUM_USUARIOS];
	int num;
}TUsuarios;

//Estructura para protocolo de invitacion
typedef struct{
	Usuario user_host;
	Usuario user_invitado;
	int id_partida;
}Partida;

//Estructura para empezar partida 
typedef struct{
	Partida partida[100];
	int numero;
}TablaPartidas;

// VARIABLES GLOBALES
//mysql para base de datos
MYSQL *mysql_conn;
MYSQL_RES *res;
MYSQL_ROW *row;

//usuario
TUsuarios usuarios[MAX_NUM_USUARIOS];

//socket & server
struct sockaddr_in serv_adr;
int serverSocket;
int sock_atendedidos[MAX_NUM_USUARIOS];

//threads
pthread_t thread[MAX_NUM_USUARIOS];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//Tabla Partidas
TablaPartidas games;

// FUNCIONES DE LISTA DE CONNECTADOS

//Funcion que da la lista de conectados 
void GetConnected(buffer notif){
	int i;
	buffer substitute;
	strcpy(notif, "6/");
	
	for(i = 0; i < usuarios->num; i++){
		strcpy(substitute, notif);
		sprintf(notif, "%s%s ", substitute, usuarios->usuario[i].username);
	}
}

//Da la posicion de un cierto socket si no existe devuelve un -1
int DarPosicionSocket(int socket){
	int encontrado = FALSE, i = 0;
	//Bucle para encontrar 
	while((!encontrado)&&(i < usuarios->num)){
		if(usuarios->usuario[i].socket == socket)
		//Si se encuentra el socket parar la busqueda
			encontrado = TRUE;
		else
		//Seguir buscando en caso contrario
			i++;
	}
	//Si se encuentra guardar la posicion
	if(encontrado)
		return i;
	//Si no se encuentra devolver un -1
	else
		return -1;
}

//a�ade a la lista de jugadores conectados un usuario
//Si se ha podido añadir correctamente devuelve un 1
//y en caso contratio devuelve 0
int AddToList(nombre user, int socket){
	int encontrado=0;
	for(int i=0;i<usuarios->num;i++)
	{
		//Comprobamos que el judagador no este en la lista
		if (strcmp(usuarios->usuario[i].username,user)==0)
			//Si esta salimos del bucle
			encontrado=1;
	}
	//Comprobamos que no sobrepase el numero maximo de usuarios
	if((usuarios->num < MAX_NUM_USUARIOS)&&(encontrado==0)){
		//Si cumple condicion añadimos usuario y su socket
		strcpy(usuarios->usuario[usuarios->num].username,user);
		usuarios->usuario[usuarios->num].socket = socket;
		//Actualizamos el numero de usuarios en la lista
		usuarios->num++;
		//Devuelve true
		return TRUE;
	}
	else{
		//En caso de no pode añadir devolver false
		return FALSE;
	}
}

//Quitar de la lista de jugadores conectados un usuario
//Si es posible quita al usuario de la lista en caso contrario
//devuelve un 0
int RemoveFromList(int socket){
	int posicionLista = DarPosicionSocket(socket), i;
	if(posicionLista >= 0){
		//Buscamos al usuario en la lista
		for(i = posicionLista; i < usuarios->num; i++){
			strcpy(usuarios->usuario[i].username, usuarios->usuario[i + 1].username);
			usuarios->usuario[i].socket = usuarios->usuario[i + 1].socket;
		}
		//Actualizamos numero de usuarios de la lista
		usuarios->num--;
	}
	else
	//Si no se ha podido quitar de la lista devuelve false
	   return FALSE;
}

//Devuelve el socket en funcion de un nombre de usuario recibido
//Si no lo encuentra devuelve un -1
int DevolverSocket(nombre user){
	int encontrado = FALSE;
	int	i = 0;
	//Buscamos al usuario
	while((encontrado==FALSE)&&(i < usuarios->num)){
		if(strcmp(usuarios->usuario[i].username, user)==0)
			//Se encuentra usuario
			encontrado = TRUE;
		else
			//Sigue con la busqueda si no se encuentra
			i++;
	}
	if(encontrado==TRUE)
	//Si se encuentra usuario devuelve el socket
		return usuarios->usuario[i].socket;
	else
	//Si no se encuentra usuario devuelve un -1
		return -1;
}

//Devuelve un nombre en funcion de un socket de usuario recibido
void DevolverNombre(int socket, nombre user){
	int encontrado = FALSE, i = 0;
	//Buscamos al usuario
	while((encontrado==FALSE)&&(i < usuarios->num)){
		if(usuarios->usuario[i].socket == socket){
			//Si se encuentra devuelve el nombre
			strcpy(user, usuarios->usuario[i].username);
			encontrado = TRUE;
		}
		else
		//Sigue con la busqueda en caso contrario
		   i++;
	}
}

//A�ade una partida a la lista de partidas
//En caso de no ser posible devuelve un booleano false
int AnadirPartida(Partida p){
	int encontrado=0;
	int i=0;

	//si hay mas de 100 partidas activas no puedes crear la partida
	if (games.numero < 100) {
		while ((i < 100) && (encontrado == 0))
		{
			//miramos si hay alguna partida libre
			if (games.partida[i].id_partida == -1)
				encontrado = 1;
			else i++;
		}
		if (encontrado == 1)
		{
			//componentes de partida = id_partida, user_host, user_invitado
			p.id_partida = i;
			games.partida[i] = p;
			games.numero++;
			//Devuelve true en caso de poder añadir a la partida
			return TRUE;
		}
		//Devuelve false en caso de no poder añadir la partida
		else return FALSE;
	}
	//Devuelve false en caso de superar más de 100 partidas
	else return FALSE;
}

//Crea una partida 
void CrearPartida(Partida *p, nombre J1, nombre J2)
{
	strcpy(p->user_host.username,J1);		//Jugador 1-> es el host de la partida
	strcpy(p->user_invitado.username,J2);	//Jugador 2-> es el invitado de la partida
}

//Funcion que nos dice quien es el rival y quien es el host de la partida
//Devuelve un 1 en caso de ser el rival y un 0 en caso contrario
int DevolverRival(nombre jugador, nombre rival){
	int i=0;
	while (i<100)
	{
		//si es el host de la partida
		if (strcmp(games.partida[i].user_host.username,jugador)==0)
	    {
			strcpy(rival, games.partida[i].user_invitado.username);
			return TRUE;
	    }

		//si es el invitado de la partida
		if (strcmp(games.partida[i].user_invitado.username,jugador)==0)
		{
			strcpy(rival,games.partida[i].user_host.username);
			return TRUE;
		}
		else i++;
    }

	//si no ha encontrado al jugador, devolvemos 0
	return FALSE;
};

// INICIADORES
/*
funcion para el inicio y config del servidor
aqui solo configuramos la IP y el puerto en uso
���PARA CAMBIAR DE PUERTO CAMBIAMOS DEL DEFINE EL PORT!!!
*/
void InitServer(){
	//CONFIGURACION DEL SERVIDOR
	printf("[Iniciando Servidor...]\n");
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	printf("Habilitamos serverSocket.\n");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(PORT);
	printf("Configuraci�n del server lista, escuchando en puerto: %d\n", PORT);
}


/*
Bind con el puerto:
*/
void InitBind(){
	//CONFIGURACION DEL BIND
	printf("[Iniciando Bind...]\n");
	if (bind(serverSocket, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0){
		printf ("Error al bind\n");
		exit(1);
	}
	else{
		printf("Bind creado correctamente.\n");
		
		//Comprobamos que el servidor este escuchando
		if (listen(serverSocket, 2) < 0){
			printf("Error en el Listen");
			exit(1);
		}
		else
			printf("serverSocket funcionando correctamente.\n[Todo OK.]\n");
	}
}



/*
Iniciamos la base de datos:
*/
void InitBBDD(){
	//CONFIGURAMOS LA CONEXION BASEDATOS CON SERVIDOR C
	int err;
	mysql_conn = mysql_init(NULL);
	if (mysql_conn==NULL) 
	{
		printf ("Error al crear la conexion: %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
	//inicializar la conexion, indicando nuestras claves de acceso
	// al servidor de bases de datos (user,pass)
	mysql_conn = mysql_real_connect (mysql_conn, "localhost", "root", "mysql", NULL, 0, NULL, 0);
	if (mysql_conn==NULL)
	{
		printf ("Error al inicializar la conexion: %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
	
	//indicamos la base de datos con la que queremos trabajar 
	err=mysql_query(mysql_conn, "use juego;");
	if (err!=0)
	{
		//Mensaje de error en caso de no encotrar la base de datos
		printf ("Error al conectar con la base de datos %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
}

//Iniciar la tabla de partidas
void InitTablaPartidas(){
	for (int i=0; i<100; i++)
	{
		games.partida[i].id_partida = -1;
	}
}

// CERRAR BBDD
void CerrarBBDD(){
	mysql_close(mysql_conn);
}



// CONSULTAS EN LA BBDD
void ConsultaBBDD(query consulta){
	int err;
	err = mysql_query(mysql_conn, consulta);
	if(err){
		//Mensaje de error al no poder concetar con la base de datos
		printf ("Error al crear la conexion: %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
	else{
		res = mysql_store_result(mysql_conn); //Esto se asigna a la variable global MYSQL_RES *res
	}
}

//Buscamos a un jugador en la base de datos, si no se encuentra devuelve un 0 
//si se encuentra devuelve su id
int EncontrarJugador(nombre usuario){
	//Queremos encontrar un jugador en la tabla de jugadores.
	query consulta;
	//Seleccionamos id del jugador de la base de datos
	sprintf(consulta, "SELECT id FROM jugador WHERE usuario = '%s';", usuario);
	ConsultaBBDD(consulta);
	row = mysql_fetch_row(res);
	
	if (row == NULL)
	{
		//No se ha encontrado
		return FALSE;
	}
	
	else{
		int id;
		id=atoi(row[0]);
		//se ha encontrado, devuelve id 
		return id;
	}
}

//buscamos si existe cierta ID de jugador
//Si existe devolvemos un 1 y si no un 0
int ExisteID(int ID){
	query consulta;
	//Consultamos por el id
	sprintf(consulta, "SELECT usuario FROM jugador WHERE id = %d;", ID);
	
	ConsultaBBDD(consulta);
	row = mysql_fetch_row(res);
	
	if(row == NULL)
		return FALSE; //Si no existe la ID, devolvemos 0
	else
		return TRUE; //Si existe, devolvemos 1
}

//buscamos si existe cierta ID de partida
//si existe devolvemos un 1 y si no un 0
int ExisteIDpartida(int ID) {
	query consulta;
	//Consultamos por la partida en la base de datos
	sprintf(consulta, "SELECT id FROM partidas WHERE id = %d;", ID);
	ConsultaBBDD(consulta);
	row = mysql_fetch_row(res);
	if (row == NULL)
		return FALSE; //Si no existe la ID, devolvemos 0
	else
		return TRUE; //Si existe, devolvemos 1
}

//logearse
//Si se puede logear devuelve un 1 y sino un 0
int LogIN(input in, buffer output, int socket){
	nombre user1;
	contrasena contra1;
	query consulta;
	//obtenemos la informacion
	sscanf(in, "%s %s", user1, contra1);
	//Mirem si existeix tal usuari:
	pthread_mutex_lock(&lock);
	if (EncontrarJugador(user1) !=0){
		sprintf(consulta, "SELECT contrasena FROM jugador WHERE usuario = '%s';", user1);
		//Obtenemos el resultado de la consulta.
		ConsultaBBDD(consulta); //Ya se ha asignado el valor de la consulta al res.
		row = mysql_fetch_row(res);
		//Si exite la base de datos
		if(row != NULL){
			if(strcmp(row[0], contra1)==0){
				//Puede añadir el jugador
				if(AddToList(user1, socket)){
					pthread_mutex_unlock(&lock);
					//Guarda el mensaje "1/1"
					strcpy(output, "1/1");
					//Devuelve 1
					return TRUE;
				}
				else{
					//Guarda un "1/0" para enviar como mensaje
					//Si no ha sido posible enviar
					strcpy(output, "1/0");
					//Devuelve un 0
					return FALSE;
				}
			}
			else{
				strcpy(output, "1/0");
				return FALSE;
			}
		}
		else{
			pthread_mutex_unlock(&lock);
			strcpy(output, "1/1");
			return FALSE;
		}
	}
	//Si no existe la base de datos 
	else{
		pthread_mutex_unlock(&lock);
		strcpy(output, "1/0"); //No existe, retornamos 0.
		return FALSE;
	}
}

//dar de baja a un usuario
//Si se puede dar de baja devuelve un 1 y quita al jugador de la lista
//En caso contrario devuelve un 0
int DeleteAccount(input in, buffer output, int socket){
	query consulta;
	pthread_mutex_lock(&lock);
	//Si encuentra al jugador
	if (EncontrarJugador(in) > 0){
		//Elimina de la base de datos
		sprintf(consulta,"DELETE FROM jugador WHERE usuario = '%s';",in);
		ConsultaBBDD(consulta);
		//Elimina de la lista de conectados
		RemoveFromList(socket);
		pthread_mutex_unlock(&lock);
		//Guarda mensaje de "15/1"
		strcpy(output, "15/1");
		//Devuelve un 1
		return TRUE;
	}
	//Si no encuentra al jugador
	else{
		pthread_mutex_unlock(&lock);
		//Guarda el mensaje "15/0"
		strcpy(output, "15/0");
		//Devuelve un 0
		return FALSE;
	}
}

//registrarse
//Añade a un usuario nuevo en la la base de datos
//Si no es posible devuelve un 0
int SignUP(input in, buffer output, int socket){
	nombre user;
	contrasena contra;
	query consulta;
	int ID, existe = TRUE;
	//Obtener la información del input.
	sscanf(in,"%s %s", user, contra);
	pthread_mutex_lock(&lock);
	ID = rand();
	if(ExisteID(ID)){ //Si existe la ID, hay que cambiarla
		while(existe){
			ID = rand();
			existe = ExisteID(ID);
		}
		existe = TRUE; //Como la ID random ahora no existe podemos decir que la ID nueva esta libre con lo cual puede existir.
	}
	if(existe){ //Si la ID est� libre, significa que puede existir
		if(EncontrarJugador(user) == 0){
			sprintf(consulta, "INSERT INTO jugador (id, usuario, contrasena) VALUES(%d ,'%s', '%s')", ID ,user, contra);
			ConsultaBBDD(consulta); //Registrado
			
			if(EncontrarJugador(user) > 0){
				//Se ha podido añadir a la lista
				if(AddToList(user, socket)){
					pthread_mutex_unlock(&lock);
					strcpy(output, "2/1");
					//Devuelve un 1
					return TRUE;
				}
				//No se ha podido añadir a la lista 
				else{
					strcpy(output, "2/0");
					//Devuelve un 0
					return FALSE;
				}
			}
			//No ha podido registrarse
			else{
				pthread_mutex_unlock(&lock);
				strcpy(output, "2/0");
				//Devuelve un 0
				return FALSE;
			}
		}
	}
}

//puntos totales conseguidos por un usuario
//Nos indica los puntos totales de un jugador
//Si el jugador existe devuelve un 1
//Si no existe devuelve un 0
int PuntosTotales(input in, buffer output){
	nombre usuario;
	query consulta;
	
	//Queremos obtener el usuario del cual recoger la puntuación total:
	sscanf(in, "%s", usuario);
	
	pthread_mutex_lock(&lock);
	//Encuentra el jugador
	if(EncontrarJugador(usuario) > 0){
		//Queremos recoger el valor de la consulta:
		sprintf(consulta, "SELECT SUM(relacion.puntuacion) FROM jugador, relacion, partidas WHERE relacion.idjug = jugador.id AND jugador.usuario ='%s';", usuario);
		ConsultaBBDD(consulta);
		
		row = mysql_fetch_row(res);
		//Accede a la base de datos y obtiene puntuacion
		if(row[0] != NULL){
			int result = atoi(row[0]);
			
			pthread_mutex_unlock(&lock);
			//Devuelve puntuacion en formato de mensaje
			sprintf(output, "3/%d", result);
			//Devuelve un 1
			return TRUE;
		}
		else{
			//Error de conexion en la base de datos
			pthread_mutex_unlock(&lock);
			sprintf(output, "3/0");
			//Devuelve un 0
			return FALSE;
		}
	}
	//Si el jugador no existe
	else{
		pthread_mutex_unlock(&lock);
		sprintf(output, "3/0");
		//Devuelve 0
		return FALSE;
	}	
}

//tiempo total de juego de un jugador
//Indica el tiempo total de un jugador y en caso de poder hacerlo devuelve un 1
//Y si no devuelve un 0
int TiempoTotalJugado(input in, buffer output){
	nombre usuario;
	query consulta;
	
	sscanf(in, "%s", usuario);
	
	pthread_mutex_lock(&lock);
	//Busca al jugador
	if(EncontrarJugador(usuario) > 0){
		//Consulta eel tiempo total del jugador
		sprintf(consulta, "SELECT SUM(partidas.duracion) FROM partidas, jugador, relacion WHERE partidas.id = relacion.idpartida AND relacion.idjug = jugador.id AND jugador.usuario = '%s';", usuario);
		ConsultaBBDD(consulta);
		row = mysql_fetch_row(res);
		//Si hay error en la base de datos
		if(row[0] == NULL){
			pthread_mutex_unlock(&lock);
			strcpy(output, "4/0");
			//Devuelve 0
			return FALSE;
		}
		//Si no hay error en la base de datos
		else{
			int result = atoi(row[0]);
			pthread_mutex_unlock(&lock);
			//Muestra tiempo total
			sprintf(output, "4/%d", result);
			//Devuelve 1
			return TRUE;
		}
	}
	//No encuentra jugador
	else{
		pthread_mutex_unlock(&lock);
		strcpy(output, "4/0");
		//Devuelve 0
		return FALSE;
	}
}

//cuantas partidas ha ganado cierta persona
//Indica el numero de partidas que ha ganado un jugador
//En caso de ser posible devuelve un 1 y sino un 0
int PartidasGanadasVS(input in, buffer output){
	nombre usuario;
	query consulta;
	sscanf(in, "%s", usuario);
	pthread_mutex_lock(&lock);
	//Busca al jugador
	if(EncontrarJugador(usuario) > 0){
		//Realiza consulta a base de datos
		sprintf(consulta, "SELECT COUNT(ganador) FROM partidas WHERE ganador = '%s';", usuario);
		ConsultaBBDD(consulta);
		row = mysql_fetch_row(res);
		//Si no existe base de datos
		if(row[0] == NULL){
			pthread_mutex_unlock(&lock);
			strcpy(output, "5/0");
			//Devuelve 0
			return FALSE;
		}
		//Si existe base de datos
		else{
			int result = atoi(row[0]);
			pthread_mutex_unlock(&lock);
			//Indica el numero de partidas ganadas
			sprintf(output, "5/%d", result);
			//Devuelve 1
			return TRUE;
		}
	}
	//Si no encuentra al jugador 
	else{
		pthread_mutex_unlock(&lock);
		strcpy(output, "5/0");
		//Devuelve un 0
		return FALSE;
	}
}


// TRHEADS Y EJECUCION DE CODIGO
//Devuelve el codigo de la peticion que recibimos del cliente
int DarCodigo(buffer buff,input in){
	char *p =strtok(buff,"/");
	int codigo = atoi(p);
	if(codigo != 0){ //Si el codi es 0 no fa falta seguir tro�ejant
		p = strtok(NULL, "/");
		strcpy(in,p);
	}
	//Devuelve el codigo
	return codigo;
}

//Envia la invitacion al invitado
//En caso de ser posible devuelve un 1
//Y si no devuelve un 0
int EnviarInvitacion(input in, buffer output, int socket){
	//Separamos el mensaje 
	nombre userInvitado;
	strcpy(userInvitado, in);
	//SOCKET DEL INVITADO
	int socketInvitado = DevolverSocket(userInvitado);
	//DevolverSocket devuelve -1 si no existe tal socket:
	if(socketInvitado > 0){
		buffer mensaje;
		nombre userHost; //userHost es el LIDER DE LA PARTIDA
		DevolverNombre(socket, userHost); //Recogemos el nombre del LIDER
		sprintf(mensaje, "7/%s", userHost); //Al mensaje que enviar le decimos el nombre de usuario del LIDER
		write(socketInvitado, mensaje, strlen(mensaje)); //Enviamos al invitado.
		strcpy(output, "10/1"); //Enviamos si se ha podido enviar con exito.
		return TRUE;
	}
	else{
		strcpy(output, "10/0"); //Sin exito.
		return FALSE;
	}
}

//Se informa del rechazo en caso de hacerlo con exito devuelve un 1
//Si no es posible devuelve un 0
int EnviarRechazo(input in, buffer output, int socket){
	/*
	Para el rechazo, el invitado envia:
	"9/userHost"
	
	Al Host de la partida se le manda:
	"9/userInvitado"
	
	De esta manera solo hay que recoger el socket del lider a traves de la funcion dame socket.
	*/
	nombre userHost;
	nombre userInvitado;

	//userHost
	sscanf(in, "%s", userHost);
	
	//userInvitado
	DevolverNombre(socket, userInvitado);
	
	//ScoketHost
	int socketHost = DevolverSocket(userHost);
	
	//Finalmente enviamos las peticiones
	if(socketHost > 0){
		buffer mensaje;
		
		sprintf(mensaje, "9/%s", userInvitado);
		write(socketHost, mensaje, strlen(mensaje));
		
		strcpy(output, "11/1");
		return TRUE;
	}
	else{
	//No se ha podido enviar la peticion
		strcpy(output, "11/0");
		//Devuelve 0
		return FALSE;
	}
}

//Informa de la aceptacion de la partida
//En caso de exito devuelve un 1
//En caso contrario un 0
int EnviarAceptar(input in, buffer output, int socket){
	/*
	Para Aceptar, el invitado envia:
	"8/userHost"
	
	Al Host de la partida se le envia:
	"8/userInvitado"
	
	De esta manera solo hay que recoger el socket del lider a traves de la funcion dame socket.
	*/
	nombre userHost;
	nombre userInvitado;
	
	//userHost
	sscanf(in, "%s", userHost);
	
	//userInvitado
	DevolverNombre(socket, userInvitado);
	
	//ScoketHost
	int socketHost = DevolverSocket(userHost);
	
	//Finalmente enviamos las peticiones
	if(socketHost > 0){
		buffer mensaje;
		sprintf(mensaje, "8/%s", userInvitado);
		write(socketHost, mensaje, strlen(mensaje));
		strcpy(output, "12/1");
		Partida p;
		CrearPartida(&p,userHost,userInvitado);
		int x = AnadirPartida(p);
		if (x==TRUE)
		//se ha podido añadir a la partida el invitado
		//Devuelve 1
		 return TRUE;
		//Devuelve un 0
		else return FALSE;
	}
	else{
		//No existe devuelve un 0
		strcpy(output, "12/0");
		return FALSE;
	}
}

//Funcion para enviar mensaje a otro usuario
//Si se envia con exito devuelve un 1
//Si hay error devuelve un 0
int EnviarMensaje(input in, buffer output, int socket){
	/*
	Para Enviar mensaje, el emisor envia:
	"13/Receptor mensaje"
	
	Al Receptor se le envia:
	"13/Emisor mensaje"
	
	De esta manera solo hay que recoger el socket del receptor a traves de la funcion dame socket.
	*/
	nombre Receptor;
	nombre Emisor;
	buffer mensaje;
	
	//Separamos mensaje y receptor
	printf("El input es %s\n", in);
	char *p = strtok(in, "-");
	strcpy(Receptor,p);
	p=strtok(NULL,"-");
	printf("El mensaje es %s\n", p);
	//Emisor
	DevolverNombre(socket,Emisor);
	printf("El emisor es %s con socket %d\n",Emisor,socket);
	//SocketReceptor
	int socketReceptor=DevolverSocket(Receptor);
	printf("El receptor es %s con socket %d\n",Receptor,socketReceptor);
	//Finalmente enviamos las peticiones
	if(socketReceptor>= 0){
		buffer msg;
		sprintf(msg, "13/%s-%s",Emisor, p);
		write(socketReceptor, msg, strlen(msg));
		printf("Enviamos a socket %d: %s\n",socketReceptor,msg);
		strcpy(output, "14/1");
		//Devuelve un 1
		return TRUE;
	}
	//No se puede enviar
	else{
		strcpy(output, "14/0");
		//Devuelv un 0
		return FALSE;
	}
}

/*
Teoria sobre comunicacion en partida:
	-> en el servidor no se hace ninguna comprovacion sobre los barcos (etc)
	   simplemente nos dedicamos a reenviar la informacion que manda uno de 
	   los clientes, es por eso mismo que el codigo de todas estas funciones
	   es practicamente el mismo.

	cliente1:	 servidor:									   client2:
	x/10 4 ----> (al cliente 1)x/1o0 (al cliente2)x/10 4 ----> x/1o0
	 do x  <---- (al cliente 1)x/1o0 (al cliente2)x/1o0  <----	||
*/


//Enviamos la prioridad de la partida (quien va primero)
//Si no hay ningun error devuelve un 1
//Si hay algun error devuelve un 0
int EnviarPrioridad(input in, buffer output, int socket) {
	int result = 0; //Quien va primero '0' -> el otro, '1' -> yo
	int socket_receptor = -1;
	sscanf(in, "%d", &result);

	nombre jugador_requester;
	DevolverNombre(socket, jugador_requester);	//Quienes somos nostros
	
	nombre jugador_rival;						//Quien es el rival
	if (DevolverRival(jugador_requester, jugador_rival)){
		socket_receptor = DevolverSocket(jugador_rival);
		printf("%d\n", socket_receptor);
	}
	else
		return FALSE;	//no se ha encontrado el rival

	//ahora hay que enviarle al rival quien va primero.
	if (socket_receptor < 0) {
		strcpy(output, "40/0");
		//no se ha encontrado el socket del rival.
		return FALSE;
	}
	else {
		buffer msg;
		sprintf(msg, "17/%d", result);
		write(socket_receptor, msg, strlen(msg));
		strcpy(output, "40/1");
		//No ha habido nungun problema
		//Devuelve un 1
		return TRUE;
	}
}

//Enviamos la casilla seleccionada
//Si ha habido cualquier error devuelve un 0
//En caso de exito devuelve un 1
int EnviarPosicion(input in, buffer output, int socket) {
	int x = 0, y = 0;								
	sscanf(in, "%d-%d", &x, &y);
	printf("%d %d\n", x, y);
	int socket_receptor = -1;

	nombre jugador_requester;
	DevolverNombre(socket, jugador_requester);	//Quienes somos nostros

	nombre jugador_rival;						//Quien es el rival
	if (DevolverRival(jugador_requester, jugador_rival)){
		socket_receptor = DevolverSocket(jugador_rival);
	}
	else
		return FALSE;	//no se ha encontrado el rival

	//ahora hay que enviarle al rival nuestra eleccion
	if (socket_receptor < 0) {
		strcpy(output, "41/0");
		//no se ha encontrado el socket del rival.
		//Devuelve 0
		return FALSE;
	}
	else {
		buffer msg;
		sprintf(msg, "16/%d %d", x, y);
		write(socket_receptor, msg, strlen(msg));
		strcpy(output, "41/1");
		//Devuelve un 1 en caso de exito
		return TRUE;
	}
}

//Enviamos si ha acertado o si ha fallado
//En caso de cualquier error devuelve un 0
//En caso de tener exito un 1
int EnviarAcierto(input in, buffer output, int socket) {
	int resultado = 0;
	int socket_receptor = -1;
	sscanf(in, "%d", &resultado);

	nombre jugador_requester;
	DevolverNombre(socket, jugador_requester);	//Quienes somos nostros

	nombre jugador_rival;						//Quien es el rival
	if (DevolverRival(jugador_requester, jugador_rival)){
		socket_receptor = DevolverSocket(jugador_rival);
	}
	else
		return FALSE;	//no se ha encontrado el rival

	//ahora hay que enviarle al rival si ha fallado o ha acertado
	if (socket_receptor < 0) {
		strcpy(output, "42/0");
		//no se ha encontrado el socket del rival.
		return FALSE;
	}
	else {
		buffer msg;
		sprintf(msg, "18/%d", resultado);
		write(socket_receptor, msg, strlen(msg));
		strcpy(output, "42/1");
		//todo correcto.
		return TRUE;
	}
}

//el que ha ganado envia un mensaje al servidor para que incluya la partida a la
//base de datos.
//En caso de cualquier error devuelve un 0
int ActualizarPartida(input in, buffer output, int socket) {
	/*
	----------------------------------------------------------------------------
	| Hace falta cambiar la base de datos con los nuevos parametros de partida |
	----------------------------------------------------------------------------
	*/
	//variables
	int id_partida, existe = TRUE;
	int puntuacion_ganador, puntuacion_perdedor, tiempo;
	query insercion;

	nombre jugador_ganador;	//encontramos el jugador que ha ganado la partida
	DevolverNombre(socket, jugador_ganador);

	nombre jugador_perdedor; //encontramos el jugador que ha perdido la partida
	if (!DevolverRival(jugador_ganador, jugador_perdedor))
		return FALSE;
	
	sscanf(in, "%d %d %d", &puntuacion_ganador, &puntuacion_perdedor, &tiempo);

	pthread_mutex_lock(&lock);
	id_partida = rand();				//random id generator
	if (ExisteIDpartida(id_partida)) {	//Si existe la ID, hay que cambiarla
		while (existe) {
			id_partida = rand();
			existe = ExisteIDpartida(id_partida);
		}
		existe = TRUE; //Como la ID random ahora no existe podemos decir que la ID nueva esta libre con lo cual puede existir.
	}
	if (existe) {
		char participantes[45];
		sprintf(participantes, "%s-%s", jugador_ganador, jugador_perdedor);
		
		//formateamos las puntuaciones
		char puntuacion[20];
		sprintf(puntuacion, "%d-%d", puntuacion_ganador, puntuacion_perdedor);

		sprintf(insercion, "INSERT INTO partidas (id, duracion, participantes, ganador, puntuacion) VALUES(%d, %d,'%s', '%s', '%s');", id_partida, tiempo, participantes, jugador_ganador, puntuacion);
		ConsultaBBDD(insercion);
		
		int id_ganador = EncontrarJugador(jugador_ganador);
		int id_perdedor = EncontrarJugador(jugador_perdedor);

		sprintf(insercion, "INSERT INTO relacion (id, idjug, puntuacion) VALUES (%d, %d, %d)", id_partida, id_ganador, puntuacion_ganador);
		sprintf(insercion, "INSERT INTO relacion (id, idjug, puntuacion) VALUES (%d, %d, %d)", id_partida, id_perdedor, puntuacion_perdedor);

		pthread_mutex_unlock(&lock);
		strcpy(output, "80/0");
		//Devuelve un 1
		return TRUE;
	}
	//Si no existe
	else {
		pthread_mutex_unlock(&lock);
		strcpy(output, "80/0");
		//Devuelve un 0
		return FALSE;
	}
}

//enviar que nos hemos desconectado
//En caso de tener exito devuelve un 1 y se envia el mensaje de desconexion
//Si hay algun error devuelve 0
int EnviarDesconectar(input in, buffer output, int socket){
	int resultado = 0;
	int socket_receptor = -1;
	sscanf(in, "%d", &resultado);
	
	nombre jugador_requester;
	DevolverNombre(socket, jugador_requester);	//Quienes somos nostros
	
	nombre jugador_rival;						//Quien es el rival
	if (DevolverRival(jugador_requester, jugador_rival)){
		socket_receptor = DevolverSocket(jugador_rival);
	}
	else
		return FALSE;	//no se ha encontrado el rival
	
	//ahora hay que enviarle al rival si ha fallado o ha acertado
	if (socket_receptor < 0) {
		strcpy(output, "43/0");
		//no se ha encontrado el socket del rival.
		return FALSE;
	}
	else {
		buffer msg;
		sprintf(msg, "21/%d", resultado);
		write(socket,"20/0",strlen("20/0"));
		write(socket_receptor, msg, strlen(msg));
		strcpy(output, "43/1");
		//todo correcto.
		return TRUE;
	}
}

//Comunica fin de partida y envia el mensaje
//Si hay algun error devuelve un 0
//En caso de poder hacerlo con exito devuelve un 0
int EnviarFinPartida(input in, buffer output, int socket){
	int resultado = 0;
	int socket_receptor = -1;
	sscanf(in, "%d", &resultado);
	
	nombre jugador_requester;
	DevolverNombre(socket, jugador_requester);	//Quienes somos nostros
	
	nombre jugador_rival;						//Quien es el rival
	if (DevolverRival(jugador_requester, jugador_rival)){
		socket_receptor = DevolverSocket(jugador_rival);
	}
	else
					  return FALSE;	//no se ha encontrado el rival
	
	//ahora hay que enviarle al rival si ha fallado o ha acertado
	if (socket_receptor < 0) {
		strcpy(output, "43/0");
		//no se ha encontrado el socket del rival.
		return FALSE;
	}
	else {
		buffer msg;
		sprintf(msg, "22/%d", resultado);
		write(socket,"22/0",strlen("22/0"));
		write(socket_receptor, msg, strlen(msg));
		strcpy(output, "43/1");
		//todo correcto.
		return TRUE;
	}
}
//ejecuta las funciones necesarias en funcion de un codigo recibido
void EjecutarCodigo(buffer buff, int socket, buffer output, int *start){
	input in;		//input
	buffer notif;	//notificacion
	int codigo;		//codigo
	codigo = DarCodigo(buff, in);
	
	//Peticion de log in
	if (codigo == 1){
		if(LogIN(in, output, socket)){
			printf("Socket %d se ha connectado.\n", socket);
			//Se conecta
			GetConnected(notif);
			int i;
			//Notificacion para la lista de conectados
			for(i = 0; i < usuarios->num; i++)
				write(usuarios->usuario[i].socket, notif, strlen(notif));
			printf("Enviamos:%s\n",notif);
		}
		//Informa de cualquier problema
		else
			printf("Hubo alg�n problema.\n");
	}
	//Peticion de sign up
	else if (codigo == 2){
		if(SignUP(in, output, socket)){
			printf("Socket %d se ha registrado y conectado.\n", socket);
			//Se conecta el jugador
			GetConnected(notif);
			//Notificacion para lista de conectados
			int i;
			for(i = 0; i < usuarios->num; i++)
				write(usuarios->usuario[i].socket, notif, strlen(notif));
		
		}
		else
			//Informa de cualquier error
			printf("Hubo alg�n problema.\n");
	}
	//Petcicion para saber puntuacion de qualquier jugador
	else if (codigo == 3){
		if(PuntosTotales(in, output)>0)
			printf("Success.\n");
		else
			printf("Hubo alg�n problema.\n");
	}
	//Petcicion para saber tiempo total de juego de qualquier jugador
	else if (codigo == 4){
		if(TiempoTotalJugado(in, output)>0)
			printf("Success.\n");
		else
			printf("Hubo alg�n problema.\n");
	}
	//Petcicion para saber numero de partidas ganadas de qualquier jugador
	else if (codigo == 5){
		if(PartidasGanadasVS(in, output) != 0)
			printf("Success.\n");
		else
			printf("Hubo alg�n problema.\n");
	}
	//Peticion para enviar invitacion
	else if (codigo == 7){
		if(EnviarInvitacion(in, output, socket))
			printf("Success.\n");
		else
			printf("Hubo algun problema.\n");
	}
	//Peticion para aceptar invitacion
	else if (codigo == 8){
		if(EnviarAceptar(in, output, socket))
			printf("Success.\n");
		
		else
			printf("Hubo algun problema.\n");
	}
	//Pericion para rechazo de la invitacion
	else if (codigo == 9){
		if(EnviarRechazo(in, output, socket))
			printf("Success.\n");
		else
			printf("Hubo algun problema.\n");
	}
	//Peticion para enviar mensaje a otro ususario
	else if (codigo == 13){
		if(EnviarMensaje(in, output, socket))
		printf("Success.\n");
		else 
			printf("Hubo algun problema.\n");
	}
	//Peticion para eliminar cuenta
	else if (codigo == 15){
		if(DeleteAccount(in, output, socket)){
			
			GetConnected(notif);
			//Notificacion para actualizar lista de conetados
			int i;
			for(i = 0; i < usuarios->num; i++)
				write(usuarios->usuario[i].socket, notif, strlen(notif));
			
			printf("Desconexi�n Socket %d\n", socket);
		}
		else printf("Hubo algun problema.\n");
	}
	//Notificacion para enviar posicion
	else if (codigo == 16){
		if(EnviarPosicion(in, output, socket))
			printf("Success.\n");
		else
			printf("Hubo algun error.\n");
	}
	//Notificacion para informar de prioridad de comienza
	else if (codigo == 17){
		if(EnviarPrioridad(in, output, socket))
			printf("Success.\n");
		else
			printf("Hubo algun error.\n");
	}
	//Peticion para informar del acierto del tiro
	else if (codigo == 18){
		if(EnviarAcierto(in, output, socket))
			printf("Success.\n");
		else
			printf("Hubo algun error.\n");
	}
	//Peticion para actualizar la partida
	else if (codigo == 19){
		if(ActualizarPartida(in, output, socket))
			printf("Success.\n");
		else
			printf("Hubo algun error.\n");
	}
	//Peticion para desconectar de la partida
	else if (codigo == 20){
		if(EnviarDesconectar(in, output, socket))
			printf("Success.\n");
		else 
			printf("Hubo algun problema.\n");
	}
	//Peticion para finalizar la partida
	else if (codigo==22){
		if(EnviarFinPartida(in, output, socket))
			printf("Success.\n");
		else 
			printf("Hubo algun problema.\n");
	}
	//Peticion para desconectarse del juego
	else if(codigo == 0){
		*start = FALSE;
		//quitar de la lista de conectados
		RemoveFromList(socket);
		//Preparar notificacion para actualizar lista de conectados de todos los ususarios
		GetConnected(notif);
		int i;
		for(i = 0; i < usuarios->num; i++)
			write(usuarios->usuario[i].socket, notif, strlen(notif));
		printf("Desconexi�n Socket %d\n", socket);
	}
}
//Funcion para ejecutar el thread
void *ThreadExecute (void *socket){
	buffer buff;
	buffer output;
	int sock_att, start = TRUE, ret;
	int *s;
	s = (int *)socket;
	sock_att = *s;
	while (start){
		ret = read(sock_att, buff, sizeof(buff));
		buff[ret] = '\0';
		printf("Recibida orden de Socket %d.\n", sock_att);
		printf("Pide: \n");
		puts(buff);
		EjecutarCodigo(buff ,sock_att, output, &start);
		if(start){
			printf("Enviamos: %s\n", output);
			write(sock_att, output, strlen(output));
		}
	}
}


int main(int argc, char *argv[]) {
	InitServer();
	InitBind();
	InitBBDD();
	InitTablaPartidas();
	
	int attendSocket, i = 0;
	// Atender las peticiones
	for( ; ; ){
		printf ("Escuchando\n");
		attendSocket= accept(serverSocket, NULL, NULL);
		printf ("He recibido conexion\n");
		//attendSockeet es el socket que usaremos para un cliente
		
		sock_atendedidos[i] = attendSocket;
		
		pthread_create(&thread[i], NULL, ThreadExecute, &sock_atendedidos[i]); //Thread_que_toque_usar, NULL, Nombre_de_la_funcion_del_thread, socket_del cliente
		i++;
	}
	
	CerrarBBDD();
}

