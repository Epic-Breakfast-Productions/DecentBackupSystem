/*
	Decent Backup System Server
	
	Copyright 2016 Epic Breakfast Productions
		
	Author: Greg Stewart
*/


//includes
#include <iostream>//to do console stuff
#include <fstream>//to do file stuff
#include <time.h>//for time measurment
#include <string>//for strings
#include <sys/stat.h>//for checking filepaths
#include <stdlib.h>

//usings
using namespace std;

/**************
**	Globals  **
**************/
const char delimeter = ':';
const string version = "0.0.1";//version number
string logFileLoc = "DBSS_LOG.txt";//the log file
string confFileLoc = "DBSS_CONF.txt";//the config file

string syncFolderPred = "DBSS_sync_";// the filename predicate for sync folders
string storFolderPred = "DBSS_stor_";// the filename predicate for storage folders

string syncFolderLoc = syncFolderPred + "dir";
string storFolderLoc = storFolderPred + "dir";

int checkInterval = 10 * 1000;//for minutes

/* Option Flags */
bool runFlag = false;
bool helpFlag = false;
bool listFlag = false;
bool verbose = false;
bool setToStartOnStart = false;


bool checkFilePath(string pathIn, bool dir){
    //sendDebugMsg("Path Given: " + pathIn);
    bool worked = false;//if things worked
    struct stat pathStat;//buffer for the stat
    //check if valid 
    if(worked = (lstat(pathIn.c_str(), &pathStat) == 0)){
        //sendDebugMsg("path is present");
        //check if a file or directory
        if((S_ISDIR(pathStat.st_mode)) && dir){
            //sendDebugMsg("path is dir");
            worked = true;
        }else if((pathStat.st_mode && S_IFREG) && !dir){
            //sendDebugMsg("path is file");
            //check if valid filetype
            worked = true;
        }else{
            //sendDebugMsg("path is not recognized");
            worked = false;
        }
    }else{//if valid path
        //sendDebugMsg("path is invalid");
        worked = false;
    }
    /*
    if(debugging & !worked){
        sendDebugMsg("checking file path \"" + pathIn + "\" failed.");
    }else if(debugging){
        sendDebugMsg("path given is valid");
    }
    */
    
    return worked;
}//checkFilePath(string)

string getTimestamp(){
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [80];
	time (&rawtime);
	timeinfo = localtime (&rawtime);
	strftime (buffer,80,"%F %I:%M:%S %p",timeinfo);
	return (string)buffer;
}//getTimestamp

void outputText(char type, string message, bool verbosity){
	if(type == 'c'){//console output
		if(verbosity){
			cout << message << endl;
		}
	}else if(type == 'l'){
		ofstream confFile (logFileLoc.c_str(), ios::app);
		confFile << getTimestamp() << " - " << message << endl;
		confFile.close();
	}
}//outputText

void generateFolderConfig(string configLoc){
	//TODO:: this
}

void readFolderConfig(string configLoc){
	//TODO:: this
}

int runServer(){
	//TODO:: this
}//runServer

void generateMainConfig(){
	ofstream confFile (confFileLoc.c_str(), ios::app);
	if(!confFile.good()){
		outputText('c', "ERROR:: Unable to create or open config file.", true);
		return;
	}
	confFile << "logFileLoc" << delimeter << logFileLoc.c_str() << endl <<
		"syncFolderPred" << delimeter << syncFolderPred.c_str() << endl <<
		"syncFolderLoc" << delimeter << syncFolderLoc.c_str() << endl <<
		"storFolderPred" << delimeter << storFolderPred.c_str() << endl <<
		"storFolderLoc" << delimeter << storFolderLoc.c_str() << endl << 
		"checkInterval" << delimeter << checkInterval;
	confFile.close();
	return;
}//generateConfig()

void readMainConfig(){
	ifstream confFile (confFileLoc.c_str()); // declare file stream: http://www.cplusplus.com/reference/iostream/ifstream/
	string variable, value;
	
	while ( confFile.good() ){
		getline ( confFile, variable, delimeter );
		getline ( confFile, value, '\n' );
		//cout << "\"" << variable << "\", \"" << value << "\"" << endl;
		if(variable == "logFileLoc"){
			logFileLoc = value;
		}else if(variable == "syncFolderPred"){
			syncFolderPred = value;
		}else if(variable == "syncFolderLoc"){
			syncFolderLoc = value;
		}else if(variable == "storFolderPred"){
			storFolderPred = value;
		}else if(variable == "storFolderLoc"){
			storFolderLoc = value;
		}else if(variable == "checkInterval"){
			checkInterval = atoi(value.c_str());
		}
		
	}
	
	confFile.close();
}//readConfig()

bool setRunOnStart(){
	//ofstream log (logFile.c_str(), ios::out | ios::app);
	//TODO:: this
}//setRunOnStart()



int main(int argc, char *argv[]){
	if(argc < 2){//if got no arguments, output usage
		cout << "Usage:" << endl <<
			argv[0] << " <command> [command]..." << endl <<
			"List commands: -h" << endl;
		return 1;
	}
	
	/*
		Check for options (set flags)
	*/
	//cout << "getting inputs..." << endl;//debugging
	int curArg = 1;
	while(curArg < argc){
		//cout << "\t\"" << argv[curArg] << "\" ";//debugging
		if(string(argv[curArg]) == "-h"){
			//cout << "displaying help" << endl;//debugging
			helpFlag = true;
		}else if(string(argv[curArg]) == "-r"){
			//cout << "running server" << endl;//debugging
			runFlag = true;
		}else if(string(argv[curArg]) == "-l"){
			//cout << "listing folders" << endl;//debugging
			listFlag = true;
		}else if(string(argv[curArg]) == "-v"){
			//cout << "being verbose" << endl;//debugging
			verbose = true;
		}else if(string(argv[curArg]) == "-c"){
			//cout << "Changing config location." << endl;//debugging
			curArg++;
			confFileLoc = string(argv[curArg]);
			outputText('c', "Config file changed to: \"" + confFileLoc + "\"", true);
		}else if(string(argv[curArg]) == "-s"){
			setToStartOnStart = true;
		}else{//debugging
			//cout << endl;
		}
		curArg++;
	}
	//cout << "Done." << endl;
	~curArg;
	
	if(helpFlag){
		cout << endl << 
			"Decent Backup System Server" << endl <<
			"\tCopyright 2016 Epic Breakfast Productions" << endl <<
			"\tVersion: " << version << endl << endl <<
			"Commands:" << endl <<
			"\t-h        Display help." << endl <<
			"\t-r        Run the server." << endl <<
			"\t-v        Verbose output." << endl <<
			"\t-l        List files and folders used." << endl <<
			"\t-s        Set this to be run on startup." << endl <<
			"\t-c <loc>  Specify location of config file." << endl << endl;
		return 0;
	}
	
	/**
		Read config file for configuration stuff, creating it as necessary.
	*/
	if(!checkFilePath(confFileLoc, false) ){
		outputText('c', "Unable to open config file. Creating a new one...", verbose);
		generateMainConfig();
	}
	if(!checkFilePath(confFileLoc, false) ){
		outputText('c', "ERROR:: Unable to create or open config file.", true);
		return 1;
	}
	readMainConfig();
	
	if(listFlag){
		cout << "Files and folders:" << endl <<
			"\tConfig File:       " << confFileLoc << endl <<
			"\tLog File:          " << logFileLoc << endl <<
			"\tSync Directory:    " << syncFolderLoc << endl <<
			"\tStorage Directory: " << storFolderLoc << endl;
	}
	
	if(setToStartOnStart){
		setRunOnStart();
	}
	
	if(runFlag){
		runServer();
	}
	
	if(!helpFlag && !runFlag && !listFlag && !setToStartOnStart){
		outputText('c', "Not told to do anything. Use -h to see options.", verbose);
	}
	
	
	return 0;
}//main
