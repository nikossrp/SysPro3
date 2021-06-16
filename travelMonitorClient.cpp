#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/stat.h>
#include <sys/socket.h>                        /* sockets */
#include <netinet/in.h>               /* Internet sockets */
#include <netdb.h>                       /* gethostbyname */
#include <arpa/inet.h>
#include <dirent.h>
#include "project1/Application.h"
#include "TravelReuests.h"
#include "Send_Get.h"
#include <time.h>

#define Monitor "./monitorServer"


using namespace std;



int main (int argc, char* argv[]) {

    if (argc != 13) {
        cout << "Failure usage:./travelMonitorClient –m <monitorsServers> -b <socketBufferSize> -c <cyclicBufferSize> -s <sizeOfBloom> -i <input_dir> -t <numThreads>" << endl;
        exit(EXIT_FAILURE);
    }



    int numMonitors = 0;            //= atoi(numMonitors_c);
    int socketBufferSize_i = 0;     // = atoi(socketBufferSize_c);
    size_t socketBufferSize = 0;
    int cyclicBufferSize = 0;     //= atoi(cyclicBufferSize_c);
    int sizeOfBloom = 0;          //= atoi(sizeOfBloom_c);
    int numThreads = 0;           //= atoi(numThreads_c);
    char* input_dir = NULL;          //= "input_dir2";

    char socketBufferSize_c[10] = "";
    char cyclicBufferSize_c[10] = "";
    char numThreads_c[10] = "";
    char sizeOfBloom_c[10] = "";

    


    for (int i = 0; i < argc; i++) {

        if (!strcmp(argv[i], "-s")) {
            sizeOfBloom = atoi(argv[++i]);
            if (sizeOfBloom <= 0 ) {
                cout << "Error: sizeOfBloom, it shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

        if (!strcmp(argv[i], "-b")) {
            socketBufferSize_i = atoi(argv[++i]);
            socketBufferSize = socketBufferSize_i;
            if (socketBufferSize <= 0 ) {
                cout << "Error: socketBufferSize, it shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

        if (!strcmp(argv[i], "-m")) {
            numMonitors = atoi(argv[++i]);
            if (numMonitors <= 0 ) {
                cout << "Error: number of Monitors, it shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }


        if (!strcmp(argv[i], "-i")) {
            input_dir = strdup(argv[++i]);
        }

        if (!strcmp(argv[i], "-t")) {
            numThreads = atoi(argv[++i]);
            if (numThreads <= 0 ) {
                cout << "Error: number of threads, it shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

        if (!strcmp(argv[i], "-c")) {
            cyclicBufferSize = atoi(argv[++i]);
            if (cyclicBufferSize <= 0 ) {
                cout << "Error: number of cyclicBufferSize, it shouldn't be negative or zero\n";
                exit(EXIT_FAILURE);
            }
        }

    }


    sprintf(sizeOfBloom_c, "%d", sizeOfBloom);
    sprintf(numThreads_c, "%d", numThreads);
    sprintf(cyclicBufferSize_c, "%d", cyclicBufferSize);
    sprintf(socketBufferSize_c, "%d", socketBufferSize_i);





    Dirs dirs;
    int n_dir, length;
    int sock, newSock, enable = 1;  
    int numDirs_monitor = 0;
    in_port_t ports[numMonitors];
    pid_t child_pids[numMonitors];
    int sockets[numMonitors+1];
    char hostbuffer[256];
    int hostname;
    string all_directories_str = "";
    struct hostent *rem;
    struct sockaddr_in server, client;
    struct sockaddr *serverptr = (struct sockaddr*) &server;
    struct sockaddr *clientptr = (struct sockaddr*) &client;

    //here gather all blomfilters 
    HashTable* bloomfilters = new HashTable(MAXSIZE, "BloomFilter");

    //gather all information from socket, then decrypt, 
	char* info   = (char*) malloc(2000 * sizeof(char)); 
    //every time we get socketBufferSize bytes from sockets
    char* msgbuf = (char*) malloc(sizeof(char) * (socketBufferSize+1));

	memset(info, '\0', 2000);
	memset(msgbuf, '\0', socketBufferSize+1);



    n_dir = n_directories(input_dir);


    //if given more monitors than directories
    if (n_dir < numMonitors) {
        cout << "Number of monitors is more than number of directories, number of monitors after update is " << n_dir << endl;
        numMonitors = n_dir;
    }

    dirs = get_dir_per_Monitor(input_dir, numMonitors);  //get with Round Rubin countries for every monitor
    dirs.nMonitors = numMonitors;



    // get the current hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname < 0) {
        perror("hostname");
        exit(1);
    }

	/* Find server address */
    if ((rem = gethostbyname(hostbuffer)) == NULL) {	
	   herror("gethostbyname"); exit(1);
    }

    time_t begin = time(NULL);
    time_t current_time;
    bool watting = false;

    

    for (int i = 0; i < numMonitors; i++) {


        ports[i] = 8000 + i ;     //save port, (ports <= 1024 is only for root)

        if ((child_pids[i]=fork()) == 0) {  //child process

            numDirs_monitor = dirs.ndirs[i]-1;
            int numArgs = numDirs_monitor + 12; //10 is the number of argument without paths,  +1 for NULL arg and +1 for ./monitorServer
            int numPaths = dirs.ndirs[i]-1;
            char** paths = dirs.dir_per_process[i];
            char** all_args = (char**) malloc((numArgs)*sizeof(char*));  //arguments for monitorServers
            char port_c[10];
            memset(port_c, '\0', 10);
            sprintf(port_c, "%d", ports[i]);

            // make arguments for monitor without paths
            all_args[0] = strdup(Monitor);
            all_args[1] = strdup("-p");
            all_args[2] = strdup(port_c);
            all_args[3] = strdup("-t");
            all_args[4] = strdup(numThreads_c);
            all_args[5] = strdup("-b");
            all_args[6] = strdup(socketBufferSize_c);
            all_args[7] = strdup("-c");
            all_args[8] = strdup(cyclicBufferSize_c);
            all_args[9] = strdup("-s");
            all_args[10] = strdup(sizeOfBloom_c);

            int index_args = 11;
            all_args[numArgs-1] = NULL;

            //insert in array the paths after "standard" arguments
            for (int j = 0; j < numPaths; j++) {
                all_args[index_args++] = dirs.dir_per_process[i][j];
            }


            execv(all_args[0], all_args);
            perror("execv");
            exit(1);
        }
        else {  
            //parent get bloomFilter (?)

            int connectStatus = 0;

            /* Create socket */
            if ((sockets[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("TravelMonitor - socket");
                exit(1);
            }

            memset(&server, 0, sizeof(server));
            server.sin_family = AF_INET;
            memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
            server.sin_port = htons(ports[i]);
            
            /* Initialize connection, when monitor is ready to accept connection */
            do {
                connectStatus = connect(sockets[i], serverptr, sizeof(server));
            }
            while (connectStatus < 0);

        }

        current_time = time(NULL) - begin;
        if ((current_time > 2) && !watting) {
            cout << "\n\nPlease wait, this may take some time....\n\n";
            watting = true;
        }
    }

    //~~~~~~~~~ get bloomFilters!! (?) ~~~~~~~~~~~~

    /*           Protocol
    * First we get the number of bytes for virusName
    * Read virus name with bufferSize bytes each time.
    * Read bloomFilter (must read sizeOfBloom) bytes
    */

    struct pollfd pfds[numMonitors];

    //initialize poll
    for (int i = 0; i < numMonitors; i++) {
        pfds[i].fd = sockets[i];
        pfds[i].events = POLLIN;
    }


    int answers = numMonitors;

    // wait the faster monitor to send bloomfilter
    while(answers > 0) {
        int ready;
        ready = poll(pfds, numMonitors, -1);
        if (ready == -1) {
            perror("poll timeout");
            exit(11);
        }

        for (int i = 0; i < numMonitors; i++) {
            if (pfds[i].revents == POLLIN) {
                get_blooms(bloomfilters, pfds[i].fd, socketBufferSize, sizeOfBloom);
                answers--;
            }
        }
    }


    //gather all directories in a string, we need that to create logfile.pid
    for (int i = 0; i < numMonitors; i++) {
        length = dirs.ndirs[i] - 1;
        for (int j = 0; j < length; j++) {
            string dir_i(dirs.dir_per_process[i][j]);
            all_directories_str += dirs.dir_per_process[i][j];
            all_directories_str += " ";
        }
    }



    char* start_info = info;
    memset(msgbuf, '\0', socketBufferSize);
    string line_str = "";
    char* c_line = NULL;
    int nWords = 0;
    
    TravelRequests* travelStats = new TravelRequests(MAXSIZE);

    int total_request = 0;
    int accepted = 0;
    int rejected = 0;


    cout << "\n*************** ENTER COMMANDS ***************\n";
    while(1) {
        
        memset(info, '\0', 2000);
        cout << endl;
        getline(cin, line_str);
        c_line = strdup(line_str.c_str());
        nWords = countWords(c_line);



        char* command = strtok(c_line, " ");


        if (line_str.empty()) {      // note: getline not read '\n'
            free(c_line);
            continue;
        }


        if (!strcmp(command, "/exit")) {
            for(int i = 0; i < numMonitors; i++) {  // terminate children
                strcpy(info, "exit#");
                info = send_string(info,  sockets[i], msgbuf, socketBufferSize);

                wait(NULL);     //wait child terminate
            }
            creating_logFile(all_directories_str, total_request, accepted, rejected);
            free(c_line);
            break;
        }


        if ((strcmp(command, "/exit") && strcmp(command, "/travelRequest") &&
            strcmp(command, "/travelStats") && strcmp(command, "/addVaccinationRecords") &&
            strcmp(command, "/searchVaccinationStatus") && strcmp(command, "/list-nonVaccinated-Persons")) || nWords == 1) {
            cout << "ERROR COMMAND...type one of the following commands:\n";
            cout << "/travelRequest citizenID date countryFrom countryTo virusName\n";
            cout << "/travelStats virusName date1 date2 [country]\n";
            cout << "/addVaccinationRecords country\n";
            cout << "/searchVaccinationStatus citizenID\n";
            cout << "/exit" << endl;
            free(c_line);
            continue;
        }



        // /travelRequest citizenID date countryFrom countryTo virusName
        if (!strcmp(command, "/travelRequest")) {
            if (nWords != 6) {
                free(c_line);
                cout << "ERROR COMMAND\n";
                cout << "Try: /travelRequest citizenID date countryFrom countryTo virusName\n";
                continue;
            }


            Date* travel_date;
            char* id = strtok(NULL, " ");
            char* c_date = strtok(NULL, " ");
            char* countryFrom = strtok(NULL, " ");
            char* countryTo = strtok(NULL, " ");
            char* virusName = strtok(NULL, " ");

            int day = atoi( strtok(c_date, "-") );
            int month = atoi( strtok(NULL, "-") );
            int year = atoi( strtok(NULL, "-") );

            travel_date = new Date(day, month, year);
            char* date_c = travel_date->get_date_charStar();

            if (!consistentDate_str(date_c) || !consistentDate(travel_date)) {
                cout << "Invalid date format " << *travel_date << endl;
                free(c_line);
                delete travel_date;
                free(date_c);
                continue;
            }

            free(date_c);

            BloomFilter* bvirus = (BloomFilter*)bloomfilters->get_item(virusName);

            int monitor = get_Monitor_for_country(dirs, countryFrom);

            if (monitor == -1) {
                cout << "Aplication hasn't records for country: " << countryFrom << endl;
                free(c_line);
                delete travel_date;
                continue;
            }

            total_request++;

            if (bvirus != NULL) {
                int ck = bvirus->check_Record(id);

                if (ck == 0) {
                    travelStats->InsertRequest(virusName, countryTo, ck, travel_date);
                    cout << "REQUEST REJECTED – YOU ARE NOT VACCINATED\n";
                    rejected++;
                }
                else {

                    memset(info, '\0', 2000);
                    strcpy(info, id);
                    strcat(info, "$");          //end of id
                    strcat(info, virusName);
                    strcat(info, "#");          //end of string



                    // send request 
                    info = send_string(info, sockets[monitor], msgbuf, socketBufferSize);


                    //get result in info
                    info = get_string(info, sockets[monitor], msgbuf, socketBufferSize);

                    if (contains(info, "None")) {
                        cout << "No match id " << id << " with country " <<  countryFrom << endl;
                        free(c_line);
                        delete travel_date;
                        continue;
                    }


                    char* answer = strdup(info);
                    char* yes_no = strtok(answer, " ");
                    memset(info, '\0', 2000);   

                    //checks if date is less than 6 months before the desired date
                    if (!strcmp(yes_no, "YES")) {
                        int day = atoi( strtok(NULL, "-") );
                        int month = atoi( strtok(NULL, "-") );
                        int year = atoi( strtok(NULL, "-") );
                        Date* vacc_date = new Date(day, month, year);

                        int acc = vacc_date->accepted_date6Months(travel_date);
                        if ( acc == 1) {  
                            travelStats->InsertRequest(virusName, countryTo, acc, travel_date);
                            cout << "REQUEST ACCEPTED – HAPPY TRAVELS\n";
                            accepted++;
                        }
                        else if (acc == 0) {
                            travelStats->InsertRequest(virusName, countryTo, acc, travel_date);
                            cout << "REQUEST REJECTED – YOU WILL NEED ANOTHER VACCINATION BEFORE TRAVEL " << *travel_date << endl;
                            rejected++;
                        }

                        delete vacc_date;
                    }
                    else {
                        travelStats->InsertRequest(virusName, countryTo, false, travel_date);
                        cout << "REQUEST REJECTED – YOU ARE NOT VACCINATED\n";
                        rejected++;
                    }

                    free(answer);
                }
            }
            else {
                cout << "No records for " << virusName << endl;
                total_request--;
            }

            delete travel_date;

        }


    //  /travelStats virusName date1 date2 [country]
        if (!strcmp(command, "/travelStats")) {
            if (nWords != 5 && nWords != 4) {
                free(c_line);
                cout << "ERROR COMMAND\n";
                cout << "Try: /travelStats virusName date1 date2 [country]\n";
                continue;
            }

            char* virusName = strtok(NULL, " ");
            char* c_date1 = strtok(NULL, " ");
            char* c_date2 = strtok(NULL, " ");

            if (total_request > 0) {
                BloomFilter* virus = (BloomFilter*)bloomfilters->get_item(virusName);

                if (virus != NULL) {

                    if (nWords == 5) {

                        char* countryTo = strtok(NULL, "");
                        int d1 = atoi( strtok(c_date1, "-") );
                        int m1 = atoi( strtok(NULL, "-") );
                        int y1 = atoi( strtok(NULL, "") );

                        Date* date1 = new Date(d1, m1, y1);

                        int d2 = atoi( strtok(c_date2, "-") );
                        int m2 = atoi( strtok(NULL, "-") );
                        int y2 = atoi( strtok(NULL, "") );

                        Date* date2 = new Date(d2, m2, y2);

                        // travel requests for countryTo
                        travelStats->TravelStats(virusName, date1, date2, countryTo);

                        delete date1;
                        delete date2;
                    }
                    else if (nWords == 4) {

                        int d1 = atoi( strtok(c_date1, "-") );
                        int m1 = atoi( strtok(NULL, "-") );
                        int y1 = atoi( strtok(NULL, "") );

                        Date* date1 = new Date(d1, m1, y1);

                        int d2 = atoi( strtok(c_date2, "-") );
                        int m2 = atoi( strtok(NULL, "-") );
                        int y2 = atoi( strtok(NULL, "") );

                        Date* date2 = new Date(d2, m2, y2);

                        // travel requests for all countries together
                        travelStats->TravelStats(virusName, date1, date2);

                        //travel requests per country
                        // travelStats->TravelStats_per_country(virusName, date1, date2);
    
                        delete date1;
                        delete date2;
                    }

                }
                else {
                    cout << "No records for " << virusName << endl;
                }
            }

        }

        // /addVaccinationRecords country 
        if (!strcmp(command, "/addVaccinationRecords")) {
            if (nWords != 2) {
                free(c_line);
                cout << "ERROR COMMAND\n";
                cout << "Try: /addVaccinationRecords country\n";
                continue;
            }

            char* country = strtok(NULL, "");

            //get the child who manages the country
            int monitor = get_Monitor_for_country(dirs, country);
            if (monitor == -1) {
                cout << "Aplication hasn't records for country: " << country << endl;
                free(c_line);
                continue;
            }

            strcpy(info, "updateDataset#");
            info = send_string(info, sockets[monitor], msgbuf, socketBufferSize);
            get_blooms(bloomfilters, sockets[monitor], socketBufferSize, sizeOfBloom);
            cout << "Dataset updated\n";

        }



        // /searchVaccinationStatus citizenID
        if (!strcmp(command, "/searchVaccinationStatus")) {
            if (nWords != 2) {
                free(c_line);
                cout << "ERROR COMMAND\n";
                cout << "Try: /searchVaccinationStatus citizenID\n";
                continue;
            }

            //send citizenID to all Monitors
            char* citizenID = strtok(NULL, "");
            int get_results = 0;
            char* search_id = (char*) malloc(100 * sizeof(char));
            strcpy(search_id, "search$");
            strcat(search_id, citizenID);
            strcat(search_id, "#");

            char* results = (char*) malloc(sizeof(char) * 1000);
            memset(results, '\0', 1000);

            for(int j = 0; j < numMonitors; j++) {
                send_string(search_id, sockets[j], msgbuf, socketBufferSize);
                pfds[j].events = POLLIN;
            }   


            // using poll to detect message from the right child
            int num_answers = numMonitors;
            while(num_answers > 0) {
                int ready;
                ready = poll(pfds, numMonitors, 300);
                if (ready == -1) {
                    perror("poll");
                    exit(11);
                }
                if (ready == 0) {       
                    // poll time out
                    break;
                }

                for(int k = 0; k < numMonitors; k++) {
                    if (pfds[k].revents == POLLIN) {
                        memset(info, '\0', 2000);
                        info = get_string(info, sockets[k], msgbuf, socketBufferSize);

                        if (!contains(info, "None")) {
                            strcpy(results, info);
                            get_results = 1;
                        } 
                        num_answers--;
                    }
    
                }
            }

            if (contains(results, "#")) {
                char* output = strtok(results, "#");
                cout  << output << endl;
            }
            else {
                cout << "No records for citizen with id: " << citizenID << endl;
            }

            free(search_id);
            free(results);
        }



        free(c_line);
    }





    //close all sockets
    for (int i = 0; i < numMonitors; i++)
        close(sockets[i]);



/////////////////////////////////////////////////////////////////////////
///////////////////////// free structs/classes /////////////////////////
////////////////////////////////////////////////////////////////////////

    // free virusName / array for every bloomfilter
    for (int i = 0; i < bloomfilters->get_nNodes(); i++) {
        BloomFilter* bloom = (BloomFilter*)bloomfilters->get_i_Item(i);
        free(bloom->get_virusName());
    }

    free(input_dir);
    
    delete bloomfilters;
    delete travelStats;
    
    free(msgbuf);
    free(start_info);

    // free dirs
    for (int i = 0; i < numMonitors; i++) {
        length = dirs.ndirs[i] - 1;

        for (int j = 0; j < length; j++) {
            free(dirs.dir_per_process[i][j]);
        }
        free(dirs.dir_per_process[i]);
    }
    free(dirs.dir_per_process);
    free(dirs.ndirs);

    cout << "\nTravelMonitor terminated successfully\n\n\n";

    return 0;
}





