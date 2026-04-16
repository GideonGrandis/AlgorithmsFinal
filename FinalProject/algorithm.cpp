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

#define LINE_SIZE 4096 //Size of lines when parsing.
#define LINE_NUMBER 1000

using namespace std;


//Variables and structs
int studentPreferenceValue = 0; //Number of classes student were able to take by the end of the algorithm.
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
int** timeslotTimeEnds; //Same as timeslotTimeStarts

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
std::multiset<struct aRoom> roomList; //Ordered array of room objects, from smallest capacity to largest.

struct std::vector<std::multiset<struct aRoom>*> roomListSlots; //array size t of ordered array of room objects,
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

int extensionOneFlag = 0; //Flag for extension one.
float oneFlagPercentage = 0; //Percent of classes which do not adhere to the restrictions of extension one.
int oneFlagCount = 0; //Number of classes affected by the restrictions set by extension one based on the given percentage.
int extensionTwoFlag = 0; //Flag for extension two.
float twoFlagPercentage = 0; //Percentage of classes which adhere to the restrictions set by extension two.
int twoFlagLimit = 0; //Classes adhering to the restrictions of extension two must go into a timeslot with a total time less than twoFlagLimit (in minutes).
int extensionThreeFlag = 0; //Flag for extension three.
int extensionThreeDirection = 0; //Converts MWF to MW timeslots for 0, and MW to MWF timeslots for 1.
int extensionFourFlag = 0; //Flag for extension four.
int extensionFourValue = 0; //Value to add to the adjacency matrix of G for classes with the same subject. Can be negative.
int extensionFiveFlag = 0; //Flag for extension two.
int extensionFiveLimit = 0; //How popular a class must be before it is split into a new section. Maximum number of sections for a single class is 10.
int extensionFiveClassCount = 0; //Number of spots for classes in classArray allocated under extension five. classes * 10. May be redundant, but helps with readability.
int extensionFiveExtraCount = 0; //Number of extra classes/sections created by extension five.
int ignoreOverlap = 0; //Ignores overlap between timeslots if provided in the data.
int dataModeFlag = 0; //Outputs data in format for csv files.

struct aStudent** studentArray; //Array of students.

std::unordered_map<char*, int> roomMap; //Maps room names with indices. Used to find extension one constraints quickly.
std::unordered_map<int, int> professorMap; //Maps professor id's to indices. Used to handle/parse
					   //the format of professor id's in the real world data.
char** professorIds; //Array of professor id's. Used exclusively to print output correctly.
int professorCountGlobal = 0; //Keeps track of the total number of professor id's retrived when searching through professorMap.

std::unordered_map<char*, int> classMap; //Maps class id's to indices. Used to find preferences quickly.


//Inserts class 'tempClass' into a room in 'timeslot.' Behavior changes depending on whether we are inserting
//the first t classes, or trying to insert all classes. 'begin' is 0 for inital insertions, and 1 for general insertions.
//Returns 1 if the function was succesfully able to insert the given class into the given timeslot, and 0 if it was not able to do so.
int insertClass(struct aClass* tempClass, int timeslot, int begin) {
  
    if (tempClass->inserted == 1) { //Checks if the class is already listed as inserted. Should never
				    //occur, but it doesn't hurt to keep the check for redundancy.
      return 0;
    }

    if (tempClass->conflictScores[timeslot] == INT_MAX) { //Makes sure the class is able to go into
							  //the given timeslot.
      return 0;
    }
   
    if (roomListSlots[timeslot]->size() == 0) { //Makes sure the timeslot still has available rooms for
						//the given class to go in.
      tempClass->conflictScores[timeslot] = INT_MAX;
      return 0;
    }

    //Getting a room for the class to go in.

    struct std::multiset<struct aRoom> tempRoomList = *roomListSlots[timeslot]; //Retrieve the list of rooms
										//for the timeslot.
    struct aRoom tempRoom; //Variable to store a found room.

    int endEarly = 0; //0 if a valid room has not been found, 1 if a valid room has. Ends the search loop
		      //early if set to 1.

    if (begin == 0) { //Inital insert for the first t classes.
      for (std::multiset<struct aRoom>::reverse_iterator i = tempRoomList.rbegin(); i != tempRoomList.rend() && endEarly != 1; ++i) {
        tempRoom = *i; //Save the room at this index of the set of rooms.
        if (extensionOneFlag == 1) { //For extension one. Makes sure that the room is valid for this class.
          if (tempClass->openRooms[tempRoom.index] == 1) {
	    endEarly = 1;
	  }
        } else {
  	  endEarly = 1;
        }
	if (timeslotTimesGiven == 1) { //If the data provided has timeslot information.
          for (int j = 0; j < classes; j++) { //Then for all classes.
            if (classArray[j]->room == tempRoom.index && classArray[j]->inserted == 1) { //If this class is in the room we are checking.
              if (timeslotAdjacencies[timeslot][classArray[j]->timeslot] == 1) { //If there is an overlap in the timeslots of the two classes.
                endEarly = 0; //Then this is not a valid room.
              }
            }
          }
        }
      }
    } else { //Regular insert
      int extensionOneCheck = 0; //Checks to see if this is a valid room for the given class for extension one.
      for (std::multiset<struct aRoom>::iterator i = tempRoomList.begin(); i != tempRoomList.end() && endEarly != 1; ++i) {
        tempRoom = *i;
        if (tempClass->popularity <= tempRoom.capacity) { //Class can fit in this room.
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

    //Remove the found room from roomListSlots. A little messy, but we can't seem to find a
    //better method for removing objects by index from c++ multisets.     
    int indexTracker = 0;
    for (std::multiset<struct aRoom>::iterator j = roomListSlots[timeslot]->begin(); j != roomListSlots[timeslot]->end(); j++) {
      struct aRoom iterateRoom = *j; //The room we get from the for loop.
      if (iterateRoom.index == tempRoom.index) { //If they have the same index.
         auto newPtr = next(roomListSlots[timeslot]->begin(), indexTracker); //Get a pointer to this room's object.
         roomListSlots[timeslot]->erase(newPtr); //Then remove based on the pointer. Avoids deleting
						 //another room with the same capacity.
	 break; //Exits the for loop early. Feels unsafe, but I don't think changing the pointer woud be much better.
      }
      indexTracker++;
    }
    

    tempClass->inserted = 1; //Sets the class to inserted.
    tempClass->room = tempRoom.index; //Assign the class to the found room.
    tempClass->timeslot = timeslot; //Assign the class to the provided timeslot.

    //Technically a part of step 7. Go through all classes and add any conflicts based on
    //professors and timeslot overlaps.
    for (int i = 0; i < classes; i++) {
      if (classArray[i]->professor == tempClass->professor) {
        classArray[i]->conflictScores[timeslot] = INT_MAX; //If they have the same professor.
	if (timeslotTimesGiven == 1) {
          for (int j = 0; j < timeslots; j++) {
            if (timeslotAdjacencies[j][timeslot] == 1) { //If their timeslots overlap.
              classArray[i]->conflictScores[j] = INT_MAX;
	    }
          }
	}
      }
      if (extensionOneFlag == 1) { //For extension one. Keeps track of open rooms for each classs.
        if (classArray[i]->openRooms[tempRoom.index] == 1) { //This is an available room for course i as well.
          classArray[i]->openRooms[tempRoom.index] == 0;
	  classArray[i]->openRoomCount--;
	  if (classArray[i]->openRoomCount == 1) { //Gives priority of a room to a class if that room is the only room
						   //it has left available. Not great in terms of time complexity,
						   //and probably uneccesary, but it seemed to help in terms of getting all classes assigned.
						   //for some of the more extreme cases under multiple extensions, so I figured I'd leave it in.
						   //Number of timeslots is usually so low compared to the number of classes, and it can only
						   //trigger once for each class, so I don't believe this affects the actual time analysis of the code.
            for (int j = 0; j < timeslots; j++) {
              if (classArray[i]->conflictScores[j] != INT_MAX) {
                classArray[i]->conflictScores[j] = -1; //Give priority if its the only available room for the class.
	      }
	    }
	  } else if (classArray[i]->openRoomCount == 0) {
            for (int j = 0; j < timeslots; j++) {
              classArray[i]->conflictScores[j] = INT_MAX; //If despite the check, this class just wasn't able to be assigned to the
							  //last room it could have gone in, then make sure it's ignored by the rest
							  //of the algorithm.
	    }
	  }
	}
      }
    } 
    return 1; //Class inserted succesfully
}

//The main body of the algorithm. Parses data as well.
int main(int argc, char *argv[]) { //Given the constraints and student preferences file.

  struct timeval startA, stopA; //Clock
  double secsA = 0;
  gettimeofday(&startA, NULL);

  struct timeval startC, stopC; //Clock
  double secsC = 0;
  gettimeofday(&startC, NULL);

  //Setup and parsing parameters.
  if (argc < 3) {
    printf("Usage: ./algorithm [Constraints].txt [Preferences].txt [Outputfile].txt\n");
    return 0;
  }

  if (argc > 4) {
    for (int i = 4; i < argc; i++) {
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
      } else if (strcmp(argv[i], "-d") == 0) {
        dataModeFlag = 1; //Used for parsing many runs of the program at once into a .csv file.
      } else if (strcmp(argv[i], "-i") == 0) {
	ignoreOverlap = 1;
      } else {
        printf("Tag not recognized.\n Tags are:\n   -e1 (percentage of classes with room restrictions) for extension 1.\n   -e2 (percentage of classes with limit) (limit in minutes) for extension 2.\n   -e3 (flag, 0 for MWF to MW, 1 for MW to MWF) for extension 3.\n   -e4 (value to increase conflict scores of classes with the same subject by) for extension 4.\n   -e5 (max number of student which can want to take a class before splitting into sections) for extension 5.\n -i to ignore overlap.\n-d for output in .csv file format.\n");
        return 0;
      }
    }
  }

  FILE *fptrConstraints;
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
  //There's probably a more elegant way to do this with c++ libraries, but I think it would
  //improve memory more than time.
  while (fgets(tempLine, LINE_SIZE, fptrConstraints)) {
    tempLine[LINE_SIZE - 1] = '\0';
    strncat(line, tempLine, LINE_SIZE);
  }

  char *saveptrMain; //Using multiple instances of strtok here, so we have to use strtok_r.
  char *saveptrTime;

  char delimiters[3]; //Delimiters for parsing.
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
  for (int i = 0; i < timeslots; i++) { //Setting up timeslot information.
    timeslotTimeStarts[i] = (int*) malloc(7 * sizeof(int));
    timeslotTimeEnds[i] = (int*) malloc(7 * sizeof(int));
    for (int j = 0; j < 7; j++) {
      timeslotTimeStarts[i][j] = -1; //Should initalize, even if it goes unused.
      timeslotTimeEnds[i][j] = -1; //Timeslot times are left at -1 if that data is not provided,
				   //such as in the randomly generated data sets.
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
        //printf("DEBUG: Error parsing timeslot data. Expected 'AM', but got '%s'.\n", spaceToken);
      } //Should only occur if something really goes wrong with parsing. So it's fine to leave this here.
      twelveCheck = 0; //We should really swap what we call 12:00 PM with what we call 12:00 AM...
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
        //printf("DEBUG: Error parsing timeslot data. Expected 'AM', but got '%s'.\n", spaceToken);
      }
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //Null/day of the week.
      int tempBitFlag[7];
      for (int j = 0; j < 7; j++) {
        tempBitFlag[j] = 0; //We keep the days of the week the classes are
			    //held on in a 7-byte bitfield. Technically
			    //inefficent, but a real 7-bit bitfield feels
			    //overkill.
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
	      if (i != 6) { //Thursday is represented as two characters, for some reason.
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
              tempBitFlag[5] = 1; //Not sure these are used? I
				  //suppose I must have seen at least one
				  //example if I added them. May be
				  //redundant.
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
	endTime = endTime - 30; //Add friday and remove 30 minutes.
      }
      for (int j = 0; j < 7; j++) {
        if (tempBitFlag[j] == 1) { //Add the minutes of the timeslot for each day it's held on.
          timeslotTimeStarts[i][j] = startTime;
	  timeslotTimeEnds[i][j] = endTime;
	}
      }
      token = strtok_r(NULL, delimiters, &saveptrMain); //Next timeslot index, or 'Rooms.'
    }
  }

  //Initalize timeslot adjacencies, even if it goes unused.
  timeslotAdjacencies = (int**) malloc(sizeof(int*) * timeslots);
  for (int i = 0; i < timeslots; i++) {
    timeslotAdjacencies[i] = (int*) malloc(sizeof(int) * timeslots);
    for (int j = 0; j < timeslots; j++) {
      timeslotAdjacencies[i][j] = 0;
    }
  }

  //1 if there is a conflict. 0 if there is not a conflict.
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

  //Continuing parsing.
  token = strtok_r(NULL, delimiters, &saveptrMain); //Rooms
  classrooms = atoi(token);
  roomArray = (struct aRoom**) malloc(classrooms * sizeof(struct aRoom*)); //Create a new
        								   //array of rooms
  for (int i = 0; i < classrooms; i++) { //For each room
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
  if (extensionFiveFlag == 1) { //Allocate extra space for sections if we need to.
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
  oneFlagCount = (int) (oneFlagPercentage * classes); //Get the percentage of classes
						      //not affected by extension one.
  if (oneFlagCount > classes) {
    oneFlagCount = classes;
  } else if (oneFlagCount < 0) {
    oneFlagCount = 0;
  }

  for (int i = 0; i < classes; i++) { //For each class
    struct aClass *classPointer = (struct aClass*) malloc(sizeof(struct aClass)); //Create a new class
    classArray[i] = classPointer; //Add to the class array
    classArray[i]->index = i; //Set the index of the class.
    classArray[i]->currentStudents = 0; //Keep track of the current number of students in the class.
    if (i >= originalClasses) { //If this is a potential section. Leave it initalized, but largely empty.
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
    } else { //If this is a regular class, then add all the parsed data to it.
      classArray[i]->section = -1; //-1 means that its a normal class.
      classArray[i]->sectionId = i; //Used to keep track of which sections are of which original classes.
      token = strtok_r(NULL, tenOnlyDelim, &saveptrMain);
      tenToken = strtok_r(token, delimiters, &tenptr); //id
      classArray[i]->id = (char*) malloc(sizeof(char) * 10);
      for (int j = 0; j < 10; j++) {
        classArray[i]->id[j] = '\0';
      }
      strncpy(classArray[i]->id, tenToken, 10);
      classMap.insert({classArray[i]->id, i});
      tenToken = strtok_r(NULL, delimiters, &tenptr); //professor id
      professorIds[i] = (char*) malloc(sizeof(char) * 10);
      for (int j = 0; j < 10; j++) {
        professorIds[i][j] = '\0';
      }
      if (professorMap.find(atoi(tenToken)) != professorMap.end()) {
        classArray[i]->professor = professorMap.at(atoi(tenToken)); //Get and store the
								    //professor based on
								    //the professor id.
      } else {
        professorMap[atoi(tenToken)] = professorCountGlobal;
        classArray[i]->professor = professorMap.at(atoi(tenToken));
        professorCountGlobal++;
      }
      strncpy(professorIds[i], tenToken, 10);
      professorIds[i][9] = '\0';
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
      if (tenToken == NULL || ignoreOverlap == 1) { //roomdata not given, or we're just ignoring it.
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
      while (tenToken != NULL) { //Assign rooms classes can be put in to those classes.
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

  //Setting up for parsing the student preferences data.
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

  for (int i = 0; i < students; i++) { //For each student.
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
    char tokenCopyTwo[2048]; //Helps with parsing classes based on id's.
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
    if (extensionFiveFlag == 1) { //Add room for sections if needed.
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
      int check = 0;
      for (const auto &p : classMap) {
	if (strcmp(spaceTokenTwo, p.first) == 0) {
          studentArray[i]->prefClass[classCount] = classMap.at(p.first); //Dealing with id's to index.
	  classCount++;
	  check = 1;
        }
      }
      if (check == 0) {
	studentArray[i]->prefClass[classCount] = -1;
      }
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
  if (dataModeFlag == 0) { //Regular output.
    //printf("Setup took %f seconds to run.\n", secsA);

    //printf("Timeslots: %d, Classrooms: %d, Classes: %d, Professors: %d, Students: %d.\n", timeslots,
  	//	 classrooms, classes, professors, students);
  } else { //Data mode output.
    //printf("%f ", secsA);
    printf("%d %d %d %d %d ", timeslots, classrooms, classes, professors, students);
  }
  
  struct timeval startB, stopB; //Clock
  double secsB = 0;
  gettimeofday(&startB, NULL);

  if (roomConflictGiven == 0 && extensionOneFlag == 1) {
    extensionOneFlag = 0;
    if (dataModeFlag == 0) {
      printf("WARNING: Data was not provided for room conflicts. Extension one will not apply to the output.\n");
    }
  }

  //THE ALGORITHM:

  if (extensionFiveFlag == 1) {
    for (int i = 0; i < students; i++) { //Calculate popularity earlier than normal of extension 5 is
					 //flagged. Probably could just be done here by default, but I'm
					 //afraid of breaking something this late in development.
      for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
        if (studentArray[i]->prefClass[j] != -1) {
          classArray[studentArray[i]->prefClass[j]]->popularity++; //increase popularity of all j classes.
        }
      }
    }
    for (int i = 0; i < originalClasses; i++) { //For every original class, make the needed sections for it.
      int sectionCount = classArray[i]->popularity/extensionFiveLimit;
      if (sectionCount < 0) {
        sectionCount = 0;
      }
      if (sectionCount > 9) {
        sectionCount = 9;
      }
      extensionFiveClassCount;
      for (int j = 0; j < sectionCount; j++) { //For every section of this class we need to create.
        extensionFiveExtraCount++; //Copy over all the data we need from the original class.
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
    for (int i = 0; i < students; i++) { //Add the sections to student preference arrays.
      int tempIterator = (studentArray[i]->prefClassCount - 1)/10;
      int iterator = tempIterator;
      for (int j = originalClasses; j < classes; j++) {
        if (classArray[j]->section > -1) { //For every section.
          for (int k = 0; k < tempIterator; k++) { //Check too see if it matches anything for the student.
            if (studentArray[i]->prefClass[k] != -1) {
              if (classArray[studentArray[i]->prefClass[k]]->sectionId == classArray[j]->sectionId) { //Room is a secition of a preferred class.
                studentArray[i]->prefClass[iterator] = j; //So add the section.
                iterator++;
	      }
	    }
	  }
	}
      }
      studentArray[i]->prefClassCount = iterator;
    }
    originalClasses = classes; //Replace 'classes' with how many classes are actually in use.
			       //Save original classes for later when we free data.
    classes = extensionFiveClassCount;
  }
  
  //Add students to classes' bitfields for later scheduling.
  for (int i = 0; i < students; i++) {
    for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
      if (studentArray[i]->prefClass[j] != -1) {
        classArray[studentArray[i]->prefClass[j]]->students[i] = 1;
      }
    }
  }
  
  //Initalizing the complete graph G.
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

  //Building sorted lists of room sizes.
  for (int i = 0; i < classrooms; i++) {
    roomList.insert(*roomArray[i]);
  }
  //Needs to be a multiset since rooms may have the same capacity as another room.
  for (std::multiset<struct aRoom>::iterator i = roomList.begin(); i != roomList.end(); i++) {
    struct aRoom tempRoom = *i;
  }

  //Allocating space for ordered arrays
  for (int i = 0; i < timeslots; i++) {
    std::multiset<struct aRoom>* tempSet = new multiset<struct aRoom>();
    for (int j = 0; j < classrooms; j++) {
      tempSet->insert(*roomArray[j]);
    }
    roomListSlots.push_back(tempSet);
  }

  //1. We begin by making a weighted complete graph G with vertex set C, where each edge
  //has weight equal to the number of people who want to take both of its endpoints. We
  //then set the weight of classes with the same professor to infinity. We also store the total
  //number of people who want to take each class.

  //Building G
  for (int i = 0; i < students; i++) {
    for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
      for (int q = 0; q < studentArray[i]->prefClassCount; q++) { //For each student, for each class they want to take
								  //, for each class they want to take.
        if (j != q && studentArray[i]->prefClass[j] != -1 && studentArray[i]->prefClass[q] != -1) {
          if (G[(studentArray[i]->prefClass[j])][(studentArray[i]->prefClass[q])] > -1) {
            G[(studentArray[i]->prefClass[j])][(studentArray[i]->prefClass[q])]++; //Increment
	  } else {
            G[(studentArray[i]->prefClass[j])][(studentArray[i]->prefClass[q])] = INT_MAX; //c doesn't have
											   //inf by default,
											   //so we have to perform
											   //this check to guard
											   //against overflow.
	  }
	}
      }
      if (extensionFiveFlag == 0 && studentArray[i]->prefClass[j] != -1) { //Already done if extension 5 is flagged.
        classArray[studentArray[i]->prefClass[j]]->popularity++; //increase popularity of all j classes.
      }
    }
  }

  if (extensionFourFlag == 1) { //Increase G[a][b] if a and b have the same subject by the provided value.
    for (int i = 0; i < classes; i++) {
      for (int j = 0; j < classes; j++) {
        if (strcmp(classArray[i]->subject, classArray[j]->subject) == 0) {
          if (G[i][j] != INT_MAX) {
            G[i][j] = G[i][j] + extensionFourValue;
	    if (G[i][j] < 0) {
              G[i][j] = 0;
	    }
	  }
	}
      }
    }
  }

  //Building maxHeap of popularity for classes
  for (int i = 0; i < classes; i++) {
    classHeap.push(*classArray[i]);
  }

  //2. We then initialize a length t conflict score array [0, …, 0] for each class.
    //Already did this bit in initalization of classes.
  
  //Keeps track of the number of classes added to the schedule.
  int classesAdded = 0;

  if (extensionOneFlag == 1) { //For each class, go through every room in every timeslot. If no availability, then timeslot = 0;
    for (int i = 0; i < classes; i++) {
      for (int j = 0; j < timeslots; j++) {
        int isEmpty = 0;

        std::multiset<struct aRoom> tempSet = *roomListSlots[j];
        for (std::multiset<struct aRoom>::iterator k = tempSet.begin(); k != tempSet.end(); k++) {
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

  if (extensionTwoFlag == 1) { //Make sure classes affected by extension two won't go
	  		       //into timeslots longer than their limit.
    if (timeslotTimesGiven == 0) {
      if (dataModeFlag == 0) {
        printf("Timeslot data not provided. Extension two cannot apply to this output.\n");
      }
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
    int infiniteCheck = classes; //Make sure we aren't stuck searching forever if there's less
				 //than t classes which can be inserted into the schedule.
    while (insertClass(tempClass, i, 0) == 0 && infiniteCheck != 0) {
      classHeap.pop();
      tempTempClass = classHeap.top();
      tempClass = classArray[tempTempClass.index];
      infiniteCheck--;
    }
    classHeap.pop();

    //Step 4. Increasing conflict scores.
    if (infiniteCheck != 0) {
      for (int j = 0;  j < classes; j++) {
        if (classArray[j]->conflictScores[i] < -1) {
          classArray[j]->conflictScores[i] = INT_MAX;
	}
	if (G[tempClass->index][j] < -1) {
          G[tempClass->index][j] = INT_MAX;
	}
        if (classArray[j]->conflictScores[i] + G[tempClass->index][j] > -1) {
          classArray[j]->conflictScores[i] = classArray[j]->conflictScores[i]
		  + G[tempClass->index][j];
	} else {
          classArray[j]->conflictScores[i] = INT_MAX;
	}
        if (roomConflictGiven == 1) { //Increase conflict scores based on if
				      //the timeslot they went int overlap or not.
          for (int k = 0; k < classes; k++) {
            if (classArray[k]->inserted == 1) {
              if (timeslotAdjacencies[i][classArray[k]->timeslot] == 1) { //If this 
									  //class overlaps with the one that was just inserted.
                if (G[k][j] < -1) {
                  G[k][j] = INT_MAX;
		}
		if (classArray[j]->conflictScores[i] + G[k][j] > -1) {
                  classArray[j]->conflictScores[i] = classArray[j]->conflictScores[i] + G[k][j];
		  //Then add the conflict between that class the original iterated one as well.
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

  int timeslotsFilled = 0; //Values so that we'll step when all timeslots/rooms have been
			   //filled.
  int roomsFilled = 0;
  
  //Find the best class, and the timeslot it should go in.
  while (timeslotsFilled != timeslots && classesAdded != classes) {
    int smallest = INT_MAX;
    int smallestClass = -1; //The class we will insert.
    int smallestTimeslot = -1; //The timeslot we will insert the class in.
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
    if (smallestClass == -1) { //If there's no classes left which
			       //can be inserted, then exit early.
      break;
    }

    //7. Increase the j-th conflict score of each class C by the weight of the edge between C and
    //C’, unless C’ filled the last room, in which case set it to infinity.
    if (insertClass(classArray[smallestClass], smallestTimeslot, 1) == 1) { //We insert here, in insertClass().
      classesAdded++;
      if (roomListSlots[smallestTimeslot]->size() == 0) {
        timeslotsFilled++; //If it filled the last room in that timeslot.
      }
      for (int i = 0; i < classes; i++) {
        if ((roomListSlots[smallestTimeslot]->size() != 0) && (classArray[i]->conflictScores[smallestTimeslot]
			       	+ G[i][smallestClass] > -1)) {
          classArray[i]->conflictScores[smallestTimeslot] = classArray[i]->conflictScores[smallestTimeslot]
	      + G[i][smallestClass];
        } else {
          classArray[i]->conflictScores[smallestTimeslot] = INT_MAX; //Set to infinity
        }

        //For all classes. If a class overlaps with the class that was just inserted, then add that conflict with 
	//the original iterating class as well.
        if (roomConflictGiven == 1) { //Since this section of the code is based on
				      //inserts and not classes, and every class overlapping would be equivalent
				      //to having a single timeslot, I believe these nested for loops are no more than c^2.
          for (int j = 0; j < classes; j++) {
            if (classArray[j]->inserted == 1) {
              if (timeslotAdjacencies[smallestTimeslot][classArray[j]->timeslot] == 1) { //If this class
									// overlaps with the one that was just inserted.
                if (G[j][i] < -1) {
                  G[j][i] = INT_MAX;
                }
                if (classArray[i]->conflictScores[smallestTimeslot] + G[j][i] > -1) {
                  classArray[i]->conflictScores[smallestTimeslot] = classArray[i]->conflictScores[smallestTimeslot]
			  + G[j][i]; //Then add the conflict between that class the original iterated one as well.
                } else {
                  classArray[i]->conflictScores[smallestTimeslot] = INT_MAX;
                }
              }
            }
          }
        }
        if (classArray[i]->professor == classArray[smallestClass]->professor) {
          classArray[i]->conflictScores[smallestTimeslot] = INT_MAX; //On review this is technically redundant, but
								     //doesn't really hurt to keep I suppose.
								     //Just in case.
        }
      }
    }
  }

  //Assign students. We do this by just giving what the first student wants first, and applying
  //restrictions to other students as we go. If the scheduling is good, then we should get good results.
  for (int g = 0; g < classes; g++) {
    if (classArray[g]->inserted == 1) {
      int smallestClass = g;
      int smallestTimeslot = (classArray[g]->timeslot);
      for (int i = 0; i < students; i++) { //Make sure students can't take two classes
                                           //during the same timeslot.
        if (classArray[smallestClass]->students[i] == 1 && classArray[smallestClass]->currentStudents < 
			roomArray[classArray[smallestClass]->room]->capacity) { //If student wants to take this class and its open.
	  classArray[g]->currentStudents++;
          for (int j = 0; j < studentArray[i]->prefClassCount; j++) { //Then the student can't take any other classes during this timeslot.
            if (studentArray[i]->prefClass[j] != -1) {
	      if (classArray[studentArray[i]->prefClass[j]]->sectionId == classArray[smallestClass]->sectionId && 
			      studentArray[i]->prefClass[j] != smallestClass) {
                classArray[studentArray[i]->prefClass[j]]->students[i] = 0; //For extension 5. Makes sure students
									    //don't take two sections of the same class.
	      }
              if (classArray[studentArray[i]->prefClass[j]]->timeslot == smallestTimeslot && smallestClass != 
			      studentArray[i]->prefClass[j]) {
                classArray[studentArray[i]->prefClass[j]]->students[i] = 0;
 	      } else if (timeslotTimesGiven == 1 && smallestClass != studentArray[i]->prefClass[j]) {
		      //Students also can't take classes which overlap time-wise.
                if (roomConflictGiven == 1) {
	          if (timeslotAdjacencies[smallestTimeslot][classArray[studentArray[i]->prefClass[j]]->timeslot] == 1) {
                    classArray[studentArray[i]->prefClass[j]]->students[i] = 0;
	          }
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
  fptrOut = fopen(argv[3], "w");
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
        if (classArray[i]->students[j] == 1 && roomSizeCheck < roomArray[(classArray[i]->room)]->capacity) {
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
  if (dataModeFlag == 0) {
    //printf("Algorithm took %f seconds to run.\n", secsB);
  }

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

  for (std::multiset<struct aRoom>* i : roomListSlots) {
    delete i;
  }

  //Stdout output.
  gettimeofday(&stopC, NULL); //Clock
  secsC = (double)(stopC.tv_usec - startC.tv_usec) / 1000000 + (double)(stopC.tv_sec - startC.tv_sec);
  if (dataModeFlag == 0) {
    //printf("Program took %f seconds to run in total.\n", secsC);
  } else {
    printf("%f ", secsC);
  }

  if (dataModeFlag == 0) {
    if (extensionFiveFlag == 1) {
      //printf("Extension 5 - Number of extra classes added: %d.\n", extensionFiveExtraCount);
    }
    //printf("PERCENTAGE OF CLASSES ASSIGNED %f.\n", ((float) (classes - unassignedCount)) / ((float) classes));
    //printf("STUDENT PREFERENCE VALUE: %d.\n", studentPreferenceValue);
    //printf("BEST VALUE: %d.\n", totalStudentExpectations);
    //printf("Fit percentage: %f.\n", ((float) studentPreferenceValue)/((float)totalStudentExpectations));
    printf("Student Preference Value: %d\n", studentPreferenceValue);
    printf("Best Case Student Preference Value: %d\n", totalStudentExpectations);
    printf("Fit Percentage: %f%\n", ((float) studentPreferenceValue)/((float)totalStudentExpectations));
  } else {
    printf("%f ", ((float) (classes - unassignedCount)) / ((float) classes));
    printf("%d ", studentPreferenceValue);
    printf("%d ", totalStudentExpectations);
    printf("%f ", ((float) studentPreferenceValue)/((float)totalStudentExpectations));
    if (extensionOneFlag == 1) {
      printf("%f ", oneFlagPercentage);
    } else {
      printf("NULL ");
    }
    if (extensionTwoFlag == 1) {
      printf("%f %d ", twoFlagPercentage, twoFlagLimit);
    } else if (dataModeFlag == 1) {
      printf("NULL NULL ");
    }
    if (extensionThreeFlag == 1) {
      if (extensionThreeDirection == 1) {
        printf("MW->MWF ");
      } else {
        printf("MWF->MW ");
      }
    } else {
      printf("NULL ");
    }
    if (extensionFourFlag == 1) {
      printf("%d ", extensionFourValue);
    } else {
      printf("NULL ");
    }
    if (extensionFiveFlag == 1) {
      printf("%d ", extensionFiveLimit);
      printf("%d ", extensionFiveExtraCount);
    } else {
      printf("NULL NULL");
    }
  }
  return 0;
}
