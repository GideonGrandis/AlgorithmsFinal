#include<iostream>
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

using namespace std;

//Variables and structs
int timeslots; //Number of timeslots
int classrooms; //Number of classrooms
int classes; //Number of classes
int professors; //Number of professors
int students; //Number of students

struct aRoom { //object representing a room
  int index; //Index of the room
  int capacity; //Capacity of the room

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
  int prefClass[4]; //Preferred classes
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
    printf("Usage: ./algorithm [Contraints].txt [Preferences].txt\n");
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

  char line[1024 * 50]; //File buffer
  line[0] = '\0';
  char tempLine[1024]; //Line buffer
  tempLine[0] = '\0';
  char delimiters[3];
  delimiters[0] = 9; delimiters[1] = 10; delimiters[2] = '\0'; //ASCII codes for weird tabs used in
							       //the file format.

  //Figured it was easier to just put the entire contents of the file into a seperate buffer
  //instead of reading it line by line.
  //There's probably a more elegant way to do this with c++ libraries, but I don't think it
  //greatly affects the time. Also this is just the setup.
  while (fgets(tempLine, 1024, fptrConstraints)) {
    strncat(line, tempLine, 1024); 
  }
  line[(1024 * 50) - 1] = '\0';
  //printf("{%s}\n", line);


  //Parsing constraints data
  //This whole section is sort of a mess to read, but there's really no other way
  //to read the data from such a specific format.
  char *token = strtok(line, delimiters); //"Class times"
  //printf("[%s]\n", token);
  token = strtok(NULL, delimiters); //timeslots
  timeslots = atoi(token);
  token = strtok(NULL, delimiters); //"Rooms"
  token = strtok(NULL, delimiters); //classrooms
  classrooms = atoi(token);
  roomArray = (struct aRoom**) malloc(classrooms * sizeof(struct aRoom*)); //Create a new
        								   //array of rooms
  for (int i = 0; i < classrooms; i++) { //For each room
    roomArray[i] = NULL;
    struct aRoom *roomPointer = (struct aRoom*) malloc(sizeof(struct aRoom)); //Create a new room
    roomArray[i] = roomPointer; //Add to the room array
    token = strtok(NULL, delimiters); //index
    roomArray[i]->index = atoi(token); //Assign the read index
    token = strtok(NULL, delimiters); //capacity
    roomArray[i]->capacity = atoi(token); //Assign the read capacity    
  }
  token = strtok(NULL, delimiters); //"Classes" 
  token = strtok(NULL, delimiters); //classes
  classes = atoi(token);
  classArray = (struct aClass**) malloc(classes * sizeof(struct aClass*)); //Create a new
                                                                           //array of classes
  token = strtok(NULL, delimiters); //"Teachers"
  token = strtok(NULL, delimiters); //professors
  professors = atoi(token);
  for (int i = 0; i < classes; i++) { //For each class
    struct aClass *classPointer = (struct aClass*) malloc(sizeof(struct aClass)); //Create a new class
    classArray[i] = classPointer; //Add to the class array
    token = strtok(NULL, delimiters); //index
    classArray[i]->index = atoi(token); //Assign the read index
    token = strtok(NULL, delimiters); //professor
    classArray[i]->professor = atoi(token); //Assign the read professor
    classArray[i]->room = -1;
    classArray[i]->timeslot = -1;
    classArray[i]->popularity = 0;
    classArray[i]->conflictScores = (int*) malloc(timeslots * sizeof(int));
    for (int j = 0; j < timeslots; j++) {
      classArray[i]->conflictScores[j] = 0;
    }
    classArray[i]->students = NULL;
  }

  //Setting up for parsing again.
  strcpy(line, "\0");
  strcpy(tempLine, "\0");
  while (fgets(tempLine, 1024, fptrPreferences)) {
    strncat(line, tempLine, 1024);
  }
  line[(1024 * 50) - 1] = '\0';
  char delimitersPref[3];
  delimitersPref[0] = 9; delimitersPref[1] = 10; delimitersPref[2] = ' '; 
  delimitersPref[3] = '\0'; //Parsings a little different here.

  //Parsing preferences data
  token = strtok(line, delimitersPref); //"Students"
  token = strtok(NULL, delimitersPref); //students
  students = atoi(token);
  studentArray = (struct aStudent**) malloc(students * sizeof(struct aStudent*)); //Create a new
                                                                           //array of students
  for (int i = 0; i < students; i++) { //For each student
    struct aStudent *studentPointer = (struct aStudent*) malloc(sizeof(struct aStudent)); //Create a new student
    studentArray[i] = studentPointer; //Add to the student array
    token = strtok(NULL, delimitersPref); //index
    //printf("index:[%s]\n", token);
    studentArray[i]->index = atoi(token); //Assign the read index
    for (int j = 0; j < 4; j++) {
      token = strtok(NULL, delimitersPref); //next prefered class
      //printf("(%s)\n", token);
      studentArray[i]->prefClass[j] = atoi(token);
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








  //THE ALGORITHM:

  //Initalizing the complete graph
  int** G = (int**)malloc(classes * sizeof(int*)); //Graph G
  for (int i = 0; i < classes; i++) {
    G[i] = (int*)malloc(classes * sizeof(int));
    for (int j = 0; j < classes; j++) {
      G[i][j] = 0; //Inefficent, but probably need to leave this here since valgrind gets weird about int
		   //initalization.
    }
  }

  //Building sorted list of room sizes.
  for (int i = 0; i < classrooms; i++) {
    roomList.insert(*roomArray[i]);
  }
  for (std::set<struct aRoom>::iterator i = roomList.begin(); i != roomList.end(); i++) {
    struct aRoom tempRoom = *i;
    printf("Got room %d of capacity %d.\n", tempRoom.index, tempRoom.capacity);
  }

  //Building sorted lists of room sizes.

  //Allocating space for ordered arrays

  for (int i = 0; i < timeslots; i++) {
    std::set<struct aRoom>* tempSet = new set<struct aRoom>();
    for (int j = 0; j < classrooms; j++) {
      tempSet->insert(*roomArray[j]);
    }
    printf("Inserting set.\n");
    roomListSlots.push_back(tempSet);
  }

  printf("Size of SlotsSet = %d.\n", roomListSlots.size());

  for (std::set<struct aRoom>* i : roomListSlots) {
    std::set<struct aRoom> tempSet = *i;
    for (std::set<struct aRoom>::iterator j = tempSet.begin(); j != tempSet.end(); j++) {
      struct aRoom tempRoom = *j;
      printf("Room: %d.\n", tempRoom.index);
    }
  }

  /*for (int i = 0; i < timeslots; i++) {
    roomListSlots[i] = 
    for (int j = 0; j < classrooms; i++) {
      roomListSlots[i].insert(*roomArray[j]);
      struct aRoom tempRoom = *roomListSlots[i].begin();
      printf("Room: %d.\n", tempRoom.index);
    }
  }*/

  /*for (int j = 0; j < timeslots; j++) {
    for (std::set<struct aRoom>::iterator i = roomListSlots[j].begin(); i != roomListSlots[j].end(); i++) {
      struct aRoom tempRoom = *i;
      printf("Timeslot: Got room %d of capacity %d.\n", tempRoom.index, tempRoom.capacity);
    }
  }*/

  //1. We begin by making a weighted complete graph G with vertex set C, where each edge
  //has weight equal to the number of people who want to take both of its endpoints. We
  //then set the weight of classes with the same professor to infinity. We also store the total
  //number of people who want to take each class.

  //Building G
  for (int i = 0; i < students; i++) {
    for (int j = 0; j < 4; j++) {
      for (int q = 0; q < 4; q++) {
        if (j != q) {
          if (classArray[studentArray[i]->prefClass[j] - 1]->professor !=
			  (classArray[studentArray[i]->prefClass[q] - 1]->professor)) {
            G[(studentArray[i]->prefClass[j]) - 1][(studentArray[i]->prefClass[q]) - 1]++;
          } else {
            G[(studentArray[i]->prefClass[j]) - 1][(studentArray[i]->prefClass[q]) - 1] = INT_MAX;
	  }
	}
      }
      classArray[studentArray[i]->prefClass[j] - 1]->popularity++; //increase popularity of all 4 classes.
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
  

  //3. We start by assigning the t largest classes C1, …, Ct to the largest room in the first time slot.
  for (int i = 0; i < timeslots; i++) { //What if there are more timeslots than rooms?
    struct aRoom tempRoom = *roomListSlots[0]->rbegin(); //Gets the last/largest room in the list.
    printf("Got room %d of size %d.\n", tempRoom.index, tempRoom.capacity);
    struct aClass tempClass = classHeap.top();
    tempClass.timeslot = 1;
    printf("Got class %d of popularity %d.\n", tempClass.index, tempClass.popularity);
    tempClass.room = tempRoom.index;
    roomListSlots[0]->erase(tempRoom);
    classHeap.pop();
  } //Hopefully I understand this right? It's putting the largest classes with the largest rooms
    //t times in the first timeslot, right?

  //Building roomList again.
  /*for (int i = 0; i < classrooms; i++) {
    roomList.insert(*roomArray[i]);
  }*/

  //4. For each i from 1 to t, we increase the i-th conflict score of each class C by the weight of
  //the edge between C and Ci.

  for (int i = 0; i < timeslots; i++) {
    for (int j = 0; j < classes; j++) {
      if (G[j][i] != INT_MAX) { //Just worried about int overflow here. Shame c ints dont have infinity.
        classArray[j]->conflictScores[i] = classArray[j]->conflictScores[i] + G[j][i];
      } else {
        classArray[j]->conflictScores[i] = INT_MAX;
      }
        //Hopefully I understand this right? Doesn't make a whole lot a sense to me
	//at the moment, but I think this is what the psuedocode says?
    }
  }

  //8. Repeat steps 5-7 until we’ve assigned every class to a room and time slot

  int timeslotsFilled = 0;
  int roomsFilled = 0;
  int classesAdded = 0;

  while (timeslotsFilled != timeslots && classesAdded != classes && roomsFilled != classrooms) {
    printf("Running while loop.\n");
    //5. Now, find the class C’ with the smallest minimum conflict score.
    int smallest = INT_MAX;
    int smallestClass = -1;
    int smallestTimeslot = -1;
    for (int i = 0; i < classes; i++) {
      printf("Running step 5.\n");
      for (int j = 0; j < timeslots; j++) {
        if (classArray[i]->conflictScores[j] < smallest) {
          smallest = classArray[i]->conflictScores[j];
          smallestClass = i;
    	  smallestTimeslot = j;
        }
      }
    }
    printf("Done with step 5.\n");

	  //6. Suppose this conflict score is for time slot j (smallestTimeslot). If C’ fits in a room,
	  //put it in the smallest available room it fits in, otherwise put it in the largest
	  //available room.

    printf("Size of roomList: %d.\n", roomList.size());

    printf("No. of classes: %d. Smallest class: %d, timeslot: %d.\n", classes, smallestClass, smallestTimeslot);
    classArray[smallestClass]->timeslot = smallestTimeslot + 1; //Assign class C' to the timeslot j.
    
    printf("Getting largest room in smallest timeslot");
    struct aRoom tempRoom = *--roomListSlots[smallestTimeslot]->end();

    //struct aRoom tempRoom = *--roomList.end();
 
    printf("Checking to start loop.\n");
    if (tempRoom.capacity >= classArray[smallestClass]->popularity) { //Fits
      for (std::set<struct aRoom>::iterator i = roomListSlots[smallestTimeslot]->begin(); i !=
	 roomListSlots[smallestTimeslot]->end(); i++) {
      //for (std::set<struct aRoom>::iterator i = roomList.begin(); i != roomList.end(); i++) {
        printf("Running for loop.\n");
        struct aRoom tempRoomTwo = *i;
        printf("Got room %d of capacity %d.\n", tempRoomTwo.index, tempRoomTwo.capacity);
        if (tempRoomTwo.capacity >= classArray[smallestClass]->popularity) { //Fits in smaller room.
          tempRoom = tempRoomTwo;
          printf("Ending early.\n");
          i = roomListSlots[smallestTimeslot]->end(); //End for loop early
          printf("This the issue?.\n");
        }
      }
    }

    printf("REMOVING %d, %d.\n", tempRoom.index, tempRoom.capacity);
    classArray[smallestClass]->room = tempRoom.index; //Assign the room to the class
    roomListSlots[smallestTimeslot]->erase(tempRoom); //Remove fitted room from the sorted array
    printf("Done with step 6.\n");

    roomsFilled++;

    classesAdded++;

    

    //7. Increase the j-th conflict score of each class C by the weight of the edge between C and
    //C’, unless C’ filled the last room, in which case set it to infinity.

    for (int i = 0; i < classes; i++) {
      printf("Running step 7.\n");
      if (roomListSlots[smallestTimeslot]->size() != 0) {
        classArray[i]->conflictScores[smallestTimeslot] = classArray[i]->conflictScores[smallestTimeslot]
	    + G[i][smallestClass];
      } else {
        classArray[i]->conflictScores[smallestTimeslot] = INT_MAX; //Set to infinity
        timeslotsFilled++;
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
  fprintf(fptrOut, buffer); //Printing the first string to file.
  int tempIndex = 0;
  int tempRoom = 0;
  int tempProfessor = 0;
  int tempTimeslot = 0;
  for (int i = 0; i < classes; i++) {
    buffer[0] = '\0'; //Reseting the buffer
    tempIndex = classArray[i]->index;
    tempRoom = classArray[i]->room;
    tempProfessor = classArray[i]->professor;
    tempTimeslot = classArray[i]->timeslot;    
    snprintf(intBuff, 10, "%d", tempIndex);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    snprintf(intBuff, 10, "%d", tempRoom);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    snprintf(intBuff, 10, "%d", tempProfessor);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    snprintf(intBuff, 10, "%d", tempTimeslot);
    strncat(buffer, intBuff, 10);
    strncat(buffer, weirdTab, 2);
    for (int j = 0; j < students; j++) {
      if (classArray[i]->students[j] == 1) { //This is classes * students to output. Not sure how
					     //we feel about that.
        snprintf(intBuff, 10, "%d", j);
        strncat(buffer, intBuff, 10);
        strncat(buffer, " ", 2);
      }
    }
    if (i != classes - 1) {
      strncat(buffer, lineFeed, 2);
    }
    fprintf(fptrOut, buffer);
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
    free(studentArray[i]);
  }
  free(studentArray);

  for (std::set<struct aRoom>* i : roomListSlots) {
    delete i;
  }

  gettimeofday(&stopC, NULL); //Clock
  secsC = (double)(stopC.tv_usec - startC.tv_usec) / 1000000 + (double)(stopC.tv_sec - startC.tv_sec);
  printf("Program took %f seconds to run in total.\n", secsC);

  return 0;
}
