Final project submission for CS340 by Will Bender, Gideon Grandis-McConnell, and Hongyi Zhang

Bryn mawr data .csv files provided as a part of the project is stored in the folder data/

Results from running our algorithm over both the provided and randomly generated data is stored in the folder results/
  --'newData_(Semester name).csv' are the results we got from running our algorithm over that data set. The
	first line is from running our algorithm without any extensions, and the lines after are trials
	using the extensions, listed by the 7 final parameters. If those paramters are set to NULL, then
	that extensions were not used for that trial.
  --'RandomlyGeneratedResults.csv' are the results we got from running our algorithm over randomly generated data,
	the same as the data used in the provided graph in the paper.

Three of the scripts provided as a part of the project are stored in the folder scripts/, both for run.sh to
	work properly, and for convenience.

Our implementation is in the file algorithm.cpp.

Run.sh works as described in the paper.

To run our algorithm directly, run the cpp executable 'algorithm' with the parameters listed below:
	./algorithm <constraints.txt> <student preferences.txt> <outputSchedule.txt> <options...>
		options:
	'-i' to ignore timeslot overlap
	'-d' to print data in csv format. (Note, this was used to help generate our data, does not generate a csv file,
					  only prints data to stdout in a .csv style format deliniated by spaces)
	'-e1' <percentage> applies our first extension, where <percentage> is the percent of classes which may
		be entered into any room. Every other classes may only enter the rooms listed for them in the .csv file.
	'-e2' <percentage> <limit> applies our third extension, where <percentage> is the percent of classes
		which must be entered into a timeslot with a total time across the week lower than <limit> 
		(in minutes)
	'-e3' <action> applies our second extension. If <action> is 0, it converts MWF timeslots to MW timeslots,
		and if <action> is 1, it converts MW timeslots to MWF timeslots.
	'-e4' <increase> applies our fourth extension, where <increase> is the value to increase the inital
		conflict score between two classes of the same subject by.
	'-e5' <limit> applies our fifth extension, where <limit> is the popularity a class must have before
		it is split into a new section.

	All of these options can be combined, though we have not extensively tested all combinations of
	extensions, so we can not guarantee that the program will return expected behavior under all
	conditions. We can confirm that most combinations of two extensions, and all single extensions
	work without error.
