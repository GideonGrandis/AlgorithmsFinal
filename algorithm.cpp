#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <functional>
#include <queue>
#include <vector>
#include <unordered_map>
#include <set>
#include <memory>

#define LINE_SIZE 4096
#define LINE_NUMBER 500

using namespace std;



//Variables and structs
int studentPreferenceValue = 0;
int studentPreferenceBestCase = 0;
int totalStudentExpectations = 0; //Number of classes students want to take across all students.

int timeslots; //Number of timeslots
int classrooms; //Number of classrooms
int classes; //Number of classes
int professors; //Number of professors
int students; //Number of students

int roomConflictGiven; //Flag to check if conflicts for rooms were given.
int timeslotTimesGiven; //Flag to check if times for timeslots are provided by the data. 0 if false, 1 if true.
int** timeslotTimeStarts; //Keeps track of timeslot times for each timeslot.
		    //Array of arrays of size 7. Hours are converted to minutes
		    //e.g. Timeslot 3, Friday from 3:00 pm to 6:30 pm would be:
		    //timeslotTimeStarts[2][4] = 810, timeslotTimeEnds[2][4] = 1020.
int** timeslotTimeEnds;

int** timeslotAdjacencies; //Adjacency matrix for timeslots.

struct aRoom { //object representing a room
  int index; //Index of the room
  int capacity; //Capacity of the room
  char name[1024]; //Name of the room

  //Overloading greater than operator for heap comparison
  bool operator<(const aRoom& other) const {
    return capacity < other.capacity;
  }
};

struct aRoom** roomArray; //Array of room objects.
std::set<struct aRoom> roomList; //Ordered array of room objects, from smallest capacity to largest.

struct std::vector<std::set<struct aRoom>*> roomListSlots; //array size t of ordered array of room objects,
					     //from smallest capacity to largest.

struct aClass { //object representing a class
  int index; //Index of the class
  char* id; //Id of the class
  int section; //Section of the class
  int popularity; //Number of students who want to take the class
  int professor; //Professor who teaches the class
  int timeslot; //Timeslot the class is in. -1 when unassigned.
  int room; //Room the class is in. -1 when unassigned.
  int* students; //Array of students enrolled in the class. Used as a bitfield on indices. i.e
		 //1 if the student is enrolled in the class, 0 if the student is not enrolled
		 //in the class. Should run faster than an expandable array.
  int currentStudents; //Number of students currently enrolled in the class.
  int* conflictScores; //The conflict scores of the class.
  int inserted; //0 if the class has not been inserted into the schedule. 1 if it has been inserted.
  int* openRooms; //Indices of the rooms the classes can be held in. 0 if it can't be held in that room, 1 if it can.
  int openRoomCount = 0; //Number of open rooms. for e1, gives priority to this class if 1.
  char subject[5]; //Subject of the class. 4 characters.
  int sectionId; //Used for extension 5. Makes sure students don't take two sections of the same class.

 //Overloading less than operator for heap comparison
  bool operator<(const aClass& other) const {
    return popularity < other.popularity;
  }
};

int unassignedCount = 0; //Used for final fit calculation.

struct aClass** classArray; //Array of class objects
std::priority_queue<struct aClass> classHeap; //maxHeap of class objects.

struct aStudent { //object representing a student. Honestly we don't really need this for our
		  //algorithm, but since we're not focused on the runtime of the setup, and we
		  //might still want to change our algorithm, I figured I'd add it anyways.
  char* index; //Index/id of the student
  int prefClassCount; //Number of preferred classes.
  int* prefClass; //Preferred classes
};

int extensionOneFlag = 0;
float oneFlagPercentage = 0;
int oneFlagCount = 0;
int extensionTwoFlag = 0;
float twoFlagPercentage = 0;
int twoFlagLimit = 0;
int extensionThreeFlag = 0;
int extensionThreeDirection = 0;
int extensionFourFlag = 0;
int extensionFourValue = 0; //Value to increase by for classes with the same subjects under extension four.
int extensionFiveFlag = 0;
int extensionFiveLimit = 0;
int extensionFiveClassCount = 0;
int extensionFiveExtraCount = 0;
int ignoreOverlap = 0; //Ignores overlap between timeslots if provided in the data.


struct aStudent** studentArray; //Array of students.

std::unordered_map<char*, int> roomMap; //Maps room names with indices. Used to find constraints quickly.
std::unordered_map<int, int> professorMap; //Maps professor id's to indices. Used to deal with
					   // the format the real world data is in.
char** professorIds; //Array of professor id's. Used exclusively to print output correctly.

std::unordered_map<int, int> classMap; //Maps class id's to indices. Used to find preferences quickly.
int professorCountGlobal = 0;

int insertClass(struct aClass* tempClass, int timeslot, int begin) { //Inserts class into room in timeslot. -1 if error. 0 if insertion wasn't
						  //possible. 1 if class was succesfully inserted into timeslot.
						  //Begin flag is 0 to insert in largest room. 1 to try to insert into smallest room first.
  
	//printf("Trying to insert class %d in timeslot %d.\n", tempClass->index, timeslot);
    if (tempClass->inserted == 1) {
      //printf("                             Class %d already inserted.\n", tempClass->index);
      return 0;
    }

    if (tempClass->conflictScores[timeslot] == INT_MAX) {
      //printf("Conflict is max.\n");
      return 0;
    }
   
    if (roomListSlots[timeslot]->size() == 0) {
      //printf("No available rooms left in this timeslot.\n");
      tempClass->conflictScores[timeslot] = INT_MAX; //In case a timeslot doesn't have any rooms
      return 0;
    }

    //struct aRoom tempRoom = *roomListSlots[tempClass->index]->rbegin();

    struct std::set<struct aRoom> tempRoomList = *roomListSlots[timeslot];

    struct aRoom tempRoom;

    int endEarly = 0;

    if (begin == 0) { //Inital insert.
      for (std::set<struct aRoom>::reverse_iterator i = tempRoomList.rbegin(); i != tempRoomList.rend() && endEarly != 1; ++i) {
        tempRoom = *i;
        if (extensionOneFlag == 1) {
          if (tempClass->openRooms[tempRoom.index] == 1) {
	    endEarly = 1;
	  }
        } else {
  	  endEarly = 1;
        }
	if (timeslotTimesGiven == 1) {
          for (int j = 0; j < classes; j++) {
            if (classArray[j]->room == tempRoom.index && classArray[j]->inserted == 1) { //If this class is in the same room.
              if (timeslotAdjacencies[timeslot][classArray[j]->timeslot] == 1) { //If there is an overlap in their timeslots.
                endEarly = 0; //Then this is not a valid room.
              }
            }
          }
        }
      }
    } else { //Regular insert
      int extensionOneCheck = 0;
      for (std::set<struct aRoom>::iterator i = tempRoomList.begin(); i != tempRoomList.end() && endEarly != 1; ++i) {
        tempRoom = *i;
        if (tempClass->popularity <= tempRoom.capacity) { //Can fit in room
          endEarly = 1;
        }
	if (timeslotTimesGiven == 1) {
          for (int j = 0; j < classes; j++) {
            if (classArray[j]->room == tempRoom.index && classArray[j]->inserted == 1) { //If this class is in the same room.
              if (timeslotAdjacencies[timeslot][classArray[j]->timeslot] == 1) { //If there is an overlap in their timeslots.
                endEarly = 0; //Then this is not a valid room.
              }
	    }
	  }
	}
        if (extensionOneFlag == 1) {
          if (tempClass->openRooms[tempRoom.index] == 0) {
            endEarly = 0; //Check to see if class can actually go in this room.
          } else {
            extensionOneCheck = 1;
	  }
        }
      }
      if (extensionOneFlag == 1 && extensionOneCheck == 0) {
        tempClass->conflictScores[timeslot] = INT_MAX; //Class can't go into this timeslot. No rooms it can go in.
        return 0;
      }
    }

    roomListSlots[timeslot]->erase(tempRoom);

    tempClass->inserted = 1;
    tempClass->room = tempRoom.index;
    tempClass->timeslot = timeslot;

    for (int i = 0; i < classes; i++) {
      if (classArray[i]->professor == tempClass->professor) {
        classArray[i]->conflictScores[timeslot] = INT_MAX;
	if (timeslotTimesGiven == 1) {
          for (int j = 0; j < timeslots; j++) {
            if (timeslotAdjacencies[j][timeslot] == 1) {
              classArray[i]->conflictScores[j] = INT_MAX;
	    }
          }
	}
      }
      if (extensionOneFlag == 1) {
        if (classArray[i]->openRooms[tempRoom.index] == 1) { //This is an available room for course i as well.
          classArray[i]->openRooms[tempRoom.index] == 0;
	  classArray[i]->openRoomCount--;
	  if (classArray[i]->openRoomCount == 1) {
            for (int j = 0; j < timeslots; j++) {
              if (classArray[i]->conflictScores[j] != INT_MAX) {
                classArray[i]->conflictScores[j] = -1; //Give priority if its the only available room for the class.
	      }
	    }
	  } else if (classArray[i]->openRoomCount == 0) {
            for (int j = 0; j < timeslots; j++) {
              classArray[i]->conflictScores[j] = INT_MAX; //In case its really impossible to insert them all.
	    }
	  }
	}
      }
    } 

    return 1;
}

int main(int argc, char *argv[]) {

  struct timeval startA, stopA; //Clock
  double secsA = 0;
  gettimeofday(&startA, NULL);

  struct timeval startC, stopC; //Clock
  double secsC = 0;
  gettimeofday(&startC, NULL);

  //Setup
  if (argc < 3) {
    printf("Usage: ./algorithm [Constraints].txt [Preferences].txt\n");
    return 0;
  }

  if (argc > 3) {
    for (int i = 3; i < argc; i++) {
      if (strcmp(argv[i], "-e1") == 0) {
        extensionOneFlag = 1;
	i++;
        if (i != argc) {
          oneFlagPercentage = (float) atoi(argv[i]) * 0.01;
        } else {
          printf("Usage for extension one: -e1 (percentage of classes with room restrictions)\n");
          return 0;
        }
      } else if (strcmp(argv[i], "-e2") == 0) {
        extensionTwoFlag = 1;
	i++;
	if (i != argc) {
          twoFlagPercentage = (float) atoi(argv[i]) * 0.01;
	} else {
          printf("Usage for extension two: -e2 (percentage of classes with limit) (limit in minutes)\n");
          return 0;
        }
	i++;
	if (i != argc) {
          twoFlagLimit = atoi(argv[i]);
	} else {
          printf("Usage for extension two: -e2 (percentage of classes with limit) (limit in minutes)\n");
	  return 0;
	}
      } else if (strcmp(argv[i], "-e3") == 0) {
        extensionThreeFlag = 1;
	i++;
        if (i != argc) {
          if (atoi(argv[i]) == 1) {
            extensionThreeDirection = 1;
	  }
        } else {
          printf("Usage for extension three: -e3 (flag, 0 for MWF to MW, 1 for MW to MWF)\n");
          return 0;
        }
      } else if (strcmp(argv[i], "-e4") == 0) {
        extensionFourFlag = 1;
	i++;
        if (i != argc) {
          extensionFourValue = atoi(argv[i]);
        } else {
          printf("Usage for extension four: -e4 (value to increase conflict scores of classes with the same subject by)\n");
          return 0;
        }
      } else if (strcmp(argv[i], "-e5") == 0) {
        extensionFiveFlag = 1;
	i++;
        if (i != argc) {
          extensionFiveLimit = atoi(argv[i]);
        } else {
          printf("Usage for extension five: -e5 (max number of student which can want to take a class before splitting into sections.)\n");
          return 0;
        }
      } else if (strcmp(argv[i], "-i") == 0) {
	ignoreOverlap = 1;
      } else {
        printf("Tag not recognized.\n Tags are:\n   -e1 (percentage of classes with room restrictions) for extension 1.\n   -e2 (percentage of classes with limit) (limit in minutes) for extension 2.\n   -e3 (flag, 0 for MWF to MW, 1 for MW to MWF) for extension 3.\n   -e4 (value to increase conflict scores of classes with the same subject by) for extension 4.\n   -e5 (max number of student which can want to take a class before splitting into sections) for extension 5.\n -i to ignore overlap.\n");
        return 0;
      }
    }
  }

  if (extensionTwoFlag == 1) {
    printf("Extension two values: percentage: %f. limit %d.\n", twoFlagPercentage, twoFlagLimit);
  }

  FILE *fptrConstraints; //I'm using c style file read/write since I'm more used to it, but I
	      //can swap it out for cpp style if we'd prefer that.
  FILE *fptrPreferences;

  //Opening files & setting up parsing.
  fptrConstraints = fopen(argv[1], "r");
  if (fptrConstraints == NULL) {
    printf("Error reading \"%s\".\n", argv[1]);
    return 0;
  }
  fptrPreferences = fopen(argv[2], "r");
  if (fptrPreferences == NULL) {
    printf("Error reading \"%s\".\n", argv[2]);
    return 0;
  }

  char line[LINE_SIZE * LINE_NUMBER]; //File buffer
  line[0] = '\0';
  char tempLine[LINE_SIZE]; //Line buffer
  tempLine[0] = '\0';

  //Figured it was easier to just put the entire contents of the file into a seperate buffer
  //instead of reading it line by line.
  //There's probably a more elegant way to do this with c++ libraries, but I don't think it
  //greatly affects the time.
  while (fgets(tempLine, LINE_SIZE, fptrConstraints)) {
    tempLine[LINE_SIZE - 1] = '\0';
    strncat(line, tempLine, LINE_SIZE);
  }

  char *saveptrMain; //Using multiple instances of strtok here, so we have to use strtok_r.
  char *saveptrTime;

  char delimiters[3];
  delimiters[0] = 9;
  delimiters[1] = 10;
  delimiters[2] = '\0';

  //Parsing constraints data
  //This whole section is sort of a mess to read, but there's really no other way
  //to read the data from such a specific format.
  char *token = strtok_r(line, delimiters, &saveptrMain); //"Class times"
  token = strtok_r(NULL, delimiters, &saveptrMain); //timeslots
  timeslots = atoi(token);
  timeslotTimeStarts = (int**) malloc(timeslots * sizeof(int*));
  timeslotTimeEnds = (int**) malloc(timeslots * sizeof(int*));
  for (int i = 0; i < timeslots; i++) {
    timeslotTimeStarts[i] = (int*) malloc(7 * sizeof(int));
    timeslotTimeEnds[i] = (int*) malloc(7 * sizeof(int));
    for (int j = 0; j < 7; j++) {
      timeslotTimeStarts[i][j] = -1; //Should initalize, even if it goes unused.
      timeslotTimeEnds[i][j] = -1;
    }
  }
  token = strtok_r(NULL, delimiters, &saveptrMain); //"Rooms"
  if (strcmp(token, "Rooms") != 0) { //If not rooms, then timeslot index 1.
    for (int i = 0; i < timeslots; i++) {
      if (ignoreOverlap == 0) {
        timeslotTimesGiven = 1;
      }
      int startTime = 0;
      int endTime = 0;
      token = strtok_r(NULL, delimiters, &saveptrMain); //TIME BLOCK
      //printf("Debug: Time block: %s.\n", token);
      char spaceDelim[3];
      spaceDelim[0] = ' '; //Paring parsing parsing...
      spaceDelim[1] = ':';
      spaceDelim[2] = '\0';
      char tokenCopy[1024];
      for (int j = 0; j < 1024; j++) {
        tokenCopy[j] = '\0';
      }
      int twelveCheck = 0;
      strncpy(tokenCopy, token, 1024);
      char* spaceToken = strtok_r(tokenCopy, spaceDelim, &saveptrTime); //* (hour)
      if (12 == atoi(spaceToken)) {
        twelveCheck = 1;
      }
      startTime = 60 * atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //* (minute)
      startTime = startTime + atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //AM/PM
      if (strcmp(spaceToken, "PM") == 0 && twelveCheck == 0) {
        startTime = startTime + (60 * 12);
      } else if (strcmp(spaceToken, "AM") == 0 && twelveCheck == 1) {
        startTime = startTime + (60 * 12);
      } else if (strcmp(spaceToken, "AM") != 0 && strcmp(spaceToken, "PM") != 0) {
        printf("DEBUG: Error parsing timeslot data. Expected 'AM', but got '%s'.\n", spaceToken);
      }
      twelveCheck = 0;
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); // (hour)
      if (12 == atoi(spaceToken)) {
        twelveCheck = 1;
      }
      endTime = 60 * atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //:
      endTime = endTime + atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //AM/PM
      if (strcmp(spaceToken, "PM") == 0 && twelveCheck == 0) {
        endTime = endTime + (60 * 12);
      } else if (strcmp(spaceToken, "AM") == 0 && twelveCheck == 1) {
        startTime = startTime + (60 * 12);
      } else if (strcmp(spaceToken, "AM") != 0 && strcmp(spaceToken, "PM") != 0) {
        printf("DEBUG: Error parsing timeslot data. Expected 'AM', but got '%s'.\n", spaceToken);
      }
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //Null/day of the week.
      int tempBitFlag[7];
      for (int j = 0; j < 7; j++) {
        tempBitFlag[j] = 0;
      }
      int monCheck = 0;
      int wedCheck = 0;
      int friCheck = 0; //Flags for extension 3
      for (int i = 0; i < 7; i++) {
        if (spaceToken[i] != '\0') {
          char DOTW = spaceToken[i];
          switch (DOTW) {
            case 'M':
              tempBitFlag[0] = 1;
	      monCheck = 1;
              break;
	    case 'T':
	      if (i != 6) {
                if (spaceToken[i + 1] != 'H') {
                  tempBitFlag[1] = 1;
		}
	      } else {
                tempBitFlag[1] = 1;
	      }
	      monCheck = 0;
              wedCheck = 0;
              friCheck = 0;
              break;
	    case 'W':
              tempBitFlag[2] = 1;
	      wedCheck = 1;
              break;
	    case 'H':
              tempBitFlag[3] = 1;
	      monCheck = 0;
              wedCheck = 0;
              friCheck = 0;
              break;
	    case 'F':
              tempBitFlag[4] = 1;
	      friCheck = 1;
              break;
	    case 'S':
              tempBitFlag[5] = 1;
	      monCheck = 0;
              wedCheck = 0;
              friCheck = 0;
              break;
	    case 'U':
              tempBitFlag[6] = 1;
	      monCheck = 0;
              wedCheck = 0;
              friCheck = 0;
              break;
	    case '-': //Used exclusively for M-F I believe. Very annoying.
	      tempBitFlag[1] = 1; tempBitFlag[2] = 1; tempBitFlag[3] = 1;
	      monCheck = 0;
              wedCheck = 0;
              friCheck = 0;
          }
	}
      }

      if (extensionThreeFlag == 1 && extensionThreeDirection == 0 && monCheck == 1 && wedCheck == 1 && friCheck == 1) {
        tempBitFlag[4] = 0;
	endTime = endTime + 30; //Remove friday class, and add 30 minutes.
      } else if (extensionThreeFlag == 1 && extensionThreeDirection == 1 && monCheck == 1 && wedCheck == 1 && friCheck == 0) {
	tempBitFlag[4] = 1;
	endTime = endTime - 30; //Add friday and remove 3- minutes.
      }
      for (int j = 0; j < 7; j++) {
        if (tempBitFlag[j] == 1) {
          timeslotTimeStarts[i][j] = startTime;
	  timeslotTimeEnds[i][j] = endTime;
	}
      }
      token = strtok_r(NULL, delimiters, &saveptrMain); //Next timeslot index, or 'Rooms.'
    }
  }

  timeslotAdjacencies = (int**) malloc(sizeof(int*) * timeslots);
  for (int i = 0; i < timeslots; i++) {
    timeslotAdjacencies[i] = (int*) malloc(sizeof(int) * timeslots);
    for (int j = 0; j < timeslots; j++) {
      timeslotAdjacencies[i][j] = 0;
    }
  }
  

  if (timeslotTimesGiven == 1) {
    for (int i = 0; i < timeslots; i++) {
      for (int j = 0; j < timeslots; j++) {
        if (i != j) { //Don't have timeslot be adjacent to themselves.
          for (int k = 0; k < 7; k++) {
            if (timeslotTimeStarts[i][k] != -1 && timeslotTimeStarts[j][k] != -1) { //If both have a time on this day
              if (timeslotTimeStarts[i][k] < timeslotTimeStarts[j][k]) { //if 1 starts before 2
                if (timeslotTimeEnds[i][k] > timeslotTimeStarts[j][k]) { //Time conflict
                  timeslotAdjacencies[i][j] = 1;
	          timeslotAdjacencies[j][i] = 1;
                }
              } else if (timeslotTimeStarts[i][k] >= timeslotTimeStarts[j][k]) { //if 2 starts before 1
                if (timeslotTimeEnds[j][k] > timeslotTimeStarts[i][k]) { //Time conflict
                  timeslotAdjacencies[i][j] = 1;
                  timeslotAdjacencies[j][i] = 1;
                }
              } 
            }
          }
	}
      }
    }
  }

  token = strtok_r(NULL, delimiters, &saveptrMain); //Rooms
  classrooms = atoi(token);
  roomArray = (struct aRoom**) malloc(classrooms * sizeof(struct aRoom*)); //Create a new
        								   //array of rooms
  for (int i = 0; i < classrooms; i++) { //For each room
    roomArray[i] = NULL;
    struct aRoom *roomPointer = (struct aRoom*) malloc(sizeof(struct aRoom)); //Create a new room
    roomArray[i] = roomPointer; //Add to the room array
    roomArray[i]->index = i; //Index
    token = strtok_r(NULL, delimiters, &saveptrMain); //name
    for (int j = 0; j < 1024; j++) {
      roomArray[i]->name[j] = '\0'; //Initalize name.
    }
    strncpy(roomArray[i]->name, token, 1024);
    roomMap.insert({roomArray[i]->name, i});
    token = strtok_r(NULL, delimiters, &saveptrMain); //capacity
    roomArray[i]->capacity = atoi(token); //Assign the read capacity
  }
  token = strtok_r(NULL, delimiters, &saveptrMain); //"Classes" 
  token = strtok_r(NULL,  delimiters, &saveptrMain); //classes
  classes = atoi(token);
  int originalClasses = classes;
  if (extensionFiveFlag == 1) {
    classes = classes * 10; //No more than 10 sections a class.
    extensionFiveClassCount = originalClasses;
  }
  classArray = (struct aClass**) malloc(classes * sizeof(struct aClass*)); //Create a new
                                                                           //array of classes
  token = strtok_r(NULL, delimiters, &saveptrMain); //"Teachers"
  token = strtok_r(NULL, delimiters, &saveptrMain); //professors
  professors = atoi(token);

  char tenOnlyDelim[2];
  tenOnlyDelim[0] = 10;
  tenOnlyDelim[1] = '\0';
  char* tenToken;
  char* tenptr;

  professorIds = (char**) malloc(sizeof(char*) * classes);
  oneFlagCount = (int) (oneFlagPercentage * classes);
  if (oneFlagCount > classes) {
    oneFlagCount = classes;
  } else if (oneFlagCount < 0) {
    oneFlagCount = 0;
  }

  for (int i = 0; i < classes; i++) { //For each class
    struct aClass *classPointer = (struct aClass*) malloc(sizeof(struct aClass)); //Create a new class
    classArray[i] = classPointer; //Add to the class array
    classArray[i]->index = i;
    classArray[i]->currentStudents = 0;
    if (i >= originalClasses) {
      classArray[i]->section = -2;
      classArray[i]->id = (char*) malloc(sizeof(char) * 10);
      for (int j = 0; j < 10; j++) {
        classArray[i]->id[j] = '\0';
      }
      classArray[i]->professor = -1;
      for (int j = 0; j < 4; j++) {
        classArray[i]->subject[j] = '\0';
      }
      professorIds[i] = (char*) malloc(sizeof(char) * 10);
      for (int j = 0; j < 10; j++) {
        professorIds[i][j] = '\0';
      }
      classArray[i]->openRooms = (int*) malloc(sizeof(int) * classrooms);
      for (int j = 0; j < classrooms; j++) {
        classArray[i]->openRooms[j] = 0;
      }
      classArray[i]->openRoomCount = -1;
      classArray[i]->room = -1;
      classArray[i]->timeslot = -1;
      classArray[i]->popularity = 0;
      classArray[i]->inserted = 0;
      classArray[i]->conflictScores = (int*) malloc(timeslots * sizeof(int));
      for (int j = 0; j < timeslots; j++) {
        classArray[i]->conflictScores[j] = 0;
      }
      classArray[i]->students = NULL;
    } else {
      classArray[i]->section = -1;
      classArray[i]->sectionId = i;
      token = strtok_r(NULL, tenOnlyDelim, &saveptrMain);
      tenToken = strtok_r(token, delimiters, &tenptr); //id
      classArray[i]->id = (char*) malloc(sizeof(char) * 10);
      for (int j = 0; j < 10; j++) {
        classArray[i]->id[j] = '\0';
      }
      strncpy(classArray[i]->id, tenToken, 10);
      tenToken = strtok_r(NULL, delimiters, &tenptr); //professor id
      professorIds[i] = (char*) malloc(sizeof(char) * 10);
      for (int j = 0; j < 10; j++) {
        professorIds[i][j] = '\0';
      }
      if (professorMap.find(atoi(tenToken)) != professorMap.end()) {
        classArray[i]->professor = professorMap.at(atoi(tenToken));
      } else {
        professorMap[atoi(tenToken)] = professorCountGlobal;
        classArray[i]->professor = professorMap.at(atoi(tenToken));
        professorCountGlobal++;
      }
      strncpy(professorIds[i], tenToken, 10);
      professorIds[i][9] = '\0';
      classMap.insert({atoi(token), i});
      tenToken = strtok_r(NULL, delimiters, &tenptr); //subject
      for (int j = 0; j < 5; j++) {
        classArray[i]->subject[j] = '\0';
      }
      if (tenToken != NULL) {
        strncpy(classArray[i]->subject, tenToken, 4);
      }
      classArray[i]->subject[4] = '\0';
      tenToken = strtok_r(NULL, delimiters, &tenptr); //first room or NULL
      classArray[i]->openRooms = (int*) malloc(sizeof(int) * classrooms);
      if (tenToken == NULL || ignoreOverlap == 1) { //roomdata not given
        roomConflictGiven = 0;
        for (int j = 0; j < classrooms; j++) {
          classArray[i]->openRooms[j] = 1;
        }
      } else {
        roomConflictGiven = 1;
        for (int j = 0; j < classrooms; j++) {
          if (i > oneFlagCount) {
            classArray[i]->openRooms[j] = 0;
	  } else {
            classArray[i]->openRooms[j] = 1;
	  }
        }
      }
      classArray[i]->openRoomCount = 0;
      while (tenToken != NULL) {
        for (const auto &p : roomMap) {
          if (strcmp(p.first, tenToken) == 0) {
            int temp = 0;
            for (int j = 0; j < classrooms; j++) {
              if (strcmp(roomArray[j]->name, p.first) == 0) {
                if (classArray[i]->openRooms[j] == 1) {
                  temp = 1;
	        }
	      }
	    }
	    if (temp == 0) {
              classArray[i]->openRooms[p.second] = 1;
	      classArray[i]->openRoomCount++;
	    }
          }
        }
        tenToken = strtok_r(NULL, delimiters, &tenptr);
      }
      classArray[i]->room = -1;
      classArray[i]->timeslot = -1;
      classArray[i]->popularity = 0;
      classArray[i]->inserted = 0;
      classArray[i]->conflictScores = (int*) malloc(timeslots * sizeof(int));
      for (int j = 0; j < timeslots; j++) {
        classArray[i]->conflictScores[j] = 0;
      }
      classArray[i]->students = NULL;
    }
  }

  char* tempptr;

  //Setting up for parsing again.
  strcpy(line, "\0");
  strcpy(tempLine, "\0");
  char tempLineCopy[LINE_SIZE * LINE_NUMBER];
  for (int i = 0; i < LINE_SIZE; i++) {
    tempLineCopy[i] = '\0';
  }
  char tempDelim[2];
  tempDelim[0] = 10;
  tempDelim[1] = '\0';
  while (fgets(tempLine, LINE_SIZE, fptrPreferences)) {
    strncat(line, tempLine, LINE_SIZE - 1);
  }
  line[(LINE_SIZE * LINE_NUMBER) - 1] = '\0';
  strncat(tempLineCopy, line, LINE_SIZE * LINE_NUMBER - 1);

  int countCount = 0;
  char* tempToken = strtok_r(tempLineCopy, tempDelim, &tempptr);
  while (tempToken != NULL) {
    countCount++;
    tempToken = strtok_r(NULL, tempDelim, &tempptr);
  }

  //Parsing preferences data
  char* saveptrNew;
  char* saveptrStudent;
  char* saveptrStudentSecond;

  char spaceDelim[2];
  spaceDelim[0] = ' '; spaceDelim[1] = '\0';

  token = strtok_r(line, delimiters, &saveptrNew); //"Students"
  token = strtok_r(NULL, delimiters, &saveptrNew); //students
  students = atoi(token);
  studentArray = (struct aStudent**) malloc(students * sizeof(struct aStudent*)); //Create a new
                                                                           //array of students
  
  for (int i = 0; i < students; i++) {
    struct aStudent *studentPointer = (struct aStudent*) malloc(sizeof(struct aStudent)); //Create a new student
    studentArray[i] = studentPointer; //Add to the student array

    token = strtok_r(NULL, delimiters, &saveptrNew); //index
    studentArray[i]->index = (char*) malloc(sizeof(char) * 10);
    for (int j = 0; j < 10; j++) {
      studentArray[i]->index[j] = '\0';
    }
    strncpy(studentArray[i]->index, token, 10);
    token = strtok_r(NULL, delimiters, &saveptrNew); //classes

    char tokenCopy[2048];
    char tokenCopyTwo[2048];
    for (int i = 0; i < 2048; i++) {
      tokenCopy[i] = '\0';
      tokenCopyTwo[i] = '\0';
    }
    strncpy(tokenCopy, token, 2048);
    strncpy(tokenCopyTwo, token, 2048);
    char* spaceToken = strtok_r(tokenCopy, spaceDelim, &saveptrStudent);
    int classCount = 0;
    while (spaceToken != NULL) {
      classCount++;
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrStudent);
    }
    totalStudentExpectations = totalStudentExpectations + classCount;
    if (extensionFiveFlag == 1) {
      classCount = (classCount * 10) + 1;
    }
    studentArray[i]->prefClassCount = classCount;

    studentArray[i]->prefClass = (int*) malloc(classCount * sizeof(int));
    for (int j = 0; j < classCount; j++) {
      studentArray[i]->prefClass[j] = -1;
    }
    classCount = 0;
    char* spaceTokenTwo = strtok_r(tokenCopyTwo, spaceDelim, &saveptrStudentSecond);
    while (spaceTokenTwo != NULL) {
      studentArray[i]->prefClass[classCount] = classMap.at(atoi(spaceTokenTwo));
      classCount++;
      spaceTokenTwo = strtok_r(NULL, spaceDelim, &saveptrStudentSecond);
    }
  }

  //Adding student arrays to each class.
  for (int i = 0; i < classes; i++) {
    classArray[i]->students = (int*) malloc(students * sizeof(int)); //Create all of the student arrays
    for (int j = 0; j < students; j++) {
      classArray[i]->students[j] = 0;
    }
  }
 
  gettimeofday(&stopA, NULL); //Clock
  secsA = (double)(stopA.tv_usec - startA.tv_usec) / 1000000 + (double)(stopA.tv_sec - startA.tv_sec);
  printf("Setup took %f seconds to run.\n", secsA);

  printf("Timeslots: %d, Classrooms: %d, Classes: %d, Professors: %d, Students: %d.\n", timeslots,
		 classrooms, classes, professors, students);
  
  struct timeval startB, stopB; //Clock
  double secsB = 0;

  gettimeofday(&startB, NULL);

  if (roomConflictGiven == 0 && extensionOneFlag == 1) {
    extensionOneFlag = 0;
    printf("WARNING: Data was not provided for room conflicts. Extension one will not apply to the output.\n");
  }

  //THE ALGORITHM:

  if (extensionFiveFlag == 1) {
    for (int i = 0; i < students; i++) {
      for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
        if (studentArray[i]->prefClass[j] != -1) {
          classArray[studentArray[i]->prefClass[j]]->popularity++; //increase popularity of all j classes.
        }
      }
    }
    for (int i = 0; i < originalClasses; i++) {
      int sectionCount = classArray[i]->popularity/extensionFiveLimit;
      if (sectionCount < 0) {
        sectionCount = 0;
      }
      if (sectionCount > 9) {
        sectionCount = 9;
      }
      extensionFiveClassCount;
      for (int j = 0; j < sectionCount; j++) {
        extensionFiveExtraCount++;
        classArray[extensionFiveClassCount]->inserted = 0;
        classArray[extensionFiveClassCount]->section = j;
	classArray[extensionFiveClassCount]->popularity = classArray[i]->popularity;
	classArray[extensionFiveClassCount]->professor = classArray[i]->professor;
	for (int k = 0; k < classrooms; k++) {
          classArray[extensionFiveClassCount]->openRooms[k] = classArray[i]->openRooms[k];
	}
	strncpy(professorIds[extensionFiveClassCount], professorIds[i], 10);
        classArray[extensionFiveClassCount]->openRoomCount = classArray[i]->openRoomCount;
	strcpy(classArray[extensionFiveClassCount]->subject, classArray[i]->subject);
        strcpy(classArray[extensionFiveClassCount]->id, classArray[i]->id);
        strncat(classArray[extensionFiveClassCount]->id, "-", 2);
	classArray[extensionFiveClassCount]->sectionId = classArray[i]->sectionId;
	char temp[2];
	temp[0] = j + '0';
	temp[1] = '\0';
	strncat(classArray[extensionFiveClassCount]->id, temp, 2);
        extensionFiveClassCount++;
      }
    }
    for (int i = 0; i < students; i++) {
      int tempIterator = (studentArray[i]->prefClassCount - 1)/10;
      int iterator = tempIterator;
      for (int j = originalClasses; j < classes; j++) {
        if (classArray[j]->section > -1) { //For every section.
          for (int k = 0; k < tempIterator; k++) { //Check too see if it matches anything for the student.
            if (studentArray[i]->prefClass[k] != -1) {
              if (classArray[studentArray[i]->prefClass[k]]->sectionId == classArray[j]->sectionId) { //Room is a secition of a preferred class.
                studentArray[i]->prefClass[iterator] = j; //So add the section.
                iterator++;
		if (iterator > studentArray[i]->prefClassCount) {
                  printf("DEBUG ERROR:  EXEEDING AVAILABLE SPACE.\n");
		}
	      }
	    }
	  }
	}
      }
      studentArray[i]->prefClassCount = iterator;
    }
    originalClasses = classes;
    classes = extensionFiveClassCount;
  }
  
  for (int i = 0; i < students; i++) { //Add students to classes' bitfields for later scheduling.
    for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
      if (studentArray[i]->prefClass[j] != -1) {
        classArray[studentArray[i]->prefClass[j]]->students[i] = 1;
      }
    }
  }

  //Initalizing the complete graph
  int** G = (int**)malloc(classes * sizeof(int*)); //Graph G
  for (int i = 0; i < classes; i++) {
    G[i] = (int*)malloc(classes * sizeof(int));
    for (int j = 0; j < classes; j++) {
      if (i == j || (classArray[i]->professor == classArray[j]->professor && classArray[j]->professor != -1)) { //Professor check
									    //Since we already
									    //have to do classes^2
									    //anyways for
									    //initalization.
        G[i][j] = INT_MAX;
      } else {
        G[i][j] = 0;
      }
    }
  }





 // printf("prof done.\n");

  //Building sorted list of room sizes.
  for (int i = 0; i < classrooms; i++) {
    roomList.insert(*roomArray[i]);
  }
  for (std::set<struct aRoom>::iterator i = roomList.begin(); i != roomList.end(); i++) {
    struct aRoom tempRoom = *i;
  }

  //Building sorted lists of room sizes.

  //Allocating space for ordered arrays

  for (int i = 0; i < timeslots; i++) {
    std::set<struct aRoom>* tempSet = new set<struct aRoom>();
    for (int j = 0; j < classrooms; j++) {
      tempSet->insert(*roomArray[j]);
    }
    roomListSlots.push_back(tempSet);
  }

  for (std::set<struct aRoom>* i : roomListSlots) {
    std::set<struct aRoom> tempSet = *i;
    for (std::set<struct aRoom>::iterator j = tempSet.begin(); j != tempSet.end(); j++) {
      struct aRoom tempRoom = *j;
    }
  }

  //1. We begin by making a weighted complete graph G with vertex set C, where each edge
  //has weight equal to the number of people who want to take both of its endpoints. We
  //then set the weight of classes with the same professor to infinity. We also store the total
  //number of people who want to take each class.

  //Building G
  for (int i = 0; i < students; i++) {
    for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
      for (int q = 0; q < studentArray[i]->prefClassCount; q++) {
        if (j != q) {
          if (G[(studentArray[i]->prefClass[j])][(studentArray[i]->prefClass[q])] > -1) {
            G[(studentArray[i]->prefClass[j])][(studentArray[i]->prefClass[q])]++;
	  } else {
            G[(studentArray[i]->prefClass[j])][(studentArray[i]->prefClass[q])] = INT_MAX;
	  }
	}
      }
      if (extensionFiveFlag == 0) { //Already done if 1.
        classArray[studentArray[i]->prefClass[j]]->popularity++; //increase popularity of all j classes.
      }
    }
  }
  if (extensionFourFlag == 1) {
    for (int i = 0; i < classes; i++) {
      for (int j = 0; j < classes; j++) {
        if (strcmp(classArray[i]->subject, classArray[j]->subject) == 0) {
          if (G[i][j] != INT_MAX && G[i][j] + extensionFourValue > 0) {
            G[i][j] = G[i][j] + extensionFourValue;
	  }
	} else {
	}
      }
    }
  }

  //Building maxHeap of popularity for classes
  for (int i = 0; i < classes; i++) {
    classHeap.push(*classArray[i]);
  }

  //2. We then initialize a length t conflict score array [0, …, 0] for each class.
    //Already did this bit in initalization of classes objects since its faster. Technically
    //maybe cheats the time our algorithm runs a little bit, but frankly it all seems a little
    //subjective what's neccescary initalization and initalization specific to our algorithm anyways.
  
  int classesAdded = 0;

  if (extensionOneFlag == 1) { //Go through every room in every timeslot. If no availability, then timeslot = 0;
    for (int i = 0; i < classes; i++) {
      for (int j = 0; j < timeslots; j++) {
        int isEmpty = 0;

        std::set<struct aRoom> tempSet = *roomListSlots[j];
        for (std::set<struct aRoom>::iterator k = tempSet.begin(); k != tempSet.end(); k++) {
          struct aRoom tempRoom = *k;
          if (classArray[i]->openRooms[tempRoom.index] == 1) {
            isEmpty = 1;
	  
	  }
	}
	if (isEmpty == 0) {
          classArray[i]->conflictScores[j] = INT_MAX;
	}
      }
    }
  }
  if (extensionTwoFlag == 1) {
    if (timeslotTimesGiven == 0) {
      printf("Timeslot data not provided. Extension two cannot apply to this output.\n");
      extensionTwoFlag = 0;
    } else {
      int extensionTwoCount = (int) (twoFlagPercentage * classes);
      if (extensionTwoCount > classes - 1) {
        extensionTwoCount = classes - 1;
      } else if (extensionTwoCount < 0) {
        extensionTwoCount = 0;
      }
      while (extensionTwoCount != -1) {
        for (int i = 0; i < timeslots; i++) {
          int totalTime = 0;
          for (int j = 0; j < 7; j++) {
            if (timeslotTimeEnds[i][j] != -1) {
              totalTime = totalTime + (timeslotTimeEnds[i][j] - timeslotTimeStarts[i][j]);
	    }
          }
          if (totalTime > twoFlagLimit) {
            classArray[extensionTwoCount]->conflictScores[i] = INT_MAX;
	  } else {
	  }
        }
	extensionTwoCount--;
      }
    }
  }

  //3. We start by assigning the t largest classes C1, …, Ct to the largest room in the first time slot.
  for (int i = 0; i < timeslots; i++) {
    struct aClass tempTempClass;
    struct aClass* tempClass;

    tempTempClass = classHeap.top();
    tempClass = classArray[tempTempClass.index];
    if (extensionOneFlag == 1) {
      for (int j = 0; j < classes; j++) {
        if (classArray[j]->openRoomCount == 1 && classArray[j]->inserted == 0) { //Only one room left this class can go in. Give priority.
          tempClass = classArray[j];
	}
      }
    }

    int infiniteCheck = classes;
    while (insertClass(tempClass, i, 0) == 0 && infiniteCheck != 0) {
      classHeap.pop();
      tempTempClass = classHeap.top();
      tempClass = classArray[tempTempClass.index];
      infiniteCheck--;
    }
    classHeap.pop();

    //Step 4
    if (infiniteCheck != 0) {
      for (int j = 0;  j < classes; j++) {
        if (classArray[j]->conflictScores[i] < -1) {
          classArray[j]->conflictScores[i] = INT_MAX;
	}
	if (G[tempClass->index][j] < -1) {
          G[tempClass->index][j] = INT_MAX;
	}
        if (classArray[j]->conflictScores[i] + G[tempClass->index][j] > -1) {
          classArray[j]->conflictScores[i] = classArray[j]->conflictScores[i] + G[tempClass->index][j];
	} else {
          classArray[j]->conflictScores[i] = INT_MAX;
	}
        if (roomConflictGiven == 1) {
          for (int k = 0; k < classes; k++) {
            if (classArray[k]->inserted == 1) {
              if (timeslotAdjacencies[i][classArray[k]->timeslot] == 1) { //If this class overlaps with the one that was just inserted.
                if (G[k][j] < -1) {
                  G[k][j] = INT_MAX;
		}
		if (classArray[j]->conflictScores[i] + G[k][j] > -1) {
                  classArray[j]->conflictScores[i] = classArray[j]->conflictScores[i] + G[k][j]; //Then add the conflict between that class the original iterated one as well.
		} else {
                  classArray[j]->conflictScores[i] = INT_MAX;
		}
	      }
	    }
	  }
	}
      }
    }
  }

  //8. Repeat steps 5-7 until we’ve assigned every class to a room and time slot

  int timeslotsFilled = 0;
  int roomsFilled = 0;
  
  int iteration = 0;

  while (timeslotsFilled != timeslots && classesAdded != classes) {
    iteration++;
    int smallest = INT_MAX;
    int smallestClass = -1;
    int smallestTimeslot = -1;
    for (int i = 0; i < classes; i++) {
      if (classArray[i]->inserted == 0) {
        for (int j = 0; j < timeslots; j++) {
          if (classArray[i]->conflictScores[j] < smallest) {
            smallest = classArray[i]->conflictScores[j];
            smallestClass = i;
    	    smallestTimeslot = j;
          }
        }
      }
    }
    if (smallestClass == -1) {
      break;
    }

    //7. Increase the j-th conflict score of each class C by the weight of the edge between C and
    //C’, unless C’ filled the last room, in which case set it to infinity.
    if (insertClass(classArray[smallestClass], smallestTimeslot, 1) == 1) {
      classesAdded++;
      for (int i = 0; i < classes; i++) {
        if ((roomListSlots[smallestTimeslot]->size() != 0) && (classArray[i]->conflictScores[smallestTimeslot] + G[i][smallestClass] > -1)) {
          classArray[i]->conflictScores[smallestTimeslot] = classArray[i]->conflictScores[smallestTimeslot]
	      + G[i][smallestClass];
        } else {
          classArray[i]->conflictScores[smallestTimeslot] = INT_MAX; //Set to infinity
          if (roomListSlots[smallestTimeslot]->size() == 0) {
            timeslotsFilled++;
  	  }
        }

        //For all classes. If a class overlaps with the class that was just inserted, then add that conflict with the oriignal iterating class as well.
        if (roomConflictGiven == 1) {
          for (int j = 0; j < classes; j++) {
            if (classArray[j]->inserted == 1) {
              if (timeslotAdjacencies[smallestTimeslot][classArray[j]->timeslot] == 1) { //If this class overlaps with the one that was just inserted.
                if (G[j][i] < -1) {
                  G[j][i] = INT_MAX;
                }
                if (classArray[i]->conflictScores[smallestTimeslot] + G[j][i] > -1) {
                  classArray[i]->conflictScores[smallestTimeslot] = classArray[i]->conflictScores[smallestTimeslot] + G[j][i]; //Then add the conflict between that class the original iterated one as well.
                } else {
                  classArray[i]->conflictScores[smallestTimeslot] = INT_MAX;
                }
              }
            }
          }
        }
        if (classArray[i]->professor == classArray[smallestClass]->professor) {
          classArray[i]->conflictScores[smallestTimeslot] = INT_MAX;
        }
      }
    }
  }

  //Assign students.
  
  for (int g = 0; g < classes; g++) {
    if (classArray[g]->inserted == 1) {
      int smallestClass = g;
      int smallestTimeslot = (classArray[g]->timeslot);
      for (int i = 0; i < students; i++) { //Make sure students can't take two classes
                                           //during the same timeslot.
        if (classArray[smallestClass]->students[i] == 1 && classArray[smallestClass]->currentStudents < roomArray[classArray[smallestClass]->room]->capacity) { //If student wants to take this class and its open.
	  classArray[g]->currentStudents++;
          for (int j = 0; j < studentArray[i]->prefClassCount; j++) { //Then the student can't take any other classes during this timeslot.
	    if (classArray[studentArray[i]->prefClass[j]]->sectionId == classArray[smallestClass]->sectionId && studentArray[i]->prefClass[j] != smallestClass) {
              classArray[studentArray[i]->prefClass[j]]->students[i] = 0; //For extension 5. Makes sure students don't take two sections of the same class.
	    }
            if (classArray[studentArray[i]->prefClass[j]]->timeslot == smallestTimeslot && smallestClass != studentArray[i]->prefClass[j]) {
              classArray[studentArray[i]->prefClass[j]]->students[i] = 0;
 	    } else if (timeslotTimesGiven == 1 && smallestClass != studentArray[i]->prefClass[j]) { //Students also can't take classes which overlap time-wise.
              if (roomConflictGiven == 1) {
	        if (timeslotAdjacencies[smallestTimeslot][classArray[studentArray[i]->prefClass[j]]->timeslot] == 1) {
                  classArray[studentArray[i]->prefClass[j]]->students[i] = 0;
	        }
	      }
	    }
          }
        } else {
          classArray[smallestClass]->students[i] = 0;
	}
      }
    }
  }

  //Output
  FILE *fptrOut;
  fptrOut = fopen("output_schedule.txt", "w");
  if (fptrOut == NULL) {
    printf("Error opening file.\n");
    return 0;
  }
  char buffer[LINE_SIZE];
  buffer[0] = '\0';
  char intBuff[10];
  char idBuff[8];
  for (int i = 0; i < 10; i++) {
    intBuff[i] = '\0';
    if (i < 7) {
      idBuff[i] = '0';
    }
  }
  idBuff[7] = '\0';
  char weirdTab[2];
  weirdTab[0] = 9;
  weirdTab[1] = '\0'; //Converting that weird tab char to a string. Probably should've just
		      //done this with cpp strings...
  char lineFeed[2];
  lineFeed[0] = 10; //Converting line feed char to a string.
  lineFeed[1] = '\0';
  strncat(buffer, "Course", 7); //Building the string
  strncat(buffer, weirdTab, 2);
  strncat(buffer, "Room", 5);
  strncat(buffer, weirdTab, 2);
  strncat(buffer, "Teacher", 8);
  strncat(buffer, weirdTab, 2);
  strncat(buffer, "Time", 5);
  strncat(buffer, weirdTab, 2);
  strncat(buffer, "Students", 9);
  strncat(buffer, lineFeed, 2);
  fprintf(fptrOut, "%s", buffer); //Printing the first string to file.
  int tempIndex = 0;
  //int tempRoom = 0;
  char tempRoomName[256];
  for (int i = 0; i < 256; i++) {
    tempRoomName[i] = '\0';
  }
  char tempProfessor[10];
  for (int i = 0; i < 10; i++) {
    tempProfessor[i] = '\0';
  }
  int tempTimeslot = 0;
  for (int i = 0; i < classes; i++) {
    if (classArray[i]->inserted == 1) {
      buffer[0] = '\0'; //Reseting the buffer
      strncpy(tempProfessor, professorIds[i], 10);
      tempTimeslot = classArray[i]->timeslot;
      strncat(buffer, classArray[i]->id, 10);
      strncat(buffer, weirdTab, 2);
      snprintf(tempRoomName, 256, "%s", roomArray[classArray[i]->room]->name);
      strncat(buffer, tempRoomName, 256);
      strncat(buffer, weirdTab, 2);
      strncat(buffer, tempProfessor, 10);
      strncat(buffer, weirdTab, 2);
      snprintf(intBuff, 10, "%d", tempTimeslot);
      strncat(buffer, intBuff, 10);
      strncat(buffer, weirdTab, 2);
      int roomSizeCheck = 0;
      for (int j = 0; j < students; j++) {
        if (classArray[i]->students[j] == 1 && roomSizeCheck < roomArray[(classArray[i]->room)]->capacity) { //This is classes * students to output. Not sure how
					     //we feel about that.
          strncat(buffer, studentArray[j]->index, 10);
          strncat(buffer, " ", 2);
	  roomSizeCheck++;
	  studentPreferenceValue++;
        }
      }
      if (i != classes - 1) {
        strncat(buffer, lineFeed, 2);
      }
      fprintf(fptrOut, "%s", buffer);
    } else {
      unassignedCount++;
    }
  }


  gettimeofday(&stopB, NULL); //Clock
  secsB = (double)(stopB.tv_usec - startB.tv_usec) / 1000000 + (double)(stopB.tv_sec - startB.tv_sec);
  printf("Algorithm took %f seconds to run.\n", secsB);

  //Closing files
  fclose(fptrConstraints);
  fclose(fptrPreferences);
  fclose(fptrOut);

  //Freeing objects/object arrays
  for (int i = 0; i < classrooms; i++) {
    free(roomArray[i]);
  }
  free(roomArray);
  for (int i = 0; i < originalClasses; i++) {
    free(professorIds[i]);
    free(classArray[i]->id);
    free(classArray[i]->students);
    free(classArray[i]->conflictScores);
    free(classArray[i]->openRooms);
    free(classArray[i]);
  }
  free(professorIds);
  free(classArray);
  for (int i = 0; i < classes; i++) {
    free(G[i]);
  }
  free(G);
  for (int i = 0; i < students; i++) {
    free(studentArray[i]->prefClass);
    free(studentArray[i]->index);
    free(studentArray[i]);
  }
  free(studentArray);
 
  for (int i = 0; i < timeslots; i++) {
    free(timeslotAdjacencies[i]);
    free(timeslotTimeStarts[i]);
    free(timeslotTimeEnds[i]);
  }
  free(timeslotAdjacencies);
  free(timeslotTimeStarts);
  free(timeslotTimeEnds);

  for (std::set<struct aRoom>* i : roomListSlots) {
    delete i;
  }

  gettimeofday(&stopC, NULL); //Clock
  secsC = (double)(stopC.tv_usec - startC.tv_usec) / 1000000 + (double)(stopC.tv_sec - startC.tv_sec);
  printf("Program took %f seconds to run in total.\n", secsC);

  studentPreferenceBestCase = totalStudentExpectations;

  if (extensionFiveFlag == 1) {
    printf("Extension 5 - Number of extra classes added: %d.\n", extensionFiveExtraCount);
  }
  printf("PERCENTAGE OF CLASSES ASSIGNED %f.\n", ((float) (classes - unassignedCount)) / ((float) classes));
  printf("STUDENT PREFERENCE VALUE: %d.\n", studentPreferenceValue);
  printf("BEST VALUE: %d.\n", studentPreferenceBestCase);
  printf("Fit percentage: %f.\n", ((float) studentPreferenceValue)/((float)studentPreferenceBestCase));

  return 0;
}
