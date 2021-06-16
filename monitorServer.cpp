#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <string.h>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <netdb.h>	         /* gethostbyaddr */
#include <arpa/inet.h>
#include <pthread.h>        //posix threads: compile with argument -lpthread
#include "project1/Application.h"
#include "Send_Get.h"
#include "CyclicBuffer.h"

int inserted_files;

#define perror2(s, e)  fprintf(stderr, "%s: %s\n", s, strerror(e))


using namespace std;



pthread_mutex_t mtx;

pthread_cond_t cond_nonempty;           /* Condition variable non full */
pthread_cond_t cond_nonfull;           /* Condition variable for non empty */


CyclicBuffer* cyclicBuffer;
int terminated_threads;
App* app;           //here is all records
int nFiles_temp;


HashTable* get_update_blooms_app(App* app, Array* all_files, Array* directories);
bool new_fileName(Array* files, char* check_fileName);


void *Insert_File(void *);        /* Forward declaration */
void pthread_lock(pthread_mutex_t mtx);
void pthread_unlock(pthread_mutex_t mtx);
void place(char* file);
char* obtain();



int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: ./Monitor myfifo\n";
		return 1;
	}



	int numMonitors = 0;
    int socketBufferSize = 0;
    int cyclicBufferSize = 0;
    int sizeOfBloom = 0;
    int numThreads = 0;
	int port = 0;

	/*  
	monitorServer -p port -t numThreads -b socketBufferSize -c cyclicBufferSize -s 
	sizeOfBloom path1 path2 ... pathn
	*/
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-p")) {
            port = atoi(argv[++i]);
            if (port < 0 ) {
                cout << "Error: port shouldn't be negative\n";
                exit(EXIT_FAILURE);
            }
        }

		if (!strcmp(argv[i], "-t")) {
            numThreads = atoi(argv[++i]);
            if (numThreads <= 0 ) {
                cout << "Error: number of threads shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

        if (!strcmp(argv[i], "-b")) {
            socketBufferSize = atoi(argv[++i]);
            if (socketBufferSize <= 0 ) {
                cout << "Error: socketbufferSize shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

		if (!strcmp(argv[i], "-c")) {
            cyclicBufferSize = atoi(argv[++i]);
            if (cyclicBufferSize <= 0 ) {
                cout << "Error: cyclicBufferSize shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

        if (!strcmp(argv[i], "-s")) {
            sizeOfBloom = atoi(argv[++i]);
            if (sizeOfBloom <= 0 ) {
                cout << "Error: sizeOfBloom shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }
    }

	int index_paths = 11;	//10 is the number of arguments without paths
	int numPaths = argc - 11;
    int nFiles;
    Array* paths_arr; 
    string all_directors_str = "";
    
    char* msgbuf = (char*) malloc(sizeof(char) * (socketBufferSize+1));   
	char* info = (char*) malloc(2000 * sizeof(char));   //gather all information here
    cyclicBuffer = new CyclicBuffer(cyclicBufferSize);
    app = new App(sizeOfBloom, MAXSIZE);
	memset(info, '\0', 2000);
	memset(msgbuf, '\0', socketBufferSize+1);

    //get all paths
    char** paths = (char**) malloc(sizeof(char*) * numPaths);
    for (int i = 0; i < numPaths; i++) {
        paths[i] = strdup(argv[index_paths++]);
        string dir_str(paths[i]);
        all_directors_str += dir_str;
        all_directors_str += " ";
    }

    //get all paths in an array (we will use that to update Dataset)
    paths_arr = get_token_array(all_directors_str, " ");

    //get all files from paths, cyclicbuffer gona take files from here
    Array* files_bigBuffer = get_all_files(paths, numPaths); 
    nFiles_temp = files_bigBuffer->length;
    nFiles = nFiles_temp;


    /*------------- Threads -------------*/

    index_paths = 0;
    int temp_numThreads = numThreads;
    terminated_threads = numThreads;
    pthread_t thr[numThreads]; int err;
	pthread_mutex_init(&mtx, 0);

    pthread_cond_init(&cond_nonempty, 0);
	pthread_cond_init(&cond_nonfull, 0);


    // Create numThreads, insert in App the first cyclicBufferSize files
    for (int i = 0; i < temp_numThreads; i++) {
        if (err = pthread_create(&thr[i], NULL, Insert_File, NULL)) { // New thread
            perror2("pthread_create", err);
            exit(1);
        }
    } 

    inserted_files = 0;
    //build application for requests
    while(inserted_files < nFiles) {
        if (nFiles_temp > 0) {
            place(files_bigBuffer->arr[index_paths]);
        }
        nFiles_temp--;
        index_paths++;
        pthread_cond_signal(&cond_nonempty);
        usleep(3000);
    }

    // app->print_Records();


    /* ----------------- sockets ----------------*/
    struct hostent *rem;
    int sock, newsock;
    int enable = 1;     
    
    struct sockaddr_in server, client;
    struct sockaddr* serverptr = (struct sockaddr* )& server;
    struct sockaddr* clientptr = (struct sockaddr* )& client;
    socklen_t clientlen = sizeof(client);


    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket - server");
        exit(1);
    }

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);

    // make sure socket will be valid after aplication terminate
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    if (bind(sock, serverptr, sizeof(server)) < 0) {
        perror("bind - server");
        exit(1);
    }

    if (listen(sock, 5) < 0) {
        perror ("listen - server");
        exit(1);
    }

    // wait client to connect
    if ((newsock = accept(sock, clientptr, &clientlen)) < 0) {
        perror("accept");
        exit(1);
    }
    close(sock);



    // send bloomfilters
	HashTable* bloomfilters = app->get_BloomFilters();
    send_blooms(bloomfilters, newsock, socketBufferSize, sizeOfBloom);



    char *id, *virus, *results;
    char *garbage;
    int accepted, rejected, total_rq;
    accepted = rejected = total_rq = 0;

    // cout << "Monitor " << getpid() << "waitting for request" << endl;
    // wait for requests from travelMonitor - client
    while(1) {
        memset(info, '\0', 2000);
        info = get_string(info, newsock, msgbuf, socketBufferSize);


        if (contains(info, "exit")) {
            creating_logFile(all_directors_str, total_rq, accepted, rejected);
            break;

        }else  if (contains(info, "search")) { 
            // /searchVaccinationStatus

            garbage = strtok(info, "$");
            id = strtok(NULL, "#");
            results = app->vaccineStatus(id);

            strcat(results, "#");
            results = send_string(results, newsock, msgbuf, socketBufferSize);
            free(results);

        }
        else if (contains(info, "updateDataset")) {
            // /addVaccinationRecords

            HashTable* new_bloomfilters = get_update_blooms_app(app, files_bigBuffer, paths_arr);
            send_blooms(new_bloomfilters, newsock, socketBufferSize, sizeOfBloom);
        }
        else {  
            // /travelRequest

            id = strtok(info, "$");
            virus = strtok(NULL, "#");

            results = app->vaccineStatus(id, virus);

            if (contains(results, "YES")) {
                accepted++;
            }
            else if (contains(results, "NO")) {
                rejected++;
            }
            total_rq++;

            strcat(results, "#");
            results = send_string (results, newsock, msgbuf, socketBufferSize);
            free(results);
        }
    }
    close(newsock);






    for (int i = 0; i < files_bigBuffer->length; i++) {
        free(files_bigBuffer->arr[i]);
    }
    free(files_bigBuffer->arr);
    free(files_bigBuffer);


    for (int i = 0; i <= paths_arr->length; i++) {
        free(paths_arr->arr[i]);
    }
    free(paths_arr->arr);
    free(paths_arr);


    for (int i = 0; i < numPaths; i++) {
        free(paths[i]);
    }
    free(paths);


    /*---- free pthread_mutex/cond ----*/

    char* vexit = strdup("exit");

    //info all threads, main thread want to terminate
    while(terminated_threads > 0) {
        place(vexit);
        pthread_cond_signal(&cond_nonempty);
        usleep(300);
    }
    free(vexit);

    // wait threads to terminate
    for (int i = 0; i < numThreads; i++) {
        pthread_join(thr[i], NULL);
    }

    pthread_cond_destroy(&cond_nonempty);
    pthread_cond_destroy(&cond_nonfull);
    pthread_mutex_destroy(&mtx);

    free(info);
    free(msgbuf);
    
    delete cyclicBuffer;
    delete app;

    return 0;
}



void *Insert_File(void *m) {        /* Forward declaration */
    int err;


    while(1) {

        if (err = pthread_mutex_lock(&mtx)) { /* Lock mutex */
            perror2("pthread_mutex_lock", err); 
            exit(1); 
        }

        while (cyclicBuffer->isEmpty()) {
            // printf (">> Found Buffer Empty \n");
            pthread_cond_wait (&cond_nonempty, &mtx);
        }

        char* file = cyclicBuffer->get_next_file();

        if (!strcmp(file, "exit")) {     
            //monitor want to terminate
            terminated_threads--;
            free(file);
            break;
        }


        string fileName(file);
        string line_str;
        char* c_line;
        string id;
   
        ifstream fileName_in(fileName);

        if (fileName_in) {

            while(getline(fileName_in, line_str)) {
                c_line = strdup(line_str.c_str());
                app->Insert_Record(c_line);
            }
        }
        else 
            perror("open file");

        free(file);

        if (err = pthread_mutex_unlock(&mtx)) { /* Unlock mutex */
            perror2("pthread_mutex_unlock", err); 
            exit(1); 
        }
		
        inserted_files++;
        pthread_cond_signal(&cond_nonfull);

        usleep(3000);
    }

    if (err = pthread_mutex_unlock(&mtx)) { /* Unlock mutex */
        perror2("pthread_mutex_unlock", err); 
        exit(1); 
    }

    pthread_cond_signal(&cond_nonfull);

    pthread_exit(NULL); 
}





void place(char* file) {
    int err;

    if (err = pthread_mutex_lock(&mtx)) { /* Lock mutex */
        perror2("pthread_mutex_lock", err); 
        exit(1); 
    }

	while (cyclicBuffer->isFull()) {
		// printf(">> Found Buffer Full numItems: %d \n", cyclicBuffer->get_numFiles());
		pthread_cond_wait(&cond_nonfull, &mtx);
    }

    cyclicBuffer->Insert(file);
    
    if (err = pthread_mutex_unlock(&mtx)) { /* Unlock mutex */
        perror2("pthread_mutex_unlock", err); 
        exit(1); 
    }
}



void pthread_lock(pthread_mutex_t mtx)
{
    int err;
    if (err = pthread_mutex_lock(&mtx)) { /* Lock mutex */
        perror2("pthread_mutex_lock", err); 
        exit(1); 
    }
}


void pthread_unlock(pthread_mutex_t mtx)
{
    int err;
    if (err = pthread_mutex_unlock(&mtx)) { /* Unlock mutex */
        perror2("pthread_mutex_unlock", err); 
        exit(1); 
    }
}


HashTable* get_update_blooms_app(App* app, Array* all_files, Array* paths_arr) 
{
    struct dirent **files;
	string line_str;
	string id;
	char* c_line;
	int n;

	for(int i = 0; i < paths_arr->length; i++) {

		n = scandir(paths_arr->arr[i], &files, 0, alphasort);
		if (n < 0) {
			perror("scandir");
			exit(12);
		}

		for(int j = 2; j < n; j++) {	//find the new files
			string fileName(paths_arr->arr[i]);
			fileName = fileName + "/" + files[j]->d_name;
            char* fileName_c = strdup(fileName.c_str());

			if (new_fileName(all_files, (char*)fileName.c_str())) {
                //insert new files in cyclicBuffer, 
                //also let threads read them
				place(fileName_c); 
                pthread_cond_signal(&cond_nonempty);
                usleep(3000);
			}

            free(fileName_c);
        }

        for (int i = 0; i < n; i++) {
            free(files[i]);
        }
		free(files);
	}	

	return app->get_BloomFilters();
}

bool new_fileName(Array* files, char* check_fileName)
{
	for(int i = 0; i < files->length; i++) {
		if (!strcmp(files->arr[i], check_fileName)) {
			return false;
		}
	}

	return true;
}