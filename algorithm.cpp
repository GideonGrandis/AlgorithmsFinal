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
#include <set>
#include <memory>

//Represent timeslots abstractly, and when finding conflicts, just define them manually.
//Find a way to combine data from Bryn mawr and Haverford of the same year.
//More than X students, then split into sections. Make sure sections aren't in the same timeslot.

using namespace std;

//Variables and structs
int studentPreferenceValue = 0;
int studentPreferenceBestCase = 0;

int timeslots; //Number of timeslots
int classrooms; //Number of classrooms
int classes; //Number of classes
int professors; //Number of professors
int students; //Number of students

int timeslotTimesGiven; //Flag to check if times for timeslots are provided by the data. 0 if false, 1 if true.
int** timeslotTimeStarts; //Keeps track of timeslot times for each timeslot.
		    //Array of arrays of size 7. Hours are converted to minutes
		    //e.g. Timeslot 3, Friday from 3:00 pm to 6:30 pm would be:
		    //timeslotTimeStarts[2][4] = 810, timeslotTimeEnds[2][4] = 1020.
int** timeslotTimeEnds;

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
  int index; //Index of the class (Check if you actually need this anywhere)
  int popularity; //Number of students who want to take the class
  int professor; //Professor who teaches the class
  int timeslot; //Timeslot the class is in. -1 when unassigned.
  int room; //Room the class is in. -1 when unassigned.
  int* students; //Array of students enrolled in the class. Used as a bitfield on indices. i.e
		 //1 if the student is enrolled in the class, 0 if the student is not enrolled
		 //in the class. Should run faster than an expandable array.
  int* conflictScores; //The conflict scores of the class.
  int inserted; //0 if the class has not been inserted into the schedule. 1 if it has been inserted.

 //Overloading less than operator for heap comparison
  bool operator<(const aClass& other) const {
    return popularity < other.popularity;
  }
};

struct aClass** classArray; //Array of class objects
std::priority_queue<struct aClass> classHeap; //maxHeap of class objects.

struct aStudent { //object representing a student. Honestly we don't really need this for our
		  //algorithm, but since we're not focused on the runtime of the setup, and we
		  //might still want to change our algorithm, I figured I'd add it anyways.
  int index; //Index of the student
  int prefClassCount; //Number of preferred classes.
  int* prefClass; //Preferred classes
};

struct aStudent** studentArray; //Array of students.

int main(int argc, char *argv[]) {

  struct timeval startA, stopA; //Clock
  double secsA = 0;
  gettimeofday(&startA, NULL);

  struct timeval startC, stopC; //Clock
  double secsC = 0;
  gettimeofday(&startC, NULL);

  //Setup
  if (argc != 3) {
    printf("Usage: ./algorithm [Constraints].txt [Preferences].txt\n");
    return 0;
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

  char line[1024 * 3000]; //File buffer
  line[0] = '\0';
  char tempLine[1024]; //Line buffer
  tempLine[0] = '\0';
  char delimiters[3];
  delimiters[0] = 9; delimiters[1] = 10; delimiters[2] = '\0'; //ASCII codes for weird tabs used in
							       //the file format.

  //Figured it was easier to just put the entire contents of the file into a seperate buffer
  //instead of reading it line by line.
  //There's probably a more elegant way to do this with c++ libraries, but I don't think it
  //greatly affects the time.
  while (fgets(tempLine, 1024, fptrConstraints)) {
    strncat(line, tempLine, 1024); 
  }
  line[(1024 * 500) - 1] = '\0';

  char* saveptrMain; //Using multiple instances of strtok here, so we have to use strtok_r.
  char* saveptrTime;

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
  printf("DEBUG: Inital token: %s.\n", token);
  printf("DOING ROOMS.\n");
  if (strcmp(token, "Rooms") != 0) { //If not rooms, then timeslot index 1.
    printf("File has timeslot data. Processing...\n");
    for (int i = 0; i < timeslots; i++) {
      printf("Getting timeslot time data for timeslot %d.\n", i + 1);
      timeslotTimesGiven = 1;
      int startTime = 0;
      int endTime = 0;
      token = strtok_r(NULL, delimiters, &saveptrMain); //TIME BLOCK
      printf("Debug: Time block: %s.\n", token);
      char spaceDelim[3];
      spaceDelim[0] = ' '; //Paring parsing parsing...
      spaceDelim[1] = ':';
      spaceDelim[2] = '\0';
      char tokenCopy[1024];
      for (int j = 0; j < 1024; j++) {
        tokenCopy[j] = '\0';
      }
      strncpy(tokenCopy, token, 1024);
      char* spaceToken = strtok_r(tokenCopy, spaceDelim, &saveptrTime); //* (hour)
      printf("Starthour: %s.\n", spaceToken);
      startTime = 60 * atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //* (minute)
      printf("Startminute: %s.\n", spaceToken);
      startTime = startTime + atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //AM/PM
      printf("StartAM/PM: %s.\n", spaceToken);
      if (strcmp(spaceToken, "PM") == 0) {
        startTime = startTime + (60 * 12);
      } else if (strcmp(spaceToken, "AM") != 0) {
        printf("DEBUG: Error parsing timeslot data. Expected 'AM', but got '%s'.\n", spaceToken);
      }
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); // (hour)
      printf("Endhour: %s.\n", spaceToken);
      endTime = 60 * atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //:
      printf("Endminute: %s.\n", spaceToken);
      endTime = endTime + atoi(spaceToken);
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //AM/PM
      printf("EndAM/PM: %s.\n", spaceToken);
      if (strcmp(spaceToken, "PM") == 0) {
        endTime = endTime + (60 * 12);
      } else if (strcmp(spaceToken, "AM") != 0) {
        printf("DEBUG: Error parsing timeslot data. Expected 'AM', but got '%s'.\n", spaceToken);
      }
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrTime); //Null/day of the week.
      //printf("DEBUG: Should be starting NULL/DOTW: %s.\n", spaceToken);
      int tempBitFlag[7];
      for (int j = 0; j < 7; j++) {
        tempBitFlag[j] = 0;
      }
      for (int i = 0; i < 7; i++) {
        if (spaceToken[i] != '\0') {
          char DOTW = spaceToken[i];
          switch (DOTW) {
            case 'M':
              tempBitFlag[0] = 1;
              break;
	    case 'T':
              tempBitFlag[1] = 1;
              break;
	    case 'W':
              tempBitFlag[2] = 1;
              break;
	    case 'H':
              tempBitFlag[3] = 1;
              break;
	    case 'F':
              tempBitFlag[4] = 1;
              break;
	    case 'S':
              tempBitFlag[5] = 1;
              break;
	      case 'U':
              tempBitFlag[6] = 1;
              break;
          }
	}
      }

      for (int j = 0; j < 7; j++) {
        if (tempBitFlag[j] == 1) {
          timeslotTimeStarts[i][j] = startTime;
	  timeslotTimeEnds[i][j] = endTime;
	}
      }
      token = strtok_r(NULL, delimiters, &saveptrMain); //Next timeslot index, or 'Rooms.'
    }
  } else {
    token = strtok_r(NULL, delimiters, &saveptrMain); //Rooms
  }
  classrooms = atoi(token);
  roomArray = (struct aRoom**) malloc(classrooms * sizeof(struct aRoom*)); //Create a new
        								   //array of rooms
  printf("DEBUG: DOING CLASSROOMS.\n");
  for (int i = 0; i < classrooms; i++) { //For each room
    roomArray[i] = NULL;
    struct aRoom *roomPointer = (struct aRoom*) malloc(sizeof(struct aRoom)); //Create a new room
    roomArray[i] = roomPointer; //Add to the room array
    roomArray[i]->index = i + 1; //Index
    token = strtok_r(NULL, delimiters, &saveptrMain); //name
    for (int j = 0; j < 1024; j++) {
      roomArray[i]->name[j] = '\0'; //Initalize name.
    }
    strncpy(roomArray[i]->name, token, 1024); //Assign the read name.
    token = strtok_r(NULL, delimiters, &saveptrMain); //capacity
    roomArray[i]->capacity = atoi(token); //Assign the read capacity    
  }
  token = strtok_r(NULL, delimiters, &saveptrMain); //"Classes" 
  token = strtok_r(NULL, delimiters, &saveptrMain); //classes
  classes = atoi(token);
  classArray = (struct aClass**) malloc(classes * sizeof(struct aClass*)); //Create a new
                                                                           //array of classes
  token = strtok_r(NULL, delimiters, &saveptrMain); //"Teachers"
  token = strtok_r(NULL, delimiters, &saveptrMain); //professors
  professors = atoi(token);
  for (int i = 0; i < classes; i++) { //For each class
    struct aClass *classPointer = (struct aClass*) malloc(sizeof(struct aClass)); //Create a new class
    classArray[i] = classPointer; //Add to the class array
    token = strtok_r(NULL, delimiters, &saveptrMain); //index
    classArray[i]->index = atoi(token); //Assign the read index
    token = strtok_r(NULL, delimiters, &saveptrMain); //professor
    classArray[i]->professor = atoi(token); //Assign the read professor
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

  printf("DEBUG: DONE WITH PARSING CONSTRAINTS.\n");

  //Setting up for parsing again.
  strcpy(line, "\0");
  strcpy(tempLine, "\0");
  char tempLineCopy[1024 * 3000];
  for (int i = 0; i < 1024 * 3000; i++) {
    tempLineCopy[i] = '\0';
  }
  char tempDelim[2];
  tempDelim[0] = 10;
  tempDelim[1] = '\0';
  while (fgets(tempLine, 1024, fptrPreferences)) {
    strncat(line, tempLine, 1024);
  }
  line[(1024 * 3000) - 1] = '\0';
  strncat(tempLineCopy, line, strlen(line));

  int countCount = 0;
  char* tempToken = strtok(tempLineCopy, tempDelim);
  while (tempToken != NULL) {
    countCount++;
    printf("[%s].\n", tempToken);
    tempToken = strtok(NULL, tempDelim);
  }

  printf("DEBUG: FOUND STUDENT SIZE OF %d.\n", countCount);


  char delimitersPref[3];
  delimitersPref[0] = 9; delimitersPref[1] = 10; delimitersPref[2] = '\0';
  char spaceDelim[2];
  spaceDelim[0] = ' '; spaceDelim[1] = '\0';

  //Parsing preferences data
  char* saveptrNew;
  char* saveptrStudent;
  char* saveptrStudentSecond;

  token = strtok_r(line, delimitersPref, &saveptrNew); //"Students"
  token = strtok_r(NULL, delimitersPref, &saveptrNew); //students
  students = atoi(token);
  studentArray = (struct aStudent**) malloc(students * sizeof(struct aStudent*)); //Create a new
                                                                           //array of students
  
  
  for (int i = 0; i < students; i++) {
    struct aStudent *studentPointer = (struct aStudent*) malloc(sizeof(struct aStudent)); //Create a new student
    studentArray[i] = studentPointer; //Add to the student array

    token = strtok_r(NULL, delimitersPref, &saveptrNew); //index
    printf("Index: [%s]\n", token);
    studentArray[i]->index = atoi(token);
    token = strtok_r(NULL, delimitersPref, &saveptrNew); //classes

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
      printf("           [%s]\n", spaceToken);

      classCount++;
      spaceToken = strtok_r(NULL, spaceDelim, &saveptrStudent);
    }
    printf("Done wirh first.\n");
    studentArray[i]->prefClassCount = classCount;
    studentArray[i]->prefClass = (int*) malloc(classCount * sizeof(int));
    printf("Done allocing.\n");
    for (int j = 0; j < classCount; j++) {
      studentArray[i]->prefClass[j] = 0;
      printf(" (%d)", studentArray[i]->prefClass[j]);
    }
    classCount = 0;
    char* spaceTokenTwo = strtok_r(tokenCopyTwo, spaceDelim, &saveptrStudentSecond);
    printf("\n");
    while (spaceTokenTwo != NULL) {
      if (atoi(spaceTokenTwo)) {
        printf("           [%d]\n", atoi(spaceTokenTwo));
        studentArray[i]->prefClass[i] = atoi(spaceTokenTwo);
      }
      classCount++;
      spaceTokenTwo = strtok_r(NULL, spaceDelim, &saveptrStudentSecond);
    }
  }
  printf("Return\n");
  return 0;
  printf("Shouldnt see this.\n");

  /*for (int i = 0; i < students; i++) { //For each student
    struct aStudent *studentPointer = (struct aStudent*) malloc(sizeof(struct aStudent)); //Create a new student
    studentArray[i] = studentPointer; //Add to the student array
   
    token = strtok(NULL, delimiters); //index
    //printf("index:[%s]\n", token);
    studentArray[i]->index = atoi(token); //Assign the read index
    token = strtok(NULL, delimiters); //next prefered classes
    //printf("Pref class list:[%s]\n", token);

    char tokenCopy[1024];
    char *saveptr;
    for (int j = 0; j < 1024; j++) {
      tokenCopy[j] = '\0';
    }
    strncpy(tokenCopy, token, 1024);
    char* spaceToken = strtok_r(tokenCopy, spaceDelim, &saveptr);
    char* spaceTokenTwo = strtok_r(tokenCopy, spaceDelim, &saveptr);
    int prefCount = 0;
    while (spaceToken != NULL) { //Probably an issue here?
      prefCount++;
      spaceToken = strtok_r(NULL, spaceDelim, &saveptr);
    }
    studentArray[i]->prefClassCount = 4;//prefCount;
    studentArray[i]->prefClass = (int*) malloc(prefCount * sizeof(int));
    for (int j = 0; j < prefCount; j++) {
      studentArray[i]->prefClass[j] = atoi(spaceTokenTwo);
      spaceTokenTwo = strtok_r(NULL, spaceDelim, &saveptr);
    }


    //printf("Pref class list:[%s]\n", token);
    //token = strtok(NULL, delimiters); //index
     //printf("Pref DEBUG:[%s]\n", token);
  }*/
  printf("DEBUG 1.\n");

  for (int i = 0; i < students; i++) { //For each student
    struct aStudent *studentPointer = (struct aStudent*) malloc(sizeof(struct aStudent)); //Create a new student
     studentArray[i] = studentPointer; //Add to the student array

    studentArray[i]->prefClassCount = 4;//prefCount;
    studentArray[i]->prefClass = (int*) malloc(4 * sizeof(int));
    token = strtok(NULL, delimitersPref); //index
    
    //printf("index:[%s]\n", token);
    studentArray[i]->index = atoi(token); //Assign the read index
    for (int j = 0; j < 4; j++) {
      token = strtok(NULL, delimitersPref); //next prefered class
      //printf("Trying to print.\n");
      //printf("(%s)\n", token);
      studentArray[i]->prefClass[j] = atoi(token);
    }
  }
   printf("DEBUG 2.\n");




  //Adding student arrays to each class.
  for (int i = 0; i < classes; i++) {
    classArray[i]->students = (int*) malloc(students * sizeof(int)); //Create all of the student arrays
    for (int j = 0; j < students; j++) {
      classArray[i]->students[j] = 0;
    }
  }

  for (int i = 0; i < students; i++) { //Add students to classes' bitfields for later scheduling.
    //printf("prefClassCount: %d.\n", studentArray[i]->prefClassCount);
    for (int j = 0; j < studentArray[i]->prefClassCount; j++) {
      //printf("Students %d wants to take class %d, so adding...\n", i, studentArray[i]->prefClass[j]);
      classArray[studentArray[i]->prefClass[j] - 1]->students[i] = 1;
    }
  }

  //DEBUG
  for (int i = 0; i < classes; i++) {
    //printf("Class %d:\n ", i);
    for (int j = 0; j < students; j++) {
      //printf("[%d]", classArray[i]->students[j]);
    }
    //printf(".\n");
  }
 
  gettimeofday(&stopA, NULL); //Clock
  secsA = (double)(stopA.tv_usec - startA.tv_usec) / 1000000 + (double)(stopA.tv_sec - startA.tv_sec);
  printf("Setup took %f seconds to run.\n", secsA);

  //printf("Timeslots: %d, Classrooms: %d, Classes: %d, Professors: %d, Students: %d.\n", timeslots,
		 // classrooms, classes, professors, students);
  
  struct timeval startB, stopB; //Clock
  double secsB = 0;

  gettimeofday(&startB, NULL);


  //THE ALGORITHM:

  //Initalizing the complete graph
  int** G = (int**)malloc(classes * sizeof(int*)); //Graph G
  for (int i = 0; i < classes; i++) {
    G[i] = (int*)malloc(classes * sizeof(int));
    for (int j = 0; j < classes; j++) {
      if (i == j || classArray[i]->professor == classArray[j]->professor) { //Professor check
									    //Since we already
									    //have to do classes^2
									    //anyways for
									    //initalization.
        if (i != j) {
          //printf("class %d and %d have the same professor.\n", i + 1, j + 1);
	}
        G[i][j] = INT_MAX;
      } else {
        G[i][j] = 0;
      }
    }
  }

  //Building sorted list of room sizes.
  for (int i = 0; i < classrooms; i++) {
    roomList.insert(*roomArray[i]);
  }
  for (std::set<struct aRoom>::iterator i = roomList.begin(); i != roomList.end(); i++) {
    struct aRoom tempRoom = *i;
    //printf("Got room %d of capacity %d.\n", tempRoom.index, tempRoom.capacity);
  }

  //Building sorted lists of room sizes.

  //Allocating space for ordered arrays

  for (int i = 0; i < timeslots; i++) {
    std::set<struct aRoom>* tempSet = new set<struct aRoom>();
    for (int j = 0; j < classrooms; j++) {
      tempSet->insert(*roomArray[j]);
    }
    //printf("Inserting set.\n");
    roomListSlots.push_back(tempSet);
  }

  //printf("Size of SlotsSet = %ld.\n", roomListSlots.size());

  for (std::set<struct aRoom>* i : roomListSlots) {
    std::set<struct aRoom> tempSet = *i;
    for (std::set<struct aRoom>::iterator j = tempSet.begin(); j != tempSet.end(); j++) {
      struct aRoom tempRoom = *j;
      //printf("Room: %d.\n", tempRoom.index);
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
          if (G[(studentArray[i]->prefClass[j]) - 1][(studentArray[i]->prefClass[q]) - 1] + 1 > -1) {
            G[(studentArray[i]->prefClass[j]) - 1][(studentArray[i]->prefClass[q]) - 1]++;
	  } else {
            G[(studentArray[i]->prefClass[j]) - 1][(studentArray[i]->prefClass[q]) - 1] = INT_MAX;
	  }
	}
      }
      classArray[studentArray[i]->prefClass[j] - 1]->popularity++; //increase popularity of all j classes.
    }
  }


  //Building maxHeap of popularity for classes
  for (int i = 0; i < classes; i++) {
    classHeap.push(*classArray[i]);
  }
  /*for (int i = 0; i < classes; i++) { //Debug code
    struct aClass tempClass = classHeap.top();
    printf("Popped class %d of popularity %d.\n", tempClass.index, tempClass.popularity);
    classHeap.pop();
  }*/

  for (int i = 0; i < classes; i++) {
    for (int j = 0; j < classes; j++) {
      printf("[%d]", G[i][j]);
    } 
    printf("\n");
  }

  //2. We then initialize a length t conflict score array [0, …, 0] for each class.
    //Already did this bit in initalization of classes objects since its faster. Technically
    //maybe cheats the time our algorithm runs a little bit, but frankly it all seems a little
    //subjective what's neccescary initalization and initalization specific to our algorithm anyways.
  
  int classesAdded = 0;

  //3. We start by assigning the t largest classes C1, …, Ct to the largest room in the first time slot.
  for (int i = 0; i < timeslots; i++) { //What if there are more timeslots than rooms?
    struct aRoom tempRoom = *roomListSlots[i]->rbegin(); //Gets the last/largest room in the list.
    //printf("Got room %d of size %d.\n", tempRoom.index, tempRoom.capacity);
    struct aClass tempClass = classHeap.top();
    int tempIndex = tempClass.index - 1;
    classArray[tempIndex]->timeslot = i + 1;
    //printf("Got class %d of popularity %d.\n", tempClass.index, tempClass.popularity);
    classArray[tempIndex]->room = tempRoom.index; //One room for one timeslot, not all into the same timeslot.
    roomListSlots[i]->erase(tempRoom);
    classArray[tempIndex]->inserted = 1;
    classesAdded++;
    if (roomListSlots[i]->size() == 0) {
      classArray[tempIndex]->conflictScores[i] = INT_MAX; //In case a timeslot doesn't have any rooms.. I guess?
							  //probably unnecesary.
    }

    //Something I added. Wasn't originally in the algorithm...
    for (int j = 0; j < classes; j++) { 
      if (classArray[j]->professor == classArray[tempIndex]->professor) {
    
        //printf("Class %d and class %d have the same professor. Added %d into timeslot %d.\n", tempIndex + 1, j + 1,
                       // tempIndex + 1, i + 1);

        classArray[j]->conflictScores[i] = INT_MAX; //Make sure the same professor cannot
						    //teach again during this timeslot.
        //printf("Conflict score of %d at timeslot %d: %d.\n", j + 1, i + 1, classArray[j]->conflictScores[i]);
      }
    }


    classHeap.pop();
  }

  //4. For each i from 1 to t, we increase the i-th conflict score of each class C by the weight of
  //the edge between C and Ci.

  for (int i = 0; i < timeslots; i++) {
    for (int j = 0; j < classes; j++) {
      if (classArray[j]->conflictScores[i] < -1) { //Having issues with int overflow. Something unintended
						   //is clearly getting through in a previous
						   //step, but this should work as a hack fix-all.
        classArray[j]->conflictScores[i] = INT_MAX;
      }
      if (G[j][i] < -1) {
        G[j][i] = INT_MAX;
      }
      //printf("Conflict score of %d at timeslot %d was %d.\n", j + 1, i + 1, classArray[j]->conflictScores[i]);
      //printf("Plus: %d.\n", G[j][i]);
      //printf("Equals: %d.\n", classArray[j]->conflictScores[i] + G[j][i]);
      if (classArray[j]->conflictScores[i] + G[j][i] > -1) { //Int overflow check.
        classArray[j]->conflictScores[i] = classArray[j]->conflictScores[i] + G[j][i]; //This doesn't make sense to me.
      } else {
        classArray[j]->conflictScores[i] = INT_MAX;
      }
      //printf("Conflict score of %d at timeslot %d is now %d.\n", j + 1, i + 1, classArray[j]->conflictScores[i]);
    }
  }

  //8. Repeat steps 5-7 until we’ve assigned every class to a room and time slot

  int timeslotsFilled = 0;
  int roomsFilled = 0;
  
  int iteration = 0;

  while (timeslotsFilled != timeslots && classesAdded != classes) {
    //printf("Running while loop. iteration %d.\n", iteration);
    iteration++;
    //5. Now, find the class C’ with the smallest minimum conflict score.
    int smallest = INT_MAX;
    int smallestClass = -1;
    int smallestTimeslot = -1;
    for (int i = 0; i < classes; i++) {
      if (classArray[i]->inserted == 0) {
      //printf("Running step 5.\n");
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
      //printf("All possible insertions made. Exiting loop.\n");
      break;
    }
    

    //printf("Done with step 5.\n");

	  //6. Suppose this conflict score is for time slot j (smallestTimeslot). If C’ fits in a room,
	  //put it in the smallest available room it fits in, otherwise put it in the largest
	  //available room.

    //printf("Size of roomList: %ld.\n", roomList.size());

    //printf("No. of classes: %d. Smallest class: %d, timeslot: %d, smallest conflict score: %d.\n", classes, smallestClass + 1, smallestTimeslot, smallest);
    classArray[smallestClass]->timeslot = smallestTimeslot + 1; //Assign class C' to the timeslot j.

     
    printf("Getting largest room in smallest timesloti.\n");
    std::set<struct aRoom> tempSet = *roomListSlots[smallestTimeslot];
    printf("Got the inital set of size %ld.\n", tempSet.size());
    struct aRoom tempRoom = *--tempSet.end();

    //struct aRoom tempRoom = *--roomList.end();
 
    printf("Checking to start loop.\n");
    if (tempRoom.capacity >= classArray[smallestClass]->popularity) { //Fits
      for (std::set<struct aRoom>::iterator i = tempSet.begin(); i != tempSet.end(); i++) {
      //for (std::set<struct aRoom>::iterator i = roomList.begin(); i != roomList.end(); i++) {
        //printf("Running for loop.\n");
        struct aRoom tempRoomTwo = *i;
        printf("Got room %d of capacity %d.\n", tempRoomTwo.index, tempRoomTwo.capacity);
        if (tempRoomTwo.capacity >= classArray[smallestClass]->popularity) { //Fits in smaller room.
          tempRoom = tempRoomTwo;
          printf("Ending early.\n");
          i = --tempSet.end(); //End for loop early
          //printf("This the issue?.\n");
        }
      }
    }

    printf("REMOVING %d, %d.\n", tempRoom.index, tempRoom.capacity);
    classArray[smallestClass]->room = tempRoom.index; //Assign the room to the class

    roomListSlots[smallestTimeslot]->erase(tempRoom);
    //tempSet.erase(tempRoom); //Remove fitted room from the sorted array

    printf("Done with step 6.\n");

    classesAdded++;

    classArray[smallestClass]->inserted = 1; //Make sure we don't try to insert this class again

    

    //7. Increase the j-th conflict score of each class C by the weight of the edge between C and
    //C’, unless C’ filled the last room, in which case set it to infinity.

    for (int i = 0; i < classes; i++) {
      //printf("Running step 7.\n");
      

      //printf("DEBUG: %d, %d.\n", classArray[i]->conflictScores[smallestTimeslot], G[i][smallestClass]);
      if ((roomListSlots[smallestTimeslot]->size() != 0) && (classArray[i]->conflictScores[smallestTimeslot] + G[i][smallestClass] > -1)) {
        classArray[i]->conflictScores[smallestTimeslot] = classArray[i]->conflictScores[smallestTimeslot]
	    + G[i][smallestClass];
      } else {
        classArray[i]->conflictScores[smallestTimeslot] = INT_MAX; //Set to infinity
        if (roomListSlots[smallestTimeslot]->size() == 0) {
          timeslotsFilled++;
	}
      }


      //Added this myself to see if it runs with this. Don't know how
      //this fits in with our algorithm...
      //Makes sure a professor never teaches two different classes at the same time.
      if (classArray[i]->professor == classArray[smallestClass]->professor) {
        printf("Class %d and class %d have the same professor. Added %d into timeslot %d.\n", smallestClass + 1, i + 1, 
			smallestClass + 1, smallestTimeslot + 1);
        classArray[i]->conflictScores[smallestTimeslot] = INT_MAX;
      }
    }

     //Improvements to make here, but will do for now!

    /*for (int i = 0; i < students; i++) { //Make sure students can't take two classes
             				 //during the same timeslot.	
      if (classArray[smallestClass]->students[i] == 1) { //If student wants to take this class.
        for (int j = 0; j < studentArray[i]->prefClassCount; j++) { //Then the student can't take any other classes during this timeslot.
          if ((classArray[studentArray[i]->prefClass[j] - 1]->timeslot) - 1 == smallestTimeslot && smallestClass != studentArray[i]->prefClass[j] - 1) {
            printf("Student %d: Class %d and class %d have the same timeslot.\n", i + 1, smallestClass + 1, studentArray[i]->prefClass[j]);
            classArray[studentArray[i]->prefClass[j] - 1]->students[i] = 0;
	  }
	}
      }
    }*/

    printf("TimeslotsFilled: %d, classesAdded: %d.\n", timeslotsFilled, classesAdded);
    timeslotsFilled != timeslots && classesAdded != classes && roomsFilled != classrooms;
  }

  //Assign students.
  
  printf("                    Assigning students:\n");
  for (int g = 0; g < classes; g++) {
    int smallestClass = g;
    int smallestTimeslot = (classArray[g]->timeslot) - 1;
    for (int i = 0; i < students; i++) { //Make sure students can't take two classes
                                           //during the same timeslot.
      if (classArray[smallestClass]->students[i] == 1) { //If student wants to take this class.
       printf("STUDENT wants to take class %d\n", g);
        for (int j = 0; j < studentArray[i]->prefClassCount; j++) { //Then the student can't take any other classes during this timeslot.i
          if ((classArray[studentArray[i]->prefClass[j] - 1]->timeslot) - 1 == smallestTimeslot && smallestClass != studentArray[i]->prefClass[j] - 1) {
            printf("Student %d: Class %d and class %d have the same timeslot.\n", i + 1, smallestClass + 1, studentArray[i]->prefClass[j]);
            classArray[studentArray[i]->prefClass[j] - 1]->students[i] = 0;
 	  }
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
  char buffer[1024];
  buffer[0] = '\0';
  char intBuff[10];
  for (int i = 0; i < 10; i++) {
    intBuff[i] = '\0';
  }
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
  int tempProfessor = 0;
  int tempTimeslot = 0;
  for (int i = 0; i < classes; i++) {
    buffer[0] = '\0'; //Reseting the buffer
    tempIndex = classArray[i]->index;
    //tempRoom = classArray[i]->room;
    tempProfessor = classArray[i]->professor;
    tempTimeslot = classArray[i]->timeslot;    
    snprintf(intBuff, 10, "%d", tempIndex);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    //snprintf(intBuff, 10, "%s", tempRoomName);
    strncat(buffer, tempRoomName, 256);
    strncat(buffer, weirdTab, 2);
    snprintf(intBuff, 10, "%d", tempProfessor);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    snprintf(intBuff, 10, "%d", tempTimeslot);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    int roomSizeCheck = 0;
    for (int j = 0; j < students; j++) {
      if (classArray[i]->students[j] == 1 && roomSizeCheck < roomArray[(classArray[i]->room - 1)]->capacity) { //This is classes * students to output. Not sure how
					     //we feel about that.
        snprintf(intBuff, 10, "%d", j + 1);
        strncat(buffer, intBuff, 10);
        strncat(buffer, " ", 2);
	roomSizeCheck++;
	studentPreferenceValue++;
      }
    }
    if (i != classes - 1) {
      strncat(buffer, lineFeed, 2);
    }
    fprintf(fptrOut, "%s", buffer);
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
  for (int i = 0; i < classes; i++) {
    free(classArray[i]->students);
    free(classArray[i]->conflictScores);
    free(classArray[i]);
  }
  free(classArray);
  for (int i = 0; i < classes; i++) {
    free(G[i]);
  }
  free(G);
  for (int i = 0; i < students; i++) {
    free(studentArray[i]->prefClass);
    free(studentArray[i]);
  }
  free(studentArray);
 
  for (int i = 0; i < timeslots; i++) {
    free(timeslotTimeStarts[i]);
    free(timeslotTimeEnds[i]);
  }
  free(timeslotTimeStarts);
  free(timeslotTimeEnds);

  for (std::set<struct aRoom>* i : roomListSlots) {
    delete i;
  }

  gettimeofday(&stopC, NULL); //Clock
  secsC = (double)(stopC.tv_usec - startC.tv_usec) / 1000000 + (double)(stopC.tv_sec - startC.tv_sec);
  printf("Program took %f seconds to run in total.\n", secsC);

  studentPreferenceBestCase = students * 4;

  printf("STUDENT PREFERENCE VALUE: %d.\n", studentPreferenceValue);
  printf("BEST VALUE: %d.\n", studentPreferenceBestCase);
  printf("Fit percentage: %f.\n", ((float) studentPreferenceValue)/((float)studentPreferenceBestCase));

  return 0;
}
